/*
    FreeRTOS V9.0.0 - Copyright (C) 2016 Real Time Engineers Ltd.
    All rights reserved
*/

/* Standard includes. */
#include <stdlib.h>

/* Defining MPU_WRAPPERS_INCLUDED_FROM_API_FILE prevents task.h from redefining
all the API functions to use the MPU wrappers.  That should only be done when
task.h is included from an application file. */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

#if ( INCLUDE_xTimerPendFunctionCall == 1 ) && ( configUSE_TIMERS == 0 )
	#error configUSE_TIMERS must be set to 1 to make the xTimerPendFunctionCall() function available.
#endif

/* Lint e961 and e750 are suppressed as a MISRA exception justified because the
MPU ports require MPU_WRAPPERS_INCLUDED_FROM_API_FILE to be defined for the
header files above, but not in this file, in order to generate the correct
privileged Vs unprivileged linkage and placement. */
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE /*lint !e961 !e750. */


/* This entire source file will be skipped if the application is not configured
to include software timer functionality.  This #if is closed at the very bottom
of this file.  If you want to include software timer functionality then ensure
configUSE_TIMERS is set to 1 in FreeRTOSConfig.h. */
#if ( configUSE_TIMERS == 1 )

/* Misc definitions. */
#define tmrNO_DELAY		( TickType_t ) 0U

/*软件定时器结构体定义 */
typedef struct tmrTimerControl
{
	const char				*pcTimerName;		/*<< 定时器名字 */
	ListItem_t				xTimerListItem;		/*<< Standard linked list item as used by all kernel features for event management. */
	TickType_t				xTimerPeriodInTicks;/*<< 定时器（周期）响应时间 */
	UBaseType_t				uxAutoReload;		/* 设置是否重装载，写入pdTRUE表示软件定时器为周期模式会自动重装载，写入pdFALSE表示软件定时器为单词模式 */
	void 					*pvTimerID;			/*<< 当多个软件定时器注册了同一个回调函数时，可以通过这个ID来判断是哪个定时器到期了 */
	TimerCallbackFunction_t	pxCallbackFunction;	/*软件定时器的回调函数，当软件定时器计时到达时会被调用 */
	#if( configUSE_TRACE_FACILITY == 1 )
		UBaseType_t			uxTimerNumber;		/*<< An ID assigned by trace tools such as FreeRTOS+Trace */
	#endif

	#if( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )
		uint8_t 			ucStaticallyAllocated; /*<< Set to pdTRUE if the timer was created statically so no attempt is made to free the memory again if the timer is later deleted. */
	#endif
} xTIMER;

/* The old xTIMER name is maintained above then typedefed to the new Timer_t
name below to enable the use of older kernel aware debuggers. */
typedef xTIMER Timer_t;

/* The definition of messages that can be sent and received on the timer queue.
Two types of message can be queued - messages that manipulate a software timer,
and messages that request the execution of a non-timer related callback.  The
two message types are defined in two separate structures, xTimerParametersType
and xCallbackParametersType respectively. */
typedef struct tmrTimerParameters
{
	TickType_t			xMessageValue;		/*<< An optional value used by a subset of commands, for example, when changing the period of a timer. */
	Timer_t *			pxTimer;			/*<< 命令的目标定时器. */
} TimerParameter_t;


typedef struct tmrCallbackParameters
{
	PendedFunction_t	pxCallbackFunction;	/* << 定时器待执行的回调函数 */
	void *pvParameter1;						/* << 回调函数的参数 */
	uint32_t ulParameter2;					/* << 回调函数的第二个参数 */
} CallbackParameters_t;

/* The structure that contains the two message types, along with an identifier
that is used to determine which message type is valid. */
typedef struct tmrTimerQueueMessage
{
	BaseType_t			xMessageID;			/*<< The command being sent to the timer service task. */
	union
	{
		TimerParameter_t xTimerParameters;

		/* Don't include xCallbackParameters if it is not going to be used as
		it makes the structure (and therefore the timer queue) larger. */
		#if ( INCLUDE_xTimerPendFunctionCall == 1 )
			CallbackParameters_t xCallbackParameters;
		#endif /* INCLUDE_xTimerPendFunctionCall */
	} u;
} DaemonTaskMessage_t;

/*lint -e956 A manual analysis and inspection has been used to determine which
static variables must be declared volatile. */

/*存储活动定时器的列表。定时器按到期时间顺序引用，最近的到期时间位于列表的最前面。只有定时器服务任务才允许访问这些列表。 */
PRIVILEGED_DATA static List_t xActiveTimerList1;
PRIVILEGED_DATA static List_t xActiveTimerList2;
PRIVILEGED_DATA static List_t *pxCurrentTimerList;
PRIVILEGED_DATA static List_t *pxOverflowTimerList;

/* xTimerQueue是软件定时器的命令队列 */
PRIVILEGED_DATA static QueueHandle_t xTimerQueue = NULL;
PRIVILEGED_DATA static TaskHandle_t xTimerTaskHandle = NULL;

/*lint +e956 */

/*-----------------------------------------------------------*/

#if( configSUPPORT_STATIC_ALLOCATION == 1 )

	/* If static allocation is supported then the application must provide the
	following callback function - which enables the application to optionally
	provide the memory that will be used by the timer task as the task's stack
	and TCB. */
	extern void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize );

#endif

/*
 * Initialise the infrastructure used by the timer service task if it has not
 * been initialised already.
 */
static void prvCheckForValidListAndQueue( void ) PRIVILEGED_FUNCTION;

/*
 * The timer service task (daemon).  Timer functionality is controlled by this
 * task.  Other tasks communicate with the timer service task using the
 * xTimerQueue queue.
 */
static void prvTimerTask( void *pvParameters ) PRIVILEGED_FUNCTION;

/*
 * Called by the timer service task to interpret and process a command it
 * received on the timer queue.
 */
static void prvProcessReceivedCommands( void ) PRIVILEGED_FUNCTION;

/*
 * Insert the timer into either xActiveTimerList1, or xActiveTimerList2,
 * depending on if the expire time causes a timer counter overflow.
 */
static BaseType_t prvInsertTimerInActiveList( Timer_t * const pxTimer, const TickType_t xNextExpiryTime, const TickType_t xTimeNow, const TickType_t xCommandTime ) PRIVILEGED_FUNCTION;

/*
 * An active timer has reached its expire time.  Reload the timer if it is an
 * auto reload timer, then call its callback.
 */
static void prvProcessExpiredTimer( const TickType_t xNextExpireTime, const TickType_t xTimeNow ) PRIVILEGED_FUNCTION;

/*
 * The tick count has overflowed.  Switch the timer lists after ensuring the
 * current timer list does not still reference some timers.
 */
static void prvSwitchTimerLists( void ) PRIVILEGED_FUNCTION;

/*
 * Obtain the current tick count, setting *pxTimerListsWereSwitched to pdTRUE
 * if a tick count overflow occurred since prvSampleTimeNow() was last called.
 */
static TickType_t prvSampleTimeNow( BaseType_t * const pxTimerListsWereSwitched ) PRIVILEGED_FUNCTION;

/*
 * If the timer list contains any active timers then return the expire time of
 * the timer that will expire first and set *pxListWasEmpty to false.  If the
 * timer list does not contain any timers then return 0 and set *pxListWasEmpty
 * to pdTRUE.
 */
static TickType_t prvGetNextExpireTime( BaseType_t * const pxListWasEmpty ) PRIVILEGED_FUNCTION;

/*
 * If a timer has expired, process it.  Otherwise, block the timer service task
 * until either a timer does expire or a command is received.
 */
static void prvProcessTimerOrBlockTask( const TickType_t xNextExpireTime, BaseType_t xListWasEmpty ) PRIVILEGED_FUNCTION;

/*
 * Called after a Timer_t structure has been allocated either statically or
 * dynamically to fill in the structure's members.
 */
static void prvInitialiseNewTimer(	const char * const pcTimerName,
									const TickType_t xTimerPeriodInTicks,
									const UBaseType_t uxAutoReload,
									void * const pvTimerID,
									TimerCallbackFunction_t pxCallbackFunction,
									Timer_t *pxNewTimer ) PRIVILEGED_FUNCTION; /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
/*-----------------------------------------------------------*/

/* 软件定时器任务的创建函数，prvTimerTask为定时器任务对应的函数 */								
BaseType_t xTimerCreateTimerTask( void )
{
BaseType_t xReturn = pdFAIL;

	/* 检查定时器相关的链表和用来传递消息的命令队列是否已经初始化，未初始化则初始化 */
	prvCheckForValidListAndQueue();

	if( xTimerQueue != NULL )
	{
		#if( configSUPPORT_STATIC_ALLOCATION == 1 )
		{
			StaticTask_t *pxTimerTaskTCBBuffer = NULL;
			StackType_t *pxTimerTaskStackBuffer = NULL;
			uint32_t ulTimerTaskStackSize;

			vApplicationGetTimerTaskMemory( &pxTimerTaskTCBBuffer, &pxTimerTaskStackBuffer, &ulTimerTaskStackSize );
			xTimerTaskHandle = xTaskCreateStatic(	prvTimerTask,
													"Tmr Svc",
													ulTimerTaskStackSize,
													NULL,
													( ( UBaseType_t ) configTIMER_TASK_PRIORITY ) | portPRIVILEGE_BIT,
													pxTimerTaskStackBuffer,
													pxTimerTaskTCBBuffer );

			if( xTimerTaskHandle != NULL )
			{
				xReturn = pdPASS;
			}
		}
		#else
		{
			xReturn = xTaskCreate(	prvTimerTask,
									"Tmr Svc",
									configTIMER_TASK_STACK_DEPTH,
									NULL,
									( ( UBaseType_t ) configTIMER_TASK_PRIORITY ) | portPRIVILEGE_BIT,
									&xTimerTaskHandle );
		}
		#endif /* configSUPPORT_STATIC_ALLOCATION */
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

	configASSERT( xReturn );
	return xReturn;
}
/*-----------------------------------------------------------*/

/* 软件定时器动态创建函数，返回所创建的软件定时器的句柄 */
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
	TimerHandle_t xTimerCreate(	const char * const pcTimerName,
								const TickType_t xTimerPeriodInTicks,
								const UBaseType_t uxAutoReload,
								void * const pvTimerID,
								TimerCallbackFunction_t pxCallbackFunction ) 
	{
	Timer_t *pxNewTimer;

		pxNewTimer = ( Timer_t * ) pvPortMalloc( sizeof( Timer_t ) );

		if( pxNewTimer != NULL )
		{
			prvInitialiseNewTimer( pcTimerName, xTimerPeriodInTicks, uxAutoReload, pvTimerID, pxCallbackFunction, pxNewTimer );

			#if( configSUPPORT_STATIC_ALLOCATION == 1 )
			{
				/* 在定时器结构体中记录是动态创建的 */
				pxNewTimer->ucStaticallyAllocated = pdFALSE;
			}
			#endif /* configSUPPORT_STATIC_ALLOCATION */
		}

		return pxNewTimer;
	}

#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/
/* 软件定时器静态创建函数，返回所创建的软件定时器的句柄 */
#if( configSUPPORT_STATIC_ALLOCATION == 1 )
	TimerHandle_t xTimerCreateStatic(	const char * const pcTimerName,
										const TickType_t xTimerPeriodInTicks,
										const UBaseType_t uxAutoReload,
										void * const pvTimerID,
										TimerCallbackFunction_t pxCallbackFunction,
										StaticTimer_t *pxTimerBuffer ) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
	{
	Timer_t *pxNewTimer;

		#if( configASSERT_DEFINED == 1 )
		{
			/* Sanity check that the size of the structure used to declare a
			variable of type StaticTimer_t equals the size of the real timer
			structures. */
			volatile size_t xSize = sizeof( StaticTimer_t );
			configASSERT( xSize == sizeof( Timer_t ) );
		}
		#endif /* configASSERT_DEFINED */

		/* A pointer to a StaticTimer_t structure MUST be provided, use it. */
		configASSERT( pxTimerBuffer );
		pxNewTimer = ( Timer_t * ) pxTimerBuffer; /*lint !e740 Unusual cast is ok as the structures are designed to have the same alignment, and the size is checked by an assert. */

		if( pxNewTimer != NULL )
		{
			prvInitialiseNewTimer( pcTimerName, xTimerPeriodInTicks, uxAutoReload, pvTimerID, pxCallbackFunction, pxNewTimer );

			#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
			{
				/* 在定时器结构体中记录是静态创建的 */
				pxNewTimer->ucStaticallyAllocated = pdTRUE;
			}
			#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
		}

		return pxNewTimer;
	}

#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

/* 软件定时器初始化函数 */
static void prvInitialiseNewTimer(	const char * const pcTimerName,
									const TickType_t xTimerPeriodInTicks,
									const UBaseType_t uxAutoReload,
									void * const pvTimerID,
									TimerCallbackFunction_t pxCallbackFunction,
									Timer_t *pxNewTimer )
{
	/* 计时周期传入0是无效的 */
	configASSERT( ( xTimerPeriodInTicks > 0 ) );

	if( pxNewTimer != NULL )
	{
		/*确保定时器任务所使用的链表和命令队列已经初始化*/
		prvCheckForValidListAndQueue();

		/* 初始化定时器结构体成员 */
		pxNewTimer->pcTimerName = pcTimerName;
		pxNewTimer->xTimerPeriodInTicks = xTimerPeriodInTicks;
		pxNewTimer->uxAutoReload = uxAutoReload;
		pxNewTimer->pvTimerID = pvTimerID;
		pxNewTimer->pxCallbackFunction = pxCallbackFunction;
		vListInitialiseItem( &( pxNewTimer->xTimerListItem ) );
		traceTIMER_CREATE( pxNewTimer );
	}
}
/*-----------------------------------------------------------*/

/* 定时器命令发送函数，xTimer为定时器句柄，xCommandID代表发送的具体命令，这些命令会被添加到全局大的定时器命令队列xTimerQueue上 */
BaseType_t xTimerGenericCommand( TimerHandle_t xTimer, const BaseType_t xCommandID, const TickType_t xOptionalValue, BaseType_t * const pxHigherPriorityTaskWoken, const TickType_t xTicksToWait )
{
BaseType_t xReturn = pdFAIL;
DaemonTaskMessage_t xMessage;

	configASSERT( xTimer );

	/* 向计时器服务任务发送一条消息以在一个特定的定时器上执行特定的操作 */
	if( xTimerQueue != NULL )
	{
		xMessage.xMessageID = xCommandID;
		xMessage.u.xTimerParameters.xMessageValue = xOptionalValue;
		xMessage.u.xTimerParameters.pxTimer = ( Timer_t * ) xTimer;
	/* 如果 xCommandID < tmrFIRST_FROM_ISR_COMMAND则表明是在任务中发送的，否则说明是在中断中发送的 */
		if( xCommandID < tmrFIRST_FROM_ISR_COMMAND )
		{
			if( xTaskGetSchedulerState() == taskSCHEDULER_RUNNING )//系统已启动并且调度器在正常运行
			{
				xReturn = xQueueSendToBack( xTimerQueue, &xMessage, xTicksToWait );
			}
			else
			{//调度器当前是暂停的或者系统还未启动，只能以非阻塞方式发送
				xReturn = xQueueSendToBack( xTimerQueue, &xMessage, tmrNO_DELAY );
			}
		}
		else
		{
			xReturn = xQueueSendToBackFromISR( xTimerQueue, &xMessage, pxHigherPriorityTaskWoken );
		}
		traceTIMER_COMMAND_SEND( xTimer, xCommandID, xOptionalValue, xReturn );
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}
	return xReturn;
}
/*-----------------------------------------------------------*/
/*该函数用于返回定时器任务句柄*/
TaskHandle_t xTimerGetTimerDaemonTaskHandle( void )
{
	/* If xTimerGetTimerDaemonTaskHandle() is called before the scheduler has been
	started, then xTimerTaskHandle will be NULL. */
	configASSERT( ( xTimerTaskHandle != NULL ) );
	return xTimerTaskHandle;
}
/*-----------------------------------------------------------*/
/*该函数用于获取目标定时器的定时周期*/
TickType_t xTimerGetPeriod( TimerHandle_t xTimer )
{
Timer_t *pxTimer = ( Timer_t * ) xTimer;

	configASSERT( xTimer );
	return pxTimer->xTimerPeriodInTicks;
}
/*-----------------------------------------------------------*/
/*该函数用于获取定时器结构体中的xTimerListItem链表项上的值*/
TickType_t xTimerGetExpiryTime( TimerHandle_t xTimer )
{
Timer_t * pxTimer = ( Timer_t * ) xTimer;
TickType_t xReturn;

	configASSERT( xTimer );
	xReturn = listGET_LIST_ITEM_VALUE( &( pxTimer->xTimerListItem ) );
	return xReturn;
}
/*-----------------------------------------------------------*/
/*该函数用于获取目标定时器的名字*/
const char * pcTimerGetName( TimerHandle_t xTimer ) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
{
Timer_t *pxTimer = ( Timer_t * ) xTimer;

	configASSERT( xTimer );
	return pxTimer->pcTimerName;
}
/*-----------------------------------------------------------*/
/*该函数用于在当前时间大于定时器应该被响应时间时调用，传入的xNextExpireTime应该是小于等于xTimeNow的*/
static void prvProcessExpiredTimer( const TickType_t xNextExpireTime, const TickType_t xTimeNow )
{
BaseType_t xResult;
Timer_t * const pxTimer = ( Timer_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxCurrentTimerList );

	/* 从当前列表中移除定时器 */
	( void ) uxListRemove( &( pxTimer->xTimerListItem ) );
	traceTIMER_EXPIRED( pxTimer );

	/* 若定时器是周期定时器，则计算下一个到期时间并将定时器重新添加到对应的定时器列表中 */
	if( pxTimer->uxAutoReload == ( UBaseType_t ) pdTRUE )
	{
		if( prvInsertTimerInActiveList( pxTimer, ( xNextExpireTime + pxTimer->xTimerPeriodInTicks ), xTimeNow, xNextExpireTime ) != pdFALSE )
		{
			/* 返回值为pdTRUE则需要立即处理  */
			xResult = xTimerGenericCommand( pxTimer, tmrCOMMAND_START_DONT_TRACE, xNextExpireTime, NULL, tmrNO_DELAY );
			configASSERT( xResult );
			( void ) xResult;
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

	/* 执行回调函数 */
	pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );
}
/*-----------------------------------------------------------*/

/* 软件定时器任务对应的函数 */
static void prvTimerTask( void *pvParameters )
{
TickType_t xNextExpireTime;
BaseType_t xListWasEmpty;

	/* 用于防止编译警告 */
	( void ) pvParameters;

	#if( configUSE_DAEMON_TASK_STARTUP_HOOK == 1 )
	{
		extern void vApplicationDaemonTaskStartupHook( void );

		/* Allow the application writer to execute some code in the context of
		this task at the point the task starts executing.  This is useful if the
		application includes initialisation code that would benefit from
		executing after the scheduler has been started. */
		vApplicationDaemonTaskStartupHook();
	}
	#endif /* configUSE_DAEMON_TASK_STARTUP_HOOK */

	for( ;; )
	{
		/* 查询下一个到期的定时器时间值，若当前列表中没有定时器则返回0，xListWasEmpty指示当前定时器链表是否为空， xNextExpireTime返回的是下一个定时器到期时间*/
		xNextExpireTime = prvGetNextExpireTime( &xListWasEmpty );

		/* 如果下一个定时器已经到期，则处理它。否则，阻塞该任务，直到定时器到期或收到命令 */
		prvProcessTimerOrBlockTask( xNextExpireTime, xListWasEmpty );

		/* Empty the command queue. */
		prvProcessReceivedCommands();
	}
}
/*-----------------------------------------------------------*/
/*该函数用于根据实际情况将当前的定时器任务阻塞到下一个定时器时间来临*/
static void prvProcessTimerOrBlockTask( const TickType_t xNextExpireTime, BaseType_t xListWasEmpty )
{
TickType_t xTimeNow;
BaseType_t xTimerListsWereSwitched;

	vTaskSuspendAll();
	{
		/* 获取现在的时间，以评估定时器是否已经过期。如果获取时间导致了列表切换，那么不要处理这个定时器，因为在列表切换函数prvSwitchTimerLists中，
		被切换列表中的任何定时器都已经处理了 */
		xTimeNow = prvSampleTimeNow( &xTimerListsWereSwitched );
		if( xTimerListsWereSwitched == pdFALSE )
		{
			/* 计时器没溢出，未交换当前定时器列表时才会执行到这里 */
			if( ( xListWasEmpty == pdFALSE ) && ( xNextExpireTime <= xTimeNow ) )
			{
				( void ) xTaskResumeAll();
				prvProcessExpiredTimer( xNextExpireTime, xTimeNow );//xNextExpireTime <= xTimeNow
			}
			else
			{
				/* 计时器尚未溢出，且尚未达到下一个定时器到期时间。因此，该任务应该阻塞以等待下一个到期时间或接收到的命令（无论哪个先到）。
				除非xNextExpireTime > xTimeNow，否则无法执行以下行，除非当前定时器列表为空。若xListWasEmpty为 pdTRUE则xNextExpireTime为0，阻塞的时间也刚好为xNextExpireTime - xTimeNow*/
				if( xListWasEmpty != pdFALSE )
				{
					/* 当前定时器列表为空，判断溢出定时器列表是否为空 */
					xListWasEmpty = listLIST_IS_EMPTY( pxOverflowTimerList );
				}

				vQueueWaitForMessageRestricted( xTimerQueue, ( xNextExpireTime - xTimeNow ), xListWasEmpty );

				if( xTaskResumeAll() == pdFALSE )
				{
					/* Yield to wait for either a command to arrive, or the
					block time to expire.  If a command arrived between the
					critical section being exited and this yield then the yield
					will not cause the task to block. */
					portYIELD_WITHIN_API();
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
		}
		else
		{
			( void ) xTaskResumeAll();
		}
	}
}
/*-----------------------------------------------------------*/

/* 该函数用于获取当前定时器列表中下一个定时器到期时间*/
static TickType_t prvGetNextExpireTime( BaseType_t * const pxListWasEmpty )
{
TickType_t xNextExpireTime;

	/* 定时器按到期时间顺序排列，链表的表头指向最先到期的进程。获取到期时间最近的定时器的到期时间。如果没有活动定时器，
	则将下一个到期时间设置为0。这将导致该任务在时标计数溢出时解除阻塞，此时定时器列表将切换，并可以重新评估下一个到期时间。  */
	*pxListWasEmpty = listLIST_IS_EMPTY( pxCurrentTimerList );
	if( *pxListWasEmpty == pdFALSE )
	{
		xNextExpireTime = listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxCurrentTimerList );
	}
	else
	{
		xNextExpireTime = ( TickType_t ) 0U;
	}

	return xNextExpireTime;
}
/*-----------------------------------------------------------*/
/*该函数判断当前系统tick计数是否溢出，并按照实际情况选择是否切换定时器计数链表*/
static TickType_t prvSampleTimeNow( BaseType_t * const pxTimerListsWereSwitched )
{
TickType_t xTimeNow;
PRIVILEGED_DATA static TickType_t xLastTime = ( TickType_t ) 0U;

	xTimeNow = xTaskGetTickCount();
	/* 当前时间比上一次记录的时间小，说明溢出了，则切换当前计时器列表，注意xLastTime是static类型的 */
	if( xTimeNow < xLastTime )
	{
		prvSwitchTimerLists();
		*pxTimerListsWereSwitched = pdTRUE;
	}
	else
	{
		*pxTimerListsWereSwitched = pdFALSE;
	}

	xLastTime = xTimeNow;

	return xTimeNow;
}
/*-----------------------------------------------------------*/
/*该函数用于判断下一次定时器响应时间是否到达并做出相应的处理，xCommandTime是上一次定时器应该响应时间，xNextExpiryTime是下一次定时器应该响应时间，xTimeNow是当前时间，如果不存在溢出的话xCommandTime应该是小于xTimeNow的，这个函数能处理掉计数值溢出的问题，在prvProcessReceivedCommands中被调用实现定时器正常响应，很关键*/
static BaseType_t prvInsertTimerInActiveList( Timer_t * const pxTimer, const TickType_t xNextExpiryTime, const TickType_t xTimeNow, const TickType_t xCommandTime )
{
BaseType_t xProcessTimerNow = pdFALSE;

	listSET_LIST_ITEM_VALUE( &( pxTimer->xTimerListItem ), xNextExpiryTime );
	listSET_LIST_ITEM_OWNER( &( pxTimer->xTimerListItem ), pxTimer );

	if( xNextExpiryTime <= xTimeNow )//这种情况要么下一次到期时间是溢出了，要么是xNextExpiryTime确实比xTimeNow小
	{
		/* 判断是否已经到期 */
		if( ( ( TickType_t ) ( xTimeNow - xCommandTime ) ) >= pxTimer->xTimerPeriodInTicks )
		{
			/* xNextExpiryTime和xTimeNow都未溢出，或都溢出，已经到期，需要立即执行回调函数  */
			xProcessTimerNow = pdTRUE;
		}
		else
		{
			/* xNextExpiryTime溢出而xTimeNow未溢出，未到期，插入溢出定时器队列  */
			vListInsert( pxOverflowTimerList, &( pxTimer->xTimerListItem ) );
		}
	}
	else
	{
		if( ( xTimeNow < xCommandTime ) && ( xNextExpiryTime >= xCommandTime ) )
		{
			/*xTimeNow溢出，xNextExpiryTime没溢出，已经到期，定时器一定已经超过了到期时间，应该立即处理。 */
			xProcessTimerNow = pdTRUE;
		}
		else
		{//都没溢出，未到期
			vListInsert( pxCurrentTimerList, &( pxTimer->xTimerListItem ) );
		}
	}

	return xProcessTimerNow;
}
/*-----------------------------------------------------------*/
/*该函数用于接收定时器命令，并根据情况响应执行回调函数*/
static void	prvProcessReceivedCommands( void )
{
DaemonTaskMessage_t xMessage;
Timer_t *pxTimer;
BaseType_t xTimerListsWereSwitched, xResult;
TickType_t xTimeNow;

	while( xQueueReceive( xTimerQueue, &xMessage, tmrNO_DELAY ) != pdFAIL )
	{
		#if ( INCLUDE_xTimerPendFunctionCall == 1 )
		{
			/* Negative commands are pended function calls rather than timer
			commands. */
			if( xMessage.xMessageID < ( BaseType_t ) 0 )
			{
				const CallbackParameters_t * const pxCallback = &( xMessage.u.xCallbackParameters );

				/* The timer uses the xCallbackParameters member to request a
				callback be executed.  Check the callback is not NULL. */
				configASSERT( pxCallback );

				/* Call the function. */
				pxCallback->pxCallbackFunction( pxCallback->pvParameter1, pxCallback->ulParameter2 );
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		#endif /* INCLUDE_xTimerPendFunctionCall */

		/* 值大于等于0命令是定时器命令而不是挂起的函数调用。 */
		if( xMessage.xMessageID >= ( BaseType_t ) 0 )
		{
			/* 获取目标软件定时器 */
			pxTimer = xMessage.u.xTimerParameters.pxTimer;

			if( listIS_CONTAINED_WITHIN( NULL, &( pxTimer->xTimerListItem ) ) == pdFALSE )
			{
				/* 定时器在某个链表中，将其从链表中移除 */
				( void ) uxListRemove( &( pxTimer->xTimerListItem ) );
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}

			traceTIMER_COMMAND_RECEIVED( pxTimer, xMessage.xMessageID, xMessage.u.xTimerParameters.xMessageValue );

			/* In this case the xTimerListsWereSwitched parameter is not used, but
			it must be present in the function call.  prvSampleTimeNow() must be
			called after the message is received from xTimerQueue so there is no
			possibility of a higher priority task adding a message to the message
			queue with a time that is ahead of the timer daemon task (because it
			pre-empted the timer daemon task after the xTimeNow value was set). */
			xTimeNow = prvSampleTimeNow( &xTimerListsWereSwitched );

			switch( xMessage.xMessageID )
			{
				case tmrCOMMAND_START :
			    case tmrCOMMAND_START_FROM_ISR :
			    case tmrCOMMAND_RESET :
			    case tmrCOMMAND_RESET_FROM_ISR :
				case tmrCOMMAND_START_DONT_TRACE :
					/* 启动或重启一个定时器 */
					if( prvInsertTimerInActiveList( pxTimer,  xMessage.u.xTimerParameters.xMessageValue + pxTimer->xTimerPeriodInTicks, xTimeNow, xMessage.u.xTimerParameters.xMessageValue ) != pdFALSE )
					{
						/* The timer expired before it was added to the active
						timer list.  Process it now. */
						pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );
						traceTIMER_EXPIRED( pxTimer );

						if( pxTimer->uxAutoReload == ( UBaseType_t ) pdTRUE )
						{
							xResult = xTimerGenericCommand( pxTimer, tmrCOMMAND_START_DONT_TRACE, xMessage.u.xTimerParameters.xMessageValue + pxTimer->xTimerPeriodInTicks, NULL, tmrNO_DELAY );
							configASSERT( xResult );
							( void ) xResult;
						}
						else
						{
							mtCOVERAGE_TEST_MARKER();
						}
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
					break;

				case tmrCOMMAND_STOP :
				case tmrCOMMAND_STOP_FROM_ISR :
					/* The timer has already been removed from the active list.
					There is nothing to do here. */
					break;

				case tmrCOMMAND_CHANGE_PERIOD :
				case tmrCOMMAND_CHANGE_PERIOD_FROM_ISR :
					pxTimer->xTimerPeriodInTicks = xMessage.u.xTimerParameters.xMessageValue;
					configASSERT( ( pxTimer->xTimerPeriodInTicks > 0 ) );

					/* The new period does not really have a reference, and can
					be longer or shorter than the old one.  The command time is
					therefore set to the current time, and as the period cannot
					be zero the next expiry time can only be in the future,
					meaning (unlike for the xTimerStart() case above) there is
					no fail case that needs to be handled here. */
					( void ) prvInsertTimerInActiveList( pxTimer, ( xTimeNow + pxTimer->xTimerPeriodInTicks ), xTimeNow, xTimeNow );
					break;

				case tmrCOMMAND_DELETE :
					/* The timer has already been removed from the active list,
					just free up the memory if the memory was dynamically
					allocated. */
					#if( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 0 ) )
					{
						/* The timer can only have been allocated dynamically -
						free it again. */
						vPortFree( pxTimer );
					}
					#elif( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )
					{
						/* The timer could have been allocated statically or
						dynamically, so check before attempting to free the
						memory. */
						if( pxTimer->ucStaticallyAllocated == ( uint8_t ) pdFALSE )
						{
							vPortFree( pxTimer );
						}
						else
						{
							mtCOVERAGE_TEST_MARKER();
						}
					}
					#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
					break;

				default	:
					/* Don't expect to get here. */
					break;
			}
		}
	}
}
/*-----------------------------------------------------------*/

/* 该函数用来切换当前定时器列表 */
static void prvSwitchTimerLists( void )
{
TickType_t xNextExpireTime, xReloadTime;
List_t *pxTemp;
Timer_t *pxTimer;
BaseType_t xResult;

	/* 溢出导致切换列表，如果当前定时器链表中还有引用的定时器，那么它们一定已经过期，应该在链表切换之前处理 */
	while( listLIST_IS_EMPTY( pxCurrentTimerList ) == pdFALSE )
	{
		xNextExpireTime = listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxCurrentTimerList );

		/*从列表中移出定时器 */
		pxTimer = ( Timer_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxCurrentTimerList );
		( void ) uxListRemove( &( pxTimer->xTimerListItem ) );
		traceTIMER_EXPIRED( pxTimer );

		/*执行回调函数 */
		pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );

		if( pxTimer->uxAutoReload == ( UBaseType_t ) pdTRUE )
		{
			/* 计算reload值，如果reload值导致定时器进入相同的定时器列表，那么它已经过期，应该将定时器重新插入到当前列表，
			以便在此循环中再次处理它。否则，应该发送命令重启定时器，确保只有在链表交换之后，才将定时器插入到链表中。 */
			xReloadTime = ( xNextExpireTime + pxTimer->xTimerPeriodInTicks );
			if( xReloadTime > xNextExpireTime )
			{
				listSET_LIST_ITEM_VALUE( &( pxTimer->xTimerListItem ), xReloadTime );
				listSET_LIST_ITEM_OWNER( &( pxTimer->xTimerListItem ), pxTimer );
				vListInsert( pxCurrentTimerList, &( pxTimer->xTimerListItem ) );
			}
			else
			{//通过定时器命令发送函数将其添加到定时器队列中，这里传入tmrNO_DELAY说明必须立刻加入队列成功，否则configASSERT( xResult )会断言失败
				xResult = xTimerGenericCommand( pxTimer, tmrCOMMAND_START_DONT_TRACE, xNextExpireTime, NULL, tmrNO_DELAY );
				configASSERT( xResult );
				( void ) xResult;
			}
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}

	pxTemp = pxCurrentTimerList;
	pxCurrentTimerList = pxOverflowTimerList;
	pxOverflowTimerList = pxTemp;
}
/*-----------------------------------------------------------*/

/* 该函数用来检查定时器相关的链表和用来传递消息的命令队列是否已经初始化，若未初始化则进行初始化 */
static void prvCheckForValidListAndQueue( void )
{
	taskENTER_CRITICAL();
	{
		if( xTimerQueue == NULL )
		{
			vListInitialise( &xActiveTimerList1 );
			vListInitialise( &xActiveTimerList2 );
			pxCurrentTimerList = &xActiveTimerList1;
			pxOverflowTimerList = &xActiveTimerList2;

			#if( configSUPPORT_STATIC_ALLOCATION == 1 )
			{
				/* 进行静态分配 */
				static StaticQueue_t xStaticTimerQueue;
				static uint8_t ucStaticTimerQueueStorage[ configTIMER_QUEUE_LENGTH * sizeof( DaemonTaskMessage_t ) ];

				xTimerQueue = xQueueCreateStatic( ( UBaseType_t ) configTIMER_QUEUE_LENGTH, sizeof( DaemonTaskMessage_t ), &( ucStaticTimerQueueStorage[ 0 ] ), &xStaticTimerQueue );
			}
			#else
			{
				xTimerQueue = xQueueCreate( ( UBaseType_t ) configTIMER_QUEUE_LENGTH, sizeof( DaemonTaskMessage_t ) );
			}
			#endif

			#if ( configQUEUE_REGISTRY_SIZE > 0 )
			{
				if( xTimerQueue != NULL )
				{
					vQueueAddToRegistry( xTimerQueue, "TmrQ" );
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			#endif /* configQUEUE_REGISTRY_SIZE */
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
	taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/
/*该函数用于判断目标定时器是否在使用*/
BaseType_t xTimerIsTimerActive( TimerHandle_t xTimer )
{
BaseType_t xTimerIsInActiveList;
Timer_t *pxTimer = ( Timer_t * ) xTimer;

	configASSERT( xTimer );

	/* Is the timer in the list of active timers? */
	taskENTER_CRITICAL();
	{
		/* Checking to see if it is in the NULL list in effect checks to see if
		it is referenced from either the current or the overflow timer lists in
		one go, but the logic has to be reversed, hence the '!'. */
		xTimerIsInActiveList = ( BaseType_t ) !( listIS_CONTAINED_WITHIN( NULL, &( pxTimer->xTimerListItem ) ) );
	}
	taskEXIT_CRITICAL();

	return xTimerIsInActiveList;
} /*lint !e818 Can't be pointer to const due to the typedef. */
/*-----------------------------------------------------------*/
/*该函数用于获取目标定时器ID*/
void *pvTimerGetTimerID( const TimerHandle_t xTimer )
{
Timer_t * const pxTimer = ( Timer_t * ) xTimer;
void *pvReturn;

	configASSERT( xTimer );

	taskENTER_CRITICAL();
	{
		pvReturn = pxTimer->pvTimerID;
	}
	taskEXIT_CRITICAL();

	return pvReturn;
}
/*-----------------------------------------------------------*/
/*该函数用于设置目标定时器ID*/
void vTimerSetTimerID( TimerHandle_t xTimer, void *pvNewID )
{
Timer_t * const pxTimer = ( Timer_t * ) xTimer;

	configASSERT( xTimer );

	taskENTER_CRITICAL();
	{
		pxTimer->pvTimerID = pvNewID;
	}
	taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/
/*该函数用于在中断中设置执行回调函数的软件定时器*/
#if( INCLUDE_xTimerPendFunctionCall == 1 )
	BaseType_t xTimerPendFunctionCallFromISR( PendedFunction_t xFunctionToPend, void *pvParameter1, uint32_t ulParameter2, BaseType_t *pxHigherPriorityTaskWoken )
	{
	DaemonTaskMessage_t xMessage;
	BaseType_t xReturn;

		/* Complete the message with the function parameters and post it to the
		daemon task. */
		xMessage.xMessageID = tmrCOMMAND_EXECUTE_CALLBACK_FROM_ISR;
		xMessage.u.xCallbackParameters.pxCallbackFunction = xFunctionToPend;
		xMessage.u.xCallbackParameters.pvParameter1 = pvParameter1;
		xMessage.u.xCallbackParameters.ulParameter2 = ulParameter2;

		xReturn = xQueueSendFromISR( xTimerQueue, &xMessage, pxHigherPriorityTaskWoken );

		tracePEND_FUNC_CALL_FROM_ISR( xFunctionToPend, pvParameter1, ulParameter2, xReturn );

		return xReturn;
	}

#endif /* INCLUDE_xTimerPendFunctionCall */
/*-----------------------------------------------------------*/

#if( INCLUDE_xTimerPendFunctionCall == 1 )

	BaseType_t xTimerPendFunctionCall( PendedFunction_t xFunctionToPend, void *pvParameter1, uint32_t ulParameter2, TickType_t xTicksToWait )
	{
	DaemonTaskMessage_t xMessage;
	BaseType_t xReturn;

		/* This function can only be called after a timer has been created or
		after the scheduler has been started because, until then, the timer
		queue does not exist. */
		configASSERT( xTimerQueue );

		/* Complete the message with the function parameters and post it to the
		daemon task. */
		xMessage.xMessageID = tmrCOMMAND_EXECUTE_CALLBACK;
		xMessage.u.xCallbackParameters.pxCallbackFunction = xFunctionToPend;
		xMessage.u.xCallbackParameters.pvParameter1 = pvParameter1;
		xMessage.u.xCallbackParameters.ulParameter2 = ulParameter2;

		xReturn = xQueueSendToBack( xTimerQueue, &xMessage, xTicksToWait );

		tracePEND_FUNC_CALL( xFunctionToPend, pvParameter1, ulParameter2, xReturn );

		return xReturn;
	}

#endif /* INCLUDE_xTimerPendFunctionCall */
/*-----------------------------------------------------------*/

/* This entire source file will be skipped if the application is not configured
to include software timer functionality.  If you want to include software timer
functionality then ensure configUSE_TIMERS is set to 1 in FreeRTOSConfig.h. */
#endif /* configUSE_TIMERS == 1 */



