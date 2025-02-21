/*
    FreeRTOS V9.0.0 - Copyright (C) 2016 Real Time Engineers Ltd.
    All rights reserved
*/

/* Standard includes. */
#include <stdlib.h>
#include <string.h>

/*定义 MPU_WRAPPERS_INCLUDED_FROM_API_FILE 可防止 task.h 重新定义所有 API 函数以使用 MPU 包装器。仅当从应用程序文件包含 task.h 时才应执行此操作。 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "StackMacros.h"

/* Lint e961 and e750 are suppressed as a MISRA exception justified because the
MPU ports require MPU_WRAPPERS_INCLUDED_FROM_API_FILE to be defined for the
header files above, but not in this file, in order to generate the correct
privileged Vs unprivileged linkage and placement. */
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE /*lint !e961 !e750. */

/* 将 configUSE_STATS_FORMATTING_FUNCTIONS 设置为 2 以包含统计格式化函数，但不包含 stdio.h。 */
#if ( configUSE_STATS_FORMATTING_FUNCTIONS == 1 )
	/* 此文件底部有两个可选函数，可用于从 uxTaskGetSystemState() 函数生成的原始数据生成人类可读的文本。请注意，格式化函数仅供方便使用，不被视为内核的一部分。 */
	#include <stdio.h>
#endif /* configUSE_STATS_FORMATTING_FUNCTIONS == 1 ) */

#if( configUSE_PREEMPTION == 0 )
	/* 系统为非抢占时定义为空，系统为抢占模式时，进行任务切换*/
	#define taskYIELD_IF_USING_PREEMPTION()
#else
	#define taskYIELD_IF_USING_PREEMPTION() portYIELD_WITHIN_API()
#endif

/* 可分配给 TCB 的 ucNotifyState 成员的值 */
#define taskNOT_WAITING_NOTIFICATION	( ( uint8_t ) 0 )
#define taskWAITING_NOTIFICATION		( ( uint8_t ) 1 )
#define taskNOTIFICATION_RECEIVED		( ( uint8_t ) 2 )

/*
 * 创建任务时可以用于填充任务堆栈的值
 */
#define tskSTACK_FILL_BYTE	( 0xa5U )

/* 有时，FreeRTOSConfig.h 设置仅允许使用动态分配的 RAM 创建任务，在这种情况下，当删除任何任务时，都知道任务的堆栈和 TCB 都需要释放。有时，FreeRTOSConfig.h 设置仅允许使用静态分配的 RAM 创建任务，在这种情况下，当删除任何任务时，都知道任务的堆栈或 TCB 都不应释放。
有时，FreeRTOSConfig.h 设置允许使用静态或动态分配的 RAM 创建任务，在这种情况下，TCB 的成员用于记录堆栈和/或 TCB 是静态分配的还是动态分配的，因此当删除任务时，动态分配的 RAM 会再次释放，并且不会尝试释放静态分配的 RAM。tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE 
仅在可以使用静态或动态分配的 RAM 创建任务时才为真。注意如果 portUSING_MPU_WRAPPERS 为 1，则可以使用静态分配的堆栈和动态分配的 TCB 创建受保护的任务。 */
#define tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE ( ( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) ) || ( portUSING_MPU_WRAPPERS == 1 ) )
#define tskDYNAMICALLY_ALLOCATED_STACK_AND_TCB 		( ( uint8_t ) 0 )
#define tskSTATICALLY_ALLOCATED_STACK_ONLY 			( ( uint8_t ) 1 )
#define tskSTATICALLY_ALLOCATED_STACK_AND_TCB		( ( uint8_t ) 2 )

/*
 * vListTask 使用的宏来指示任务处于哪种状态
 */
#define tskBLOCKED_CHAR		( 'B' )
#define tskREADY_CHAR		( 'R' )
#define tskDELETED_CHAR		( 'D' )
#define tskSUSPENDED_CHAR	( 'S' )

/*
 * 一些内核感知调试器要求调试器需要访问的数据是全局的，而不是文件范围的
 */
#ifdef portREMOVE_STATIC_QUALIFIER
	#define static
#endif

#if ( configUSE_PORT_OPTIMISED_TASK_SELECTION == 0 )
	/* 如果 configUSE_PORT_OPTIMISED_TASK_SELECTION 为 0，则任务选择以通用方式执行，不会针对任何特定的微控制器架构进行优化 */
	/* uxTopReadyPriority记录了当前就绪任务的最高优先级*/
	#define taskRECORD_READY_PRIORITY( uxPriority )														\
	{																									\
		if( ( uxPriority ) > uxTopReadyPriority )														\
		{																								\
			uxTopReadyPriority = ( uxPriority );														\
		}																								\
	} /* taskRECORD_READY_PRIORITY */

	/*-----------------------------------------------------------*/
	#define taskSELECT_HIGHEST_PRIORITY_TASK()															\
	{																									\
	UBaseType_t uxTopPriority = uxTopReadyPriority;														\
																										\
		/* 寻找当前就绪任务的最高优先级 */								\
		while( listLIST_IS_EMPTY( &( pxReadyTasksLists[ uxTopPriority ] ) ) )							\
		{																								\
			configASSERT( uxTopPriority );/*进入这里时uxTopPriority不可能是0，因为至少有一个优先级为0的IDLE任务*/\
			--uxTopPriority;																			\
		}																								\
																										\
		/* 获得一个具有当前最高优先级的任务赋值给pxCurrentTCB */											\
		listGET_OWNER_OF_NEXT_ENTRY( pxCurrentTCB, &( pxReadyTasksLists[ uxTopPriority ] ) );			\
		uxTopReadyPriority = uxTopPriority;																\
	} /* taskSELECT_HIGHEST_PRIORITY_TASK */

	/*-----------------------------------------------------------*/

	/* 通用方式用不到以下两个宏 */
	#define taskRESET_READY_PRIORITY( uxPriority )
	#define portRESET_READY_PRIORITY( uxPriority, uxTopReadyPriority )

#else /* configUSE_PORT_OPTIMISED_TASK_SELECTION */

	/* I如果configUSE_PORT_OPTIMISED_TASK_SELECTION 为1则根据具体的微控制器架构进行优化任务选择方式 */

	/* 更新当前就绪任务的优先级位图 */
	#define taskRECORD_READY_PRIORITY( uxPriority )	portRECORD_READY_PRIORITY( uxPriority, uxTopReadyPriority )

	/*-----------------------------------------------------------*/

	#define taskSELECT_HIGHEST_PRIORITY_TASK()														\
	{																								\
	UBaseType_t uxTopPriority;																		\
																									\
		/* 从优先级最高的任务就绪链表中选择下一个就绪任务，如果正在运行的任务的优先级与就绪链表中最高优先级任务的优先级一样也会切换到下一个任务 */								\
		portGET_HIGHEST_PRIORITY( uxTopPriority, uxTopReadyPriority );								\
		configASSERT( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ uxTopPriority ] ) ) > 0 );		\
		listGET_OWNER_OF_NEXT_ENTRY( pxCurrentTCB, &( pxReadyTasksLists[ uxTopPriority ] ) );		\
	} /* taskSELECT_HIGHEST_PRIORITY_TASK() */

	/*-----------------------------------------------------------*/

	/* 如果当前优先级就绪链表为空则清除优先级位图上的相应位 */
	#define taskRESET_READY_PRIORITY( uxPriority )														\
	{																									\
		if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ ( uxPriority ) ] ) ) == ( UBaseType_t ) 0 )	\
		{																								\
			portRESET_READY_PRIORITY( ( uxPriority ), ( uxTopReadyPriority ) );							\
		}																								\
	}

#endif /* configUSE_PORT_OPTIMISED_TASK_SELECTION */

/*-----------------------------------------------------------*/

/* tick计数发生溢出时进行pxDelayedTaskList和pxOverflowDelayedTaskList的切换 */
#define taskSWITCH_DELAYED_LISTS()																	\
{																									\
	List_t *pxTemp;																					\
																									\
	/* The delayed tasks list should be empty when the lists are switched. */						\
	configASSERT( ( listLIST_IS_EMPTY( pxDelayedTaskList ) ) );										\
																									\
	pxTemp = pxDelayedTaskList;																		\
	pxDelayedTaskList = pxOverflowDelayedTaskList;													\
	pxOverflowDelayedTaskList = pxTemp;																\
	xNumOfOverflows++;																				\
	prvResetNextTaskUnblockTime();																	\
}

/*-----------------------------------------------------------*/

/* 将就绪任务插入对应的优先级就绪链表（不排序插入最后面），并更新当前系统就绪任务的最高优先级或更新系统优先级位图，此处并没有改变pxCurrentTCB的指向 */
#define prvAddTaskToReadyList( pxTCB )																\
	traceMOVED_TASK_TO_READY_STATE( pxTCB );														\
	taskRECORD_READY_PRIORITY( ( pxTCB )->uxPriority );												\
	vListInsertEnd( &( pxReadyTasksLists[ ( pxTCB )->uxPriority ] ), &( ( pxTCB )->xStateListItem ) ); \
	tracePOST_MOVED_TASK_TO_READY_STATE( pxTCB )
/*-----------------------------------------------------------*/

/* 返回相应的TCB_t类型的指针 */
#define prvGetTCBFromHandle( pxHandle ) ( ( ( pxHandle ) == NULL ) ? ( TCB_t * ) pxCurrentTCB : ( TCB_t * ) ( pxHandle ) )

/* 事件列表项的项值通常用于保存其所属任务的优先级相关值（值初始化为( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) uxPriority，因为事件链表是按照链表项的值升序排列的，
这样才能使优先级大的任务排在前面）。但是，它偶尔会被借用于其他目的。重要的是，当它用于其他目的时，其值不会因任务优先级更改而更新（优先级翻转时任务优先级会更改）。以下位定义用于
通知调度程序不应更改该值（例如以下位定义了之后可表明事件链表项在用于等待事件，存储的值代表等待的目标事件的到来与否的状态） - 在这种情况下，使用该值的任何模块都有责任确保在释放时将其设置回其原始值。 */
#if( configUSE_16_BIT_TICKS == 1 )
	#define taskEVENT_LIST_ITEM_VALUE_IN_USE	0x8000U
#else
	#define taskEVENT_LIST_ITEM_VALUE_IN_USE	0x80000000UL
#endif

/*
 * 任务控制块的结构体定义
 */
typedef struct tskTaskControlBlock
{
	volatile StackType_t	*pxTopOfStack;	/*< 任务的栈顶指针，指向任务栈的栈顶 */

	#if ( portUSING_MPU_WRAPPERS == 1 )
		xMPU_SETTINGS	xMPUSettings;		/*< 配置任务的内存保护单元MPU */
	#endif

	ListItem_t			xStateListItem;	/*< 任务的状态链表项。FreeRTOS 使用链表来管理任务的状态（如就绪、阻塞、挂起等），该成员是任务在这些状态列表中的条目 */
	ListItem_t			xEventListItem;		/*< 任务事件链表项。任务可以在某些事件发生时（比如等待某个信号量）被挂起，xEventListItem 用于在事件列表中管理任务 */
	UBaseType_t			uxPriority;			/*< 任务的优先级，值越小表示优先级越低，0是最低优先级 */
	StackType_t			*pxStack;			/*指向任务栈的起始地址 */
	char				pcTaskName[ configMAX_TASK_NAME_LEN ];/*< 任务名  */

	#if ( portSTACK_GROWTH > 0 )
		StackType_t		*pxEndOfStack;		/*< 在栈从低内存增长的体系结构中，指向堆栈的末尾 */
	#endif

	#if ( portCRITICAL_NESTING_IN_TCB == 1 )
		UBaseType_t		uxCriticalNesting;	/*< 用于保存任务进入临界区的嵌套深度。有些平台可能不在内核层维护临界区的嵌套计数，需要在每个 TCB 中单独保存 */
	#endif

	#if ( configUSE_TRACE_FACILITY == 1 )
		UBaseType_t		uxTCBNumber;		/*< 调试功能，记录每个 TCB 的编号。用于追踪任务的创建、删除和切换 */
		UBaseType_t		uxTaskNumber;		/*< 用于为第三方跟踪工具提供一个特定的任务编号 */
	#endif

	#if ( configUSE_MUTEXES == 1 )
		UBaseType_t		uxBasePriority;		/*< 任务的原始优先级，优先级继承机制中使用。任务在持有互斥量时可能会暂时提高优先级*/
		UBaseType_t		uxMutexesHeld;		/*< 记录任务当前持有的互斥量数量 */
	#endif

	#if ( configUSE_APPLICATION_TASK_TAG == 1 )	/*< 任务钩子函数。用户可以为任务指定一个钩子函数，在任务创建时执行某些初始化工作。*/
		TaskHookFunction_t pxTaskTag;
	#endif

	#if( configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0 )	/*< 线程本地存储指针，用于存储任务相关的私有数据。多个任务可以在执行期间使用独立的本地存储。*/
		void *pvThreadLocalStoragePointers[ configNUM_THREAD_LOCAL_STORAGE_POINTERS ];
	#endif

	#if( configGENERATE_RUN_TIME_STATS == 1 )
		uint32_t		ulRunTimeCounter;	/*< 存储任务处于运行状态的时间。 */
	#endif

	#if ( configUSE_NEWLIB_REENTRANT == 1 )
		/* 新库（Newlib）线程重入结构。FreeRTOS 允许使用 Newlib 库，在任务之间共享这个结构，用于实现线程安全的库调用。 */
		struct	_reent xNewLib_reent;
	#endif

	#if( configUSE_TASK_NOTIFICATIONS == 1 )
		volatile uint32_t ulNotifiedValue;	/*< 任务通知机制中的通知值。FreeRTOS 允许任务接收通知，并使用此成员保存通知的值。*/
		volatile uint8_t ucNotifyState;		/*< 任务通知的状态，指示任务是否已经接收到通知。*/
	#endif

	/* See the comments above the definition of
	tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE. */
	#if( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0 )
		uint8_t	ucStaticallyAllocated; 		/*< 标记任务是否是静态分配的。如果任务是静态分配的（而不是动态分配的），则设置为 pdTRUE，这样就避免了错误地尝试释放静态分配的内存 */
	#endif

	#if( INCLUDE_xTaskAbortDelay == 1 )
		uint8_t ucDelayAborted;		/*< 如果任务的延迟被中止（例如由于外部事件发生），则该标志位会被设置*/
	#endif

} tskTCB;

typedef tskTCB TCB_t;

/*指向当前正在运行的任务的全局指针*/
PRIVILEGED_DATA TCB_t * volatile pxCurrentTCB = NULL;

/* 需要使用的几个链表*/
PRIVILEGED_DATA static List_t pxReadyTasksLists[ configMAX_PRIORITIES ];/*< 任务就绪优先级链表 */
PRIVILEGED_DATA static List_t xDelayedTaskList1;						/*< 存储所有被延迟的任务（即等待超时的任务）,这些任务在等待特定的时间到达后会被激活（即到达延迟时间后）*/
PRIVILEGED_DATA static List_t xDelayedTaskList2;						/*< 存储所有被延迟的任务（即等待超时的任务）,这些任务在等待特定的时间到达后会被激活（即到达延迟时间后） */
PRIVILEGED_DATA static List_t * volatile pxDelayedTaskList;				/*< 指向当前正在使用的延迟链表 */
PRIVILEGED_DATA static List_t * volatile pxOverflowDelayedTaskList;		/*< 指向备用的延迟链表*/
PRIVILEGED_DATA static List_t xPendingReadyList;						/*< 该列表存储了在调度器被挂起期间被激活的任务,这个链表中挂载的链表项是 pxTCB->xEventListItem */

#if( INCLUDE_vTaskDelete == 1 )

	PRIVILEGED_DATA static List_t xTasksWaitingTermination;				/*< 已删除的任务，但其内存尚未释放 */
	PRIVILEGED_DATA static volatile UBaseType_t uxDeletedTasksWaitingCleanUp = ( UBaseType_t ) 0U;/*记录系统中当前待删除任务的数量*/

#endif

#if ( INCLUDE_vTaskSuspend == 1 )

	PRIVILEGED_DATA static List_t xSuspendedTaskList;					/*< 这个列表存储了所有被挂起的任务,被挂起的任务不会被调度运行，直到它们被显式地唤醒 */

#endif

/* Other file private variables. --------------------------------*/
PRIVILEGED_DATA static volatile UBaseType_t uxCurrentNumberOfTasks 	= ( UBaseType_t ) 0U;		//当前系统中的任务总量（不包括IDLE任务)，只要所分配的栈和TCB没删除的任务都算（即使任务已被删除但是还没清理内存也算）
PRIVILEGED_DATA static volatile TickType_t xTickCount 				= ( TickType_t ) 0U;		//记录系统当前tick值
PRIVILEGED_DATA static volatile UBaseType_t uxTopReadyPriority 		= tskIDLE_PRIORITY;			//记录当前就绪队列中优先级最高的任务的优先级 或者 记录优先级位图（采用优化方法选择最高优先级时）
PRIVILEGED_DATA static volatile BaseType_t xSchedulerRunning 		= pdFALSE;					//记录当前调度器是否已经启动，系统初始化前为pdFALSE表示未启动
PRIVILEGED_DATA static volatile UBaseType_t uxPendedTicks 			= ( UBaseType_t ) 0U;		//该值用于记录调度器暂停时tick增加的次数
PRIVILEGED_DATA static volatile BaseType_t xYieldPending 			= pdFALSE;					//这是一个标志，表示是否有任务由挂起切换到就绪并比当前正在运行的任务具有更高优先级（需要切换任务时会将该标志置位）
PRIVILEGED_DATA static volatile BaseType_t xNumOfOverflows 			= ( BaseType_t ) 0;			//记录系统中tick计数溢出的次数
PRIVILEGED_DATA static UBaseType_t uxTaskNumber 					= ( UBaseType_t ) 0U;				
PRIVILEGED_DATA static volatile TickType_t xNextTaskUnblockTime		= ( TickType_t ) 0U; 		//记录下一个要被唤醒的任务对应的tick值
PRIVILEGED_DATA static TaskHandle_t xIdleTaskHandle					= NULL;			/*< Holds the handle of the idle task.  The idle task is created automatically when the scheduler is started. */

/* 当调度程序暂停时，上下文切换将处于待处理状态。此外，如果调度程序暂停，中断不得操纵 TCB 的 xStateListItem 或任何可以引用 xStateListItem 的列表。如果中断需要在调度程序暂停时解除任务阻塞，则它会将任务的事件列表项
移入 xPendingReadyList，以便内核在调度程序取消暂停时将任务从待处理就绪列表移至实际就绪列表。待处理就绪列表本身只能从关键部分访问。 该值用来表示当前调度器是否被暂停，初始值为pdFALSE表示未暂停，在运行*/
PRIVILEGED_DATA static volatile UBaseType_t uxSchedulerSuspended	= ( UBaseType_t ) pdFALSE;

#if ( configGENERATE_RUN_TIME_STATS == 1 )

	PRIVILEGED_DATA static uint32_t ulTaskSwitchedInTime = 0UL;	/*< Holds the value of a timer/counter the last time a task was switched in. */
	PRIVILEGED_DATA static uint32_t ulTotalRunTime = 0UL;		/*< Holds the total amount of execution time as defined by the run time counter clock. */

#endif

/*-----------------------------------------------------------*/

/* Callback function prototypes. --------------------------*/
#if(  configCHECK_FOR_STACK_OVERFLOW > 0 )
	extern void vApplicationStackOverflowHook( TaskHandle_t xTask, char *pcTaskName );
#endif

#if( configUSE_TICK_HOOK > 0 )
	extern void vApplicationTickHook( void );
#endif

#if( configSUPPORT_STATIC_ALLOCATION == 1 )
	extern void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );
#endif

/* File private functions. --------------------------------*/

/**
 * 如果 xTask 引用的任务当前处于挂起状态，则返回 pdTRUE，如果 xTask 引用的任务处于任何其他状态，则返回 pdFALSE。
 */
#if ( INCLUDE_vTaskSuspend == 1 )
	static BaseType_t prvTaskIsTaskSuspended( const TaskHandle_t xTask ) PRIVILEGED_FUNCTION;
#endif /* INCLUDE_vTaskSuspend */

/*
 * 用于初始化调度程序使用的所有链表的函数，在创建第一个任务时会自动调用此函数。
 */
static void prvInitialiseTaskLists( void ) PRIVILEGED_FUNCTION;

/*
 * 定义空闲任务IDLE对应的任务函数，等价于void prvIdleTask( void *pvParameters );
 */
static portTASK_FUNCTION_PROTO( prvIdleTask, pvParameters );

/*
 * 内存释放函数
 */
#if ( INCLUDE_vTaskDelete == 1 )

	static void prvDeleteTCB( TCB_t *pxTCB ) PRIVILEGED_FUNCTION;

#endif

/*
 * 仅由空闲任务使用，这将检查是否有任何内容已放入等待删除的任务列表中，如果是，则清理该任务并删除其 TCB。
 */
static void prvCheckTasksWaitingTermination( void ) PRIVILEGED_FUNCTION;

/*
 * 当前正在执行的任务正在进入阻塞状态，将该任务添加到当前或溢出延迟任务列表中。
 */
static void prvAddCurrentTaskToDelayedList( TickType_t xTicksToWait, const BaseType_t xCanBlockIndefinitely ) PRIVILEGED_FUNCTION;

/*
 * Fills an TaskStatus_t structure with information on each task that is
 * referenced from the pxList list (which may be a ready list, a delayed list,
 * a suspended list, etc.).
 *
 * THIS FUNCTION IS INTENDED FOR DEBUGGING ONLY, AND SHOULD NOT BE CALLED FROM
 * NORMAL APPLICATION CODE.
 */
#if ( configUSE_TRACE_FACILITY == 1 )

	static UBaseType_t prvListTasksWithinSingleList( TaskStatus_t *pxTaskStatusArray, List_t *pxList, eTaskState eState ) PRIVILEGED_FUNCTION;

#endif

/*
 * Searches pxList for a task with name pcNameToQuery - returning a handle to
 * the task if it is found, or NULL if the task is not found.
 */
#if ( INCLUDE_xTaskGetHandle == 1 )

	static TCB_t *prvSearchForNameWithinSingleList( List_t *pxList, const char pcNameToQuery[] ) PRIVILEGED_FUNCTION;

#endif

/*
 * When a task is created, the stack of the task is filled with a known value.
 * This function determines the 'high water mark' of the task stack by
 * determining how much of the stack remains at the original preset value.
 */
#if ( ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) )

	static uint16_t prvTaskCheckFreeStackSpace( const uint8_t * pucStackByte ) PRIVILEGED_FUNCTION;

#endif

/*
 * Return the amount of time, in ticks, that will pass before the kernel will
 * next move a task from the Blocked state to the Running state.
 *
 * This conditional compilation should use inequality to 0, not equality to 1.
 * This is to ensure portSUPPRESS_TICKS_AND_SLEEP() can be called when user
 * defined low power mode implementations require configUSE_TICKLESS_IDLE to be
 * set to a value other than 1.
 */
#if ( configUSE_TICKLESS_IDLE != 0 )

	static TickType_t prvGetExpectedIdleTime( void ) PRIVILEGED_FUNCTION;

#endif

/*
 * Set xNextTaskUnblockTime to the time at which the next Blocked state task
 * will exit the Blocked state.
 */
static void prvResetNextTaskUnblockTime( void );

#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) )

	/*
	 * Helper function used to pad task names with spaces when printing out
	 * human readable tables of task information.
	 */
	static char *prvWriteNameToBuffer( char *pcBuffer, const char *pcTaskName ) PRIVILEGED_FUNCTION;

#endif

/*
 * 任务TCB结构体初始化函数的声明，这个函数是无法被用户调用的
 */
static void prvInitialiseNewTask( 	TaskFunction_t pxTaskCode,
									const char * const pcName,
									const uint32_t ulStackDepth,
									void * const pvParameters,
									UBaseType_t uxPriority,
									TaskHandle_t * const pxCreatedTask,
									TCB_t *pxNewTCB,
									const MemoryRegion_t * const xRegions ) PRIVILEGED_FUNCTION; /*lint !e971 Unqualified char types are allowed for strings and single characters only. */

/*
 * 将新创建的任务添加到就绪队列的函数的声明
 */
static void prvAddNewTaskToReadyList( TCB_t *pxNewTCB ) PRIVILEGED_FUNCTION;

/*-----------------------------------------------------------*/
/* 任务的静态创建函数，静态创建一个任务，即使用传入的全局的栈来创建任务，创建成功后将任务加入任务就绪队列，创建成功返回指向成功创建的任务控制块TCB的指针，创建失败返回NULL */
#if( configSUPPORT_STATIC_ALLOCATION == 1 )

	TaskHandle_t xTaskCreateStatic(	TaskFunction_t pxTaskCode,
									const char * const pcName,
									const uint32_t ulStackDepth,
									void * const pvParameters,
									UBaseType_t uxPriority,
									StackType_t * const puxStackBuffer,
									StaticTask_t * const pxTaskBuffer ) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
	{
	TCB_t *pxNewTCB;
	TaskHandle_t xReturn;

		configASSERT( puxStackBuffer != NULL );
		configASSERT( pxTaskBuffer != NULL );

		if( ( pxTaskBuffer != NULL ) && ( puxStackBuffer != NULL ) )
		{
			pxNewTCB = ( TCB_t * ) pxTaskBuffer; /*lint !e740 Unusual cast is ok as the structures are designed to have the same alignment, and the size is checked by an assert. */
			pxNewTCB->pxStack = ( StackType_t * ) puxStackBuffer;

			#if( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0 )
			{
				/* 在任务控制块中记录当前task是静态创建的 */
				pxNewTCB->ucStaticallyAllocated = tskSTATICALLY_ALLOCATED_STACK_AND_TCB;
			}
			#endif /* configSUPPORT_DYNAMIC_ALLOCATION */

			prvInitialiseNewTask( pxTaskCode, pcName, ulStackDepth, pvParameters, uxPriority, &xReturn, pxNewTCB, NULL );
			prvAddNewTaskToReadyList( pxNewTCB );
		}
		else
		{
			xReturn = NULL;
		}

		return xReturn;
	}

#endif /* SUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

#if( portUSING_MPU_WRAPPERS == 1 )

	BaseType_t xTaskCreateRestricted( const TaskParameters_t * const pxTaskDefinition, TaskHandle_t *pxCreatedTask )
	{
	TCB_t *pxNewTCB;
	BaseType_t xReturn = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;

		configASSERT( pxTaskDefinition->puxStackBuffer );

		if( pxTaskDefinition->puxStackBuffer != NULL )
		{
			/* Allocate space for the TCB.  Where the memory comes from depends
			on the implementation of the port malloc function and whether or
			not static allocation is being used. */
			pxNewTCB = ( TCB_t * ) pvPortMalloc( sizeof( TCB_t ) );

			if( pxNewTCB != NULL )
			{
				/* Store the stack location in the TCB. */
				pxNewTCB->pxStack = pxTaskDefinition->puxStackBuffer;

				/* Tasks can be created statically or dynamically, so note
				this task had a statically allocated stack in case it is
				later deleted.  The TCB was allocated dynamically. */
				pxNewTCB->ucStaticallyAllocated = tskSTATICALLY_ALLOCATED_STACK_ONLY;

				prvInitialiseNewTask(	pxTaskDefinition->pvTaskCode,
										pxTaskDefinition->pcName,
										( uint32_t ) pxTaskDefinition->usStackDepth,
										pxTaskDefinition->pvParameters,
										pxTaskDefinition->uxPriority,
										pxCreatedTask, pxNewTCB,
										pxTaskDefinition->xRegions );

				prvAddNewTaskToReadyList( pxNewTCB );
				xReturn = pdPASS;
			}
		}

		return xReturn;
	}

#endif /* portUSING_MPU_WRAPPERS */
/*-----------------------------------------------------------*/
/*动态任务创建函数，任务所要用到的栈空间和控制块TCB的空间都是pvPortMalloc动态分配的*/
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

	BaseType_t xTaskCreate(	TaskFunction_t pxTaskCode,
							const char * const pcName,
							const uint16_t usStackDepth,
							void * const pvParameters,
							UBaseType_t uxPriority,
							TaskHandle_t * const pxCreatedTask ) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
	{
	TCB_t *pxNewTCB;
	BaseType_t xReturn;

		/* 如果堆栈向下增长，则先分配堆栈，然后再分配 TCB，这样堆栈就不会增长到 TCB 中。同样，如果堆栈向上增长，则先分配 TCB，然后再分配堆栈。 */
		#if( portSTACK_GROWTH > 0 )
		{
			/* 给TCB分配空间 */
			pxNewTCB = ( TCB_t * ) pvPortMalloc( sizeof( TCB_t ) );

			if( pxNewTCB != NULL )
			{
				/* 为正在创建的任务使用的堆栈分配空间。堆栈内存的基址存储在 TCB 中，以便稍后可以根据需要删除该任务。 */
				pxNewTCB->pxStack = ( StackType_t * ) pvPortMalloc( ( ( ( size_t ) usStackDepth ) * sizeof( StackType_t ) ) ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */

				if( pxNewTCB->pxStack == NULL )
				{
					/* Could not allocate the stack.  Delete the allocated TCB. */
					vPortFree( pxNewTCB );
					pxNewTCB = NULL;
				}
			}
		}
		#else /* portSTACK_GROWTH */
		{
		StackType_t *pxStack;

			/* 给任务分配栈空间 */
			pxStack = ( StackType_t * ) pvPortMalloc( ( ( ( size_t ) usStackDepth ) * sizeof( StackType_t ) ) ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */

			if( pxStack != NULL )
			{
				/* 给任务的TCB分配空间 */
				pxNewTCB = ( TCB_t * ) pvPortMalloc( sizeof( TCB_t ) ); /*lint !e961 MISRA exception as the casts are only redundant for some paths. */

				if( pxNewTCB != NULL )
				{
					/* Store the stack location in the TCB. */
					pxNewTCB->pxStack = pxStack;
				}
				else
				{
					/* The stack cannot be used as the TCB was not created.  Free
					it again. */
					vPortFree( pxStack );
				}
			}
			else
			{
				pxNewTCB = NULL;
			}
		}
		#endif /* portSTACK_GROWTH */

		if( pxNewTCB != NULL )
		{
			#if( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0 )
			{
				/* 在任务控制块中标记任务是动态创建的 */
				pxNewTCB->ucStaticallyAllocated = tskDYNAMICALLY_ALLOCATED_STACK_AND_TCB;
			}
			#endif /* configSUPPORT_STATIC_ALLOCATION */

			prvInitialiseNewTask( pxTaskCode, pcName, ( uint32_t ) usStackDepth, pvParameters, uxPriority, pxCreatedTask, pxNewTCB, NULL );
			prvAddNewTaskToReadyList( pxNewTCB );
			xReturn = pdPASS;
		}
		else
		{
			xReturn = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
		}

		return xReturn;
	}

#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
/*-----------------------------------------------------------*/

/*任务TCB结构体初始化函数，设置成功会将pxCreatedTask指向pxNewTCB，而pxNewTCB指向新初始化的任务TCB，这个函数是无法被用户调用的*/
static void prvInitialiseNewTask( 	TaskFunction_t pxTaskCode,
									const char * const pcName,
									const uint32_t ulStackDepth,
									void * const pvParameters,
									UBaseType_t uxPriority,
									TaskHandle_t * const pxCreatedTask,
									TCB_t *pxNewTCB,
									const MemoryRegion_t * const xRegions ) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
{
StackType_t *pxTopOfStack;
UBaseType_t x;
	#if( portUSING_MPU_WRAPPERS == 1 )
		/* Should the task be created in privileged mode? */
		BaseType_t xRunPrivileged;
		if( ( uxPriority & portPRIVILEGE_BIT ) != 0U )
		{
			xRunPrivileged = pdTRUE;
		}
		else
		{
			xRunPrivileged = pdFALSE;
		}
		uxPriority &= ~portPRIVILEGE_BIT;
	#endif /* portUSING_MPU_WRAPPERS == 1 */

	/* Avoid dependency on memset() if it is not required. */
	#if( ( configCHECK_FOR_STACK_OVERFLOW > 1 ) || ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) )
	{
		/* Fill the stack with a known value to assist debugging. */
		( void ) memset( pxNewTCB->pxStack, ( int ) tskSTACK_FILL_BYTE, ( size_t ) ulStackDepth * sizeof( StackType_t ) );
	}
	#endif /* ( ( configCHECK_FOR_STACK_OVERFLOW > 1 ) || ( ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) ) ) */

	/* 计算堆栈顶部地址，要进行地址对齐。按照栈是由高到底生长还是由低到高生长分别计算pxTopOfStack或pxEndOfStack */
	#if( portSTACK_GROWTH < 0 )
	{
		pxTopOfStack = pxNewTCB->pxStack + ( ulStackDepth - ( uint32_t ) 1 );
		pxTopOfStack = ( StackType_t * ) ( ( ( portPOINTER_SIZE_TYPE ) pxTopOfStack ) & ( ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) ) ); /*lint !e923 MISRA exception.  Avoiding casts between pointers and integers is not practical.  Size differences accounted for using portPOINTER_SIZE_TYPE type. */

		/* Check the alignment of the calculated top of stack is correct. */
		configASSERT( ( ( ( portPOINTER_SIZE_TYPE ) pxTopOfStack & ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) == 0UL ) );
	}
	#else /* portSTACK_GROWTH */
	{
		pxTopOfStack = pxNewTCB->pxStack;

		/* Check the alignment of the stack buffer is correct. */
		configASSERT( ( ( ( portPOINTER_SIZE_TYPE ) pxNewTCB->pxStack & ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) == 0UL ) );

		/* The other extreme of the stack space is required if stack checking is
		performed. */
		pxNewTCB->pxEndOfStack = pxNewTCB->pxStack + ( ulStackDepth - ( uint32_t ) 1 );
	}
	#endif /* portSTACK_GROWTH */

	/* 存储任务名到TCB中 */
	for( x = ( UBaseType_t ) 0; x < ( UBaseType_t ) configMAX_TASK_NAME_LEN; x++ )
	{
		pxNewTCB->pcTaskName[ x ] = pcName[ x ];
		if( pcName[ x ] == 0x00 )
		{
			break;
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}

	/* 确保任务名字在其长度大于configMAX_TASK_NAME_LEN时仍以'\0'结尾 */
	pxNewTCB->pcTaskName[ configMAX_TASK_NAME_LEN - 1 ] = '\0';

	/* 确保设置的任务优先级合法，优先级范围为0-(configMAX_PRIORITIES-1) */
	if( uxPriority >= ( UBaseType_t ) configMAX_PRIORITIES )
	{
		uxPriority = ( UBaseType_t ) configMAX_PRIORITIES - ( UBaseType_t ) 1U;
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

	pxNewTCB->uxPriority = uxPriority;
	#if ( configUSE_MUTEXES == 1 )
	{
		pxNewTCB->uxBasePriority = uxPriority;
		pxNewTCB->uxMutexesHeld = 0;
	}
	#endif /* configUSE_MUTEXES */
	/* 初始化任务对应的链表项 */
	vListInitialiseItem( &( pxNewTCB->xStateListItem ) );
	vListInitialiseItem( &( pxNewTCB->xEventListItem ) );

	/* 将任务所在链表项的owner绑定到该TCB */
	listSET_LIST_ITEM_OWNER( &( pxNewTCB->xStateListItem ), pxNewTCB );

	/* 设置事件链表项中的值，事件列表总是按优先级顺序排列，FreeRTOS中任务优先级是软件设定的，任务优先级数字越大，优先级越高。在Cortex-M3硬件中中断优先级数字越小，优先级越高。 */
	listSET_LIST_ITEM_VALUE( &( pxNewTCB->xEventListItem ), ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) uxPriority ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */
	listSET_LIST_ITEM_OWNER( &( pxNewTCB->xEventListItem ), pxNewTCB );

	/*设置嵌套深度*/
	#if ( portCRITICAL_NESTING_IN_TCB == 1 )
	{
		pxNewTCB->uxCriticalNesting = ( UBaseType_t ) 0U;
	}
	#endif /* portCRITICAL_NESTING_IN_TCB */

	#if ( configUSE_APPLICATION_TASK_TAG == 1 )
	{
		pxNewTCB->pxTaskTag = NULL;
	}
	#endif /* configUSE_APPLICATION_TASK_TAG */

	#if ( configGENERATE_RUN_TIME_STATS == 1 )
	{
		pxNewTCB->ulRunTimeCounter = 0UL;
	}
	#endif /* configGENERATE_RUN_TIME_STATS */

	#if ( portUSING_MPU_WRAPPERS == 1 )
	{
		vPortStoreTaskMPUSettings( &( pxNewTCB->xMPUSettings ), xRegions, pxNewTCB->pxStack, ulStackDepth );
	}
	#else
	{
		/* Avoid compiler warning about unreferenced parameter. */
		( void ) xRegions;
	}
	#endif

	#if( configNUM_THREAD_LOCAL_STORAGE_POINTERS != 0 )
	{
		for( x = 0; x < ( UBaseType_t ) configNUM_THREAD_LOCAL_STORAGE_POINTERS; x++ )
		{
			pxNewTCB->pvThreadLocalStoragePointers[ x ] = NULL;
		}
	}
	#endif

	#if ( configUSE_TASK_NOTIFICATIONS == 1 )
	{
		pxNewTCB->ulNotifiedValue = 0;
		pxNewTCB->ucNotifyState = taskNOT_WAITING_NOTIFICATION;
	}
	#endif

	#if ( configUSE_NEWLIB_REENTRANT == 1 )
	{
		/* Initialise this task's Newlib reent structure. */
		_REENT_INIT_PTR( ( &( pxNewTCB->xNewLib_reent ) ) );
	}
	#endif

	#if( INCLUDE_xTaskAbortDelay == 1 )
	{
		pxNewTCB->ucDelayAborted = pdFALSE;
	}
	#endif

	/* 初始化 TCB 任务栈，使其看起来好像任务已在运行，但已被调度程序中断。返回地址设置为任务函数的开头。一旦任务栈初始化完毕，栈顶部变量就会更新。 */
	#if( portUSING_MPU_WRAPPERS == 1 )
	{
		pxNewTCB->pxTopOfStack = pxPortInitialiseStack( pxTopOfStack, pxTaskCode, pvParameters, xRunPrivileged );
	}
	#else /* portUSING_MPU_WRAPPERS */
	{
		pxNewTCB->pxTopOfStack = pxPortInitialiseStack( pxTopOfStack, pxTaskCode, pvParameters );
	}
	#endif /* portUSING_MPU_WRAPPERS */

	if( ( void * ) pxCreatedTask != NULL )
	{
		/* Pass the handle out in an anonymous way.  The handle can be used to
		change the created task's priority, delete the created task, etc.*/
		*pxCreatedTask = ( TaskHandle_t ) pxNewTCB;
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}
}
/*-----------------------------------------------------------*/
/*该函数将新任务加入对应的优先级就绪链表，并根据当前调度器的运行状态进行调度*/
static void prvAddNewTaskToReadyList( TCB_t *pxNewTCB )
{
	/* 进入临界区关中断，确保在更新链表时不会被中断打断 */
	taskENTER_CRITICAL();
	{
		uxCurrentNumberOfTasks++;
		if( pxCurrentTCB == NULL )
		{
			/* 没有其他任务，则将此任务设为当前任务 */
			pxCurrentTCB = pxNewTCB;

			if( uxCurrentNumberOfTasks == ( UBaseType_t ) 1 )
			{
				/* 如果是系统的第一个任务则需要将系统用到的链表初始化，系统总是有一个IDLE任务（在开启调度器时被创建），所以任务数量在系统启动后肯定是>=1的 */
				prvInitialiseTaskLists();
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		else
		{
			/* 如果调度程序尚未运行即系统尚未启动，则如果它是迄今为止创建的最高优先级任务，则将该任务设为当前任务 */
			if( xSchedulerRunning == pdFALSE )
			{
				if( pxCurrentTCB->uxPriority <= pxNewTCB->uxPriority )
				{
					pxCurrentTCB = pxNewTCB;
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
		}

		uxTaskNumber++;

		#if ( configUSE_TRACE_FACILITY == 1 )
		{
			/* Add a counter into the TCB for tracing only. */
			pxNewTCB->uxTCBNumber = uxTaskNumber;
		}
		#endif /* configUSE_TRACE_FACILITY */
		traceTASK_CREATE( pxNewTCB );

		prvAddTaskToReadyList( pxNewTCB );

		portSETUP_TCB( pxNewTCB );
	}
	taskEXIT_CRITICAL();

	if( xSchedulerRunning != pdFALSE )
	{
		/* 如果系统已经启动，且新任务的优先级大于当前任务，且系统是基于抢占的，则会产生中断并进行调度 */
		if( pxCurrentTCB->uxPriority < pxNewTCB->uxPriority )
		{
			taskYIELD_IF_USING_PREEMPTION();
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
}
/*-----------------------------------------------------------*/
/*任务删除函数，若传入NULL则会删除当前正在运行的任务pxCurrentTCB*/
#if ( INCLUDE_vTaskDelete == 1 )
	void vTaskDelete( TaskHandle_t xTaskToDelete )
	{
	TCB_t *pxTCB;

		taskENTER_CRITICAL();
		{
			/* 获取要删除的任务控制块，若传入NULL则会删除当前正在运行的任务pxCurrentTCB*/
			pxTCB = prvGetTCBFromHandle( xTaskToDelete );

			/* 将其从其所在就绪/延时（阻塞）列表删除，如果删除后相应的优先级就绪列表为空则需要更新就绪位图 */
			if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
			{
				taskRESET_READY_PRIORITY( pxTCB->uxPriority );
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}

			/* 如果要删除的任务在事件列表，则将其从事件列表删除 */
			if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
			{
				( void ) uxListRemove( &( pxTCB->xEventListItem ) );
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}

			/* Increment the uxTaskNumber also so kernel aware debuggers can
			detect that the task lists need re-generating.  This is done before
			portPRE_TASK_DELETE_HOOK() as in the Windows port that macro will
			not return. */
			uxTaskNumber++;

			if( pxTCB == pxCurrentTCB )
			{
				/*若是任务要删除自身便不能在任务本身内完成，因为需要将上下文切换到另一个任务，
				将任务放在待终止列表中，idle任务将检查终止列表，并释放之前为已删除任务的TCB和堆栈分配的任何内存 */
				vListInsertEnd( &xTasksWaitingTermination, &( pxTCB->xStateListItem ) );

				/*增加uxDeletedTasksWaitingCleanUp变量，以便idle任务知道有一个任务已被删除，
				因此它将会检查xTasksWaitingTermination列表 */
				++uxDeletedTasksWaitingCleanUp;

				/* The pre-delete hook is primarily for the Windows simulator,
				in which Windows specific clean up operations are performed,
				after which it is not possible to yield away from this task -
				hence xYieldPending is used to latch that a context switch is
				required. */
				portPRE_TASK_DELETE_HOOK( pxTCB, &xYieldPending );
			}
			else
			{
				--uxCurrentNumberOfTasks;
				prvDeleteTCB( pxTCB );

				/* 重置下一次预计解除阻塞时间，以防它涉及刚刚被删除的任务。 */
				prvResetNextTaskUnblockTime();
			}

			traceTASK_DELETE( pxTCB );
		}
		taskEXIT_CRITICAL();

		/* 如果刚刚被删除的是当前正在运行的任务，且调度器在运行则立刻重新调度，重新调度会导致上下文切换，则pxCurrentTCB不再指向刚才被删除的任务。若是调度器没在运行，实际上调度器被重新唤醒后也会重新调度*/
		if( xSchedulerRunning != pdFALSE )
		{
			if( pxTCB == pxCurrentTCB )
			{
				configASSERT( uxSchedulerSuspended == 0 );
				portYIELD_WITHIN_API();
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
	}

#endif /* INCLUDE_vTaskDelete */
/*-----------------------------------------------------------*/
/*绝对时间任务阻塞函数，将当前任务相对于其上一次阻塞时间pxPreviousWakeTime阻塞xTimeIncrement*/
#if ( INCLUDE_vTaskDelayUntil == 1 )
	void vTaskDelayUntil( TickType_t * const pxPreviousWakeTime, const TickType_t xTimeIncrement )
	{
	/* pxPreviousWakeTime为上一次唤醒的时间点，xTimeIncrement为任务周期时间，xTimeToWake为下一次唤醒的时间点，xConstTickCount为进入延时（阻塞）的时间 */
		TickType_t xTimeToWake;
		BaseType_t xAlreadyYielded, xShouldDelay = pdFALSE;

		configASSERT( pxPreviousWakeTime );
		configASSERT( ( xTimeIncrement > 0U ) );
		configASSERT( uxSchedulerSuspended == 0 );

		vTaskSuspendAll();
		{
			/* Minor optimisation.  The tick count cannot change in this block. */
			const TickType_t xConstTickCount = xTickCount;

			/* Generate the tick time at which the task wants to wake. */
			xTimeToWake = *pxPreviousWakeTime + xTimeIncrement;

			if( xConstTickCount < *pxPreviousWakeTime )
			{
				/* 自从上次调用该函数以来，tick计数已经溢出。在这种情况下，我们实际上应该延迟的唯一时间
				是当唤醒时间也溢出时，并且唤醒时间大于tick时间。在这种情况下，就好像时间没有溢出一样。 */
				if( ( xTimeToWake < *pxPreviousWakeTime ) && ( xTimeToWake > xConstTickCount ) )
				{
					xShouldDelay = pdTRUE;
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				/* The tick time has not overflowed.  In this case we will
				delay if either the wake time has overflowed, and/or the
				tick time is less than the wake time. */
				if( ( xTimeToWake < *pxPreviousWakeTime ) || ( xTimeToWake > xConstTickCount ) )
				{
					xShouldDelay = pdTRUE;
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}

			/* Update the wake time ready for the next call. */
			*pxPreviousWakeTime = xTimeToWake;

			if( xShouldDelay != pdFALSE )
			{
				traceTASK_DELAY_UNTIL( xTimeToWake );

				/* prvAddCurrentTaskToDelayedList() needs the block time, not
				the time to wake, so subtract the current tick count. */
				prvAddCurrentTaskToDelayedList( xTimeToWake - xConstTickCount, pdFALSE );
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		xAlreadyYielded = xTaskResumeAll();

		/* Force a reschedule if xTaskResumeAll has not already done so, we may
		have put ourselves to sleep. */
		if( xAlreadyYielded == pdFALSE )
		{
			portYIELD_WITHIN_API();
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}

#endif /* INCLUDE_vTaskDelayUntil */
/*-----------------------------------------------------------*/
/*相对时间任务阻塞函数，将当前任务相对于当前系统时间xTickCount阻塞xTicksToDelay*/
#if ( INCLUDE_vTaskDelay == 1 )
	void vTaskDelay( const TickType_t xTicksToDelay )
	{
	BaseType_t xAlreadyYielded = pdFALSE;

		/* A delay time of zero just forces a reschedule. */
		if( xTicksToDelay > ( TickType_t ) 0U )
		{
			configASSERT( uxSchedulerSuspended == 0 );
			vTaskSuspendAll();/*关闭调度器*/
			{
				traceTASK_DELAY();

				/* A task that is removed from the event list while the
				scheduler is suspended will not get placed in the ready
				list or removed from the blocked list until the scheduler
				is resumed.

				This task cannot be in an event list as it is the currently
				executing task. */
				prvAddCurrentTaskToDelayedList( xTicksToDelay, pdFALSE );
			}
			xAlreadyYielded = xTaskResumeAll();
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		/* 如果 xTaskResumeAll 尚未执行重新调度，则强制重新调度 */
		if( xAlreadyYielded == pdFALSE )
		{
			portYIELD_WITHIN_API();
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}

#endif /* INCLUDE_vTaskDelay */
/*-----------------------------------------------------------*/
/*该函数用于获取目标任务的当前状态*/
#if( ( INCLUDE_eTaskGetState == 1 ) || ( configUSE_TRACE_FACILITY == 1 ) )
	eTaskState eTaskGetState( TaskHandle_t xTask )
	{
	eTaskState eReturn;
	List_t *pxStateList;
	const TCB_t * const pxTCB = ( TCB_t * ) xTask;

		configASSERT( pxTCB );

		if( pxTCB == pxCurrentTCB )
		{
			/* The task calling this function is querying its own state. */
			eReturn = eRunning;
		}
		else
		{
			taskENTER_CRITICAL();
			{
				pxStateList = ( List_t * ) listLIST_ITEM_CONTAINER( &( pxTCB->xStateListItem ) );
			}
			taskEXIT_CRITICAL();

			if( ( pxStateList == pxDelayedTaskList ) || ( pxStateList == pxOverflowDelayedTaskList ) )
			{
				/* The task being queried is referenced from one of the Blocked
				lists. */
				eReturn = eBlocked;
			}

			#if ( INCLUDE_vTaskSuspend == 1 )
				else if( pxStateList == &xSuspendedTaskList )
				{
					/* The task being queried is referenced from the suspended
					list.  Is it genuinely suspended or is it block
					indefinitely? */
					if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) == NULL )
					{
						eReturn = eSuspended;
					}
					else
					{
						eReturn = eBlocked;//任务被无限期等待阻塞时也会被加入xSuspendedTaskList链表中
					}
				}
			#endif

			#if ( INCLUDE_vTaskDelete == 1 )
				else if( ( pxStateList == &xTasksWaitingTermination ) || ( pxStateList == NULL ) )
				{
					/* The task being queried is referenced from the deleted
					tasks list, or it is not referenced from any lists at
					all. */
					eReturn = eDeleted;
				}
			#endif

			else /*lint !e525 Negative indentation is intended to make use of pre-processor clearer. */
			{
				/* If the task is not in any other state, it must be in the
				Ready (including pending ready) state. */
				eReturn = eReady;
			}
		}

		return eReturn;
	} /*lint !e818 xTask cannot be a pointer to const because it is a typedef. */

#endif /* INCLUDE_eTaskGetState */
/*-----------------------------------------------------------*/
/*该函数用于获取目标任务的优先级，传入NULL获取当前任务的优先级*/
#if ( INCLUDE_uxTaskPriorityGet == 1 )
	UBaseType_t uxTaskPriorityGet( TaskHandle_t xTask )
	{
	TCB_t *pxTCB;
	UBaseType_t uxReturn;

		taskENTER_CRITICAL();
		{
			/* If null is passed in here then it is the priority of the that
			called uxTaskPriorityGet() that is being queried. */
			pxTCB = prvGetTCBFromHandle( xTask );
			uxReturn = pxTCB->uxPriority;
		}
		taskEXIT_CRITICAL();

		return uxReturn;
	}

#endif /* INCLUDE_uxTaskPriorityGet */
/*-----------------------------------------------------------*/
/*该函数用于获取目标任务的优先级（用于中断中的），传入NULL获取当前任务的优先级*/
#if ( INCLUDE_uxTaskPriorityGet == 1 )

	UBaseType_t uxTaskPriorityGetFromISR( TaskHandle_t xTask )
	{
	TCB_t *pxTCB;
	UBaseType_t uxReturn, uxSavedInterruptState;

		/* RTOS ports that support interrupt nesting have the concept of a
		maximum	system call (or maximum API call) interrupt priority.
		Interrupts that are	above the maximum system call priority are keep
		permanently enabled, even when the RTOS kernel is in a critical section,
		but cannot make any calls to FreeRTOS API functions.  If configASSERT()
		is defined in FreeRTOSConfig.h then
		portASSERT_IF_INTERRUPT_PRIORITY_INVALID() will result in an assertion
		failure if a FreeRTOS API function is called from an interrupt that has
		been assigned a priority above the configured maximum system call
		priority.  Only FreeRTOS functions that end in FromISR can be called
		from interrupts	that have been assigned a priority at or (logically)
		below the maximum system call interrupt priority.  FreeRTOS maintains a
		separate interrupt safe API to ensure interrupt entry is as fast and as
		simple as possible.  More information (albeit Cortex-M specific) is
		provided on the following link:
		http://www.freertos.org/RTOS-Cortex-M3-M4.html */
		portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

		uxSavedInterruptState = portSET_INTERRUPT_MASK_FROM_ISR();
		{
			/* If null is passed in here then it is the priority of the calling
			task that is being queried. */
			pxTCB = prvGetTCBFromHandle( xTask );
			uxReturn = pxTCB->uxPriority;
		}
		portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptState );

		return uxReturn;
	}

#endif /* INCLUDE_uxTaskPriorityGet */
/*-----------------------------------------------------------*/
/*该函数用于设置目标任务的优先级，传入NULL设置当前任务的优先级*/
#if ( INCLUDE_vTaskPrioritySet == 1 )
	void vTaskPrioritySet( TaskHandle_t xTask, UBaseType_t uxNewPriority )
	{
	TCB_t *pxTCB;
	UBaseType_t uxCurrentBasePriority, uxPriorityUsedOnEntry;
	BaseType_t xYieldRequired = pdFALSE;

		configASSERT( ( uxNewPriority < configMAX_PRIORITIES ) );

		/* Ensure the new priority is valid. */
		if( uxNewPriority >= ( UBaseType_t ) configMAX_PRIORITIES )
		{
			uxNewPriority = ( UBaseType_t ) configMAX_PRIORITIES - ( UBaseType_t ) 1U;
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		taskENTER_CRITICAL();
		{
			/* If null is passed in here then it is the priority of the calling
			task that is being changed. */
			pxTCB = prvGetTCBFromHandle( xTask );

			traceTASK_PRIORITY_SET( pxTCB, uxNewPriority );

			#if ( configUSE_MUTEXES == 1 )
			{
				uxCurrentBasePriority = pxTCB->uxBasePriority;
			}
			#else
			{
				uxCurrentBasePriority = pxTCB->uxPriority;
			}
			#endif

			if( uxCurrentBasePriority != uxNewPriority )
			{
				/* The priority change may have readied a task of higher
				priority than the calling task. */
				if( uxNewPriority > uxCurrentBasePriority )
				{
					if( pxTCB != pxCurrentTCB )
					{
						/* The priority of a task other than the currently
						running task is being raised.  Is the priority being
						raised above that of the running task? */
						if( uxNewPriority >= pxCurrentTCB->uxPriority )
						{
							xYieldRequired = pdTRUE;
						}
						else
						{
							mtCOVERAGE_TEST_MARKER();
						}
					}
					else
					{
						/* The priority of the running task is being raised,
						but the running task must already be the highest
						priority task able to run so no yield is required. */
					}
				}
				else if( pxTCB == pxCurrentTCB )
				{
					/* Setting the priority of the running task down means
					there may now be another task of higher priority that
					is ready to execute. */
					xYieldRequired = pdTRUE;
				}
				else
				{
					/* Setting the priority of any other task down does not
					require a yield as the running task must be above the
					new priority of the task being modified. */
				}

				/* Remember the ready list the task might be referenced from
				before its uxPriority member is changed so the
				taskRESET_READY_PRIORITY() macro can function correctly. */
				uxPriorityUsedOnEntry = pxTCB->uxPriority;

				#if ( configUSE_MUTEXES == 1 )
				{
					/* Only change the priority being used if the task is not
					currently using an inherited priority. */
					if( pxTCB->uxBasePriority == pxTCB->uxPriority )
					{
						pxTCB->uxPriority = uxNewPriority;
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

					/* The base priority gets set whatever. */
					pxTCB->uxBasePriority = uxNewPriority;
				}
				#else
				{
					pxTCB->uxPriority = uxNewPriority;
				}
				#endif

				/* Only reset the event list item value if the value is not
				being used for anything else. */
				if( ( listGET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ) ) & taskEVENT_LIST_ITEM_VALUE_IN_USE ) == 0UL )
				{
					listSET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ), ( ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) uxNewPriority ) ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}

				/* If the task is in the blocked or suspended list we need do
				nothing more than change it's priority variable. However, if
				the task is in a ready list it needs to be removed and placed
				in the list appropriate to its new priority. */
				if( listIS_CONTAINED_WITHIN( &( pxReadyTasksLists[ uxPriorityUsedOnEntry ] ), &( pxTCB->xStateListItem ) ) != pdFALSE )
				{
					/* The task is currently in its ready list - remove before adding
					it to it's new ready list.  As we are in a critical section we
					can do this even if the scheduler is suspended. */
					if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
					{
						/* It is known that the task is in its ready list so
						there is no need to check again and the port level
						reset macro can be called directly. */
						portRESET_READY_PRIORITY( uxPriorityUsedOnEntry, uxTopReadyPriority );
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
					prvAddTaskToReadyList( pxTCB );
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}

				if( xYieldRequired != pdFALSE )
				{
					taskYIELD_IF_USING_PREEMPTION();
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}

				/* Remove compiler warning about unused variables when the port
				optimised task selection is not being used. */
				( void ) uxPriorityUsedOnEntry;
			}
		}
		taskEXIT_CRITICAL();
	}

#endif /* INCLUDE_vTaskPrioritySet */
/*-----------------------------------------------------------*/
/*该函数用于将传入的任务挂起，被挂起的任务不会再参与调度，若传入NULL则会挂起当前正在运行的任务pxCurrentTCB*/
#if ( INCLUDE_vTaskSuspend == 1 )
	void vTaskSuspend( TaskHandle_t xTaskToSuspend )
	{
	TCB_t *pxTCB;

		taskENTER_CRITICAL();
		{
			/* 如果传入的句柄为空，则会将当前正在运行的任务挂起 */
			pxTCB = prvGetTCBFromHandle( xTaskToSuspend );

			traceTASK_SUSPEND( pxTCB );

			/* 将该任务从其所在就绪/延迟（阻塞）列表移出，若移除后就绪列表为空，则会更新就绪位图 */
			if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
			{
				taskRESET_READY_PRIORITY( pxTCB->uxPriority );
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}

			/* Is the task waiting on an event also? */
			if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
			{
				( void ) uxListRemove( &( pxTCB->xEventListItem ) );
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		
			vListInsertEnd( &xSuspendedTaskList, &( pxTCB->xStateListItem ) );
		}
		taskEXIT_CRITICAL();

		if( xSchedulerRunning != pdFALSE )
		{
			/* 重置下一次预计解除阻塞的时间prvResetNextTaskUnblockTime，以防上面引用了现在处于暂停状态的任务。 */
			taskENTER_CRITICAL();
			{
				prvResetNextTaskUnblockTime();
			}
			taskEXIT_CRITICAL();
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		if( pxTCB == pxCurrentTCB )
		{
			if( xSchedulerRunning != pdFALSE )
			{
				/* 系统已经启动，若是当前正在执行的任务被挂起了，若调度器在运行那么就需要重新调度*/
				configASSERT( uxSchedulerSuspended == 0 );
				portYIELD_WITHIN_API();
			}
			else
			{
				/* 如果系统未启动则调度器没有在运行，而当前运行任务被挂起，那么就要重新选择当前运行任务 */
				if( listCURRENT_LIST_LENGTH( &xSuspendedTaskList ) == uxCurrentNumberOfTasks )
				{
					/*如果当前没有任何就绪任务，则将pxCurrentTCB置为NULL */
					pxCurrentTCB = NULL;
				}
				else
				{
					vTaskSwitchContext();
				}
			}
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}

#endif /* INCLUDE_vTaskSuspend */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskSuspend == 1 )
/*该函数用于判断一个任务是否被挂起了，若任务被挂起了则返回pdTRUE*/
	static BaseType_t prvTaskIsTaskSuspended( const TaskHandle_t xTask )
	{
	BaseType_t xReturn = pdFALSE;
	const TCB_t * const pxTCB = ( TCB_t * ) xTask;

		/* Accesses xPendingReadyList so must be called from a critical
		section. */

		/* It does not make sense to check if the calling task is suspended. */
		configASSERT( xTask );

		/* Is the task being resumed actually in the suspended list? */
		if( listIS_CONTAINED_WITHIN( &xSuspendedTaskList, &( pxTCB->xStateListItem ) ) != pdFALSE )
		{
			/* 该任务是否已从 ISR 中恢复？ */
			if( listIS_CONTAINED_WITHIN( &xPendingReadyList, &( pxTCB->xEventListItem ) ) == pdFALSE )
			{
				/* Is it in the suspended list because it is in the	Suspended
				state, or because is is blocked with no timeout? */
				if( listIS_CONTAINED_WITHIN( NULL, &( pxTCB->xEventListItem ) ) != pdFALSE )
				{
					xReturn = pdTRUE;
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
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		return xReturn;
	} /*lint !e818 xTask cannot be a pointer to const because it is a typedef. */

#endif /* INCLUDE_vTaskSuspend */
/*-----------------------------------------------------------*/
/*该函数用于将挂起的任务恢复就绪状态，无论任务被挂起多少次，只需要调用该恢复函数一次即可恢复到就绪态*/
#if ( INCLUDE_vTaskSuspend == 1 )
void vTaskResume( TaskHandle_t xTaskToResume )
{
TCB_t * const pxTCB = ( TCB_t * ) xTaskToResume;
	/* It does not make sense to resume the calling task. */
	configASSERT( xTaskToResume );
	/* 传入的参数不能为NULL或者当前正在运行的任务 */
	if( ( pxTCB != NULL ) && ( pxTCB != pxCurrentTCB ) )
	{
		taskENTER_CRITICAL();
		{
			if( prvTaskIsTaskSuspended( pxTCB ) != pdFALSE )
			{
				traceTASK_RESUME( pxTCB );

				/* 将其从当前（挂起）列表移出，并将其添加到就绪列表 */
				( void ) uxListRemove(  &( pxTCB->xStateListItem ) );
				prvAddTaskToReadyList( pxTCB );

				/* 保证当前运行的任务具有最高优先级，若被恢复的任务有更高优先级则重新调度一次 */
				if( pxTCB->uxPriority >= pxCurrentTCB->uxPriority )
				{
					/* This yield may not cause the task just resumed to run,
					but will leave the lists in the correct state for the
					next yield. */
					taskYIELD_IF_USING_PREEMPTION();
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
		}
		taskEXIT_CRITICAL();
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}
}

#endif /* INCLUDE_vTaskSuspend */

/*-----------------------------------------------------------*/
/*可在中断中使用的，将被挂起的任务唤醒并转换为就绪状态的函数*/
#if ( ( INCLUDE_xTaskResumeFromISR == 1 ) && ( INCLUDE_vTaskSuspend == 1 ) )
	BaseType_t xTaskResumeFromISR( TaskHandle_t xTaskToResume )
	{
	BaseType_t xYieldRequired = pdFALSE;
	TCB_t * const pxTCB = ( TCB_t * ) xTaskToResume;
	UBaseType_t uxSavedInterruptStatus;

		configASSERT( xTaskToResume );

		/* RTOS ports that support interrupt nesting have the concept of a
		maximum	system call (or maximum API call) interrupt priority.
		Interrupts that are	above the maximum system call priority are keep
		permanently enabled, even when the RTOS kernel is in a critical section,
		but cannot make any calls to FreeRTOS API functions.  If configASSERT()
		is defined in FreeRTOSConfig.h then
		portASSERT_IF_INTERRUPT_PRIORITY_INVALID() will result in an assertion
		failure if a FreeRTOS API function is called from an interrupt that has
		been assigned a priority above the configured maximum system call
		priority.  Only FreeRTOS functions that end in FromISR can be called
		from interrupts	that have been assigned a priority at or (logically)
		below the maximum system call interrupt priority.  FreeRTOS maintains a
		separate interrupt safe API to ensure interrupt entry is as fast and as
		simple as possible.  More information (albeit Cortex-M specific) is
		provided on the following link:
		http://www.freertos.org/RTOS-Cortex-M3-M4.html */
		portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

		uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
		{
			if( prvTaskIsTaskSuspended( pxTCB ) != pdFALSE )
			{
				traceTASK_RESUME_FROM_ISR( pxTCB );

				/* 检查调度器是否被暂停 */
				if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
				{
					/* Ready lists can be accessed so move the task from the
					suspended list to the ready list directly. */
					if( pxTCB->uxPriority >= pxCurrentTCB->uxPriority )
					{
						xYieldRequired = pdTRUE;
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

					( void ) uxListRemove( &( pxTCB->xStateListItem ) );
					prvAddTaskToReadyList( pxTCB );
				}
				else
				{
					/* The delayed or ready lists cannot be accessed so the task
					is held in the pending ready list until the scheduler is
					unsuspended. */
					vListInsertEnd( &( xPendingReadyList ), &( pxTCB->xEventListItem ) );
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

		return xYieldRequired;
	}

#endif /* ( ( INCLUDE_xTaskResumeFromISR == 1 ) && ( INCLUDE_vTaskSuspend == 1 ) ) */
/*-----------------------------------------------------------*/
/*调度器函数，创建一个IDLE任务（在创建任务的函数中会发现这是系统第一个任务所以会将pxCurrentTCB指向IDLE），该函数会调用xPortStartScheduler函数启动第一个任务，
也就是IDLE（如果在调用该函数之前没创建其他任务的话），并开始调度，该函数还会根据配置选择是否创建软件定时器任务。*/
void vTaskStartScheduler( void )
{
BaseType_t xReturn;

	/* Add the idle task at the lowest priority. */
	#if( configSUPPORT_STATIC_ALLOCATION == 1 )
	{
		StaticTask_t *pxIdleTaskTCBBuffer = NULL;
		StackType_t *pxIdleTaskStackBuffer = NULL;
		uint32_t ulIdleTaskStackSize;

		/* 获取IDLE任务需要用到的静态分配的栈空间和任务控制块，该函数需要用户自己实现，IDLE任务会处理待删除的任务 */
		vApplicationGetIdleTaskMemory( &pxIdleTaskTCBBuffer, &pxIdleTaskStackBuffer, &ulIdleTaskStackSize );
		xIdleTaskHandle = xTaskCreateStatic(	prvIdleTask,
												"IDLE",
												ulIdleTaskStackSize,
												( void * ) NULL,
												( tskIDLE_PRIORITY | portPRIVILEGE_BIT ),
												pxIdleTaskStackBuffer,
												pxIdleTaskTCBBuffer ); /*lint !e961 MISRA exception, justified as it is not a redundant explicit cast to all supported compilers. */

		if( xIdleTaskHandle != NULL )
		{
			xReturn = pdPASS;
		}
		else
		{
			xReturn = pdFAIL;
		}
	}
	#else
	{
		/* The Idle task is being created using dynamically allocated RAM. */
		xReturn = xTaskCreate(	prvIdleTask,
								"IDLE", configMINIMAL_STACK_SIZE,
								( void * ) NULL,
								( tskIDLE_PRIORITY | portPRIVILEGE_BIT ),
								&xIdleTaskHandle ); /*lint !e961 MISRA exception, justified as it is not a redundant explicit cast to all supported compilers. */
	}
	#endif /* configSUPPORT_STATIC_ALLOCATION */

	/* 根据宏定义创建软件定时器任务*/
	#if ( configUSE_TIMERS == 1 )
	{
		if( xReturn == pdPASS )
		{
			xReturn = xTimerCreateTimerTask();
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
	#endif /* configUSE_TIMERS */

	if( xReturn == pdPASS )
	{
		/* Interrupts are turned off here, to ensure a tick does not occur
		before or during the call to xPortStartScheduler().  The stacks of
		the created tasks contain a status word with interrupts switched on
		so interrupts will automatically get re-enabled when the first task
		starts to run. */
		portDISABLE_INTERRUPTS();

		#if ( configUSE_NEWLIB_REENTRANT == 1 )
		{
			/* Switch Newlib's _impure_ptr variable to point to the _reent
			structure specific to the task that will run first. */
			_impure_ptr = &( pxCurrentTCB->xNewLib_reent );
		}
		#endif /* configUSE_NEWLIB_REENTRANT */

		xNextTaskUnblockTime = portMAX_DELAY;
		xSchedulerRunning = pdTRUE;//表明系统已经启动
		xTickCount = ( TickType_t ) 0U;

		/* If configGENERATE_RUN_TIME_STATS is defined then the following
		macro must be defined to configure the timer/counter used to generate
		the run time counter time base. */
		portCONFIGURE_TIMER_FOR_RUN_TIME_STATS();

		/* 调用xPortStartScheduler函数，启动第一个任务并开始调度 */
		if( xPortStartScheduler() != pdFALSE )
		{
			/* Should not reach here as if the scheduler is running the
			function will not return. */
		}
		else
		{
			/* Should only reach here if a task calls xTaskEndScheduler(). */
		}
	}
	else
	{
		/* This line will only be reached if the kernel could not be started,
		because there was not enough FreeRTOS heap to create the idle task
		or the timer task. */
		configASSERT( xReturn != errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY );
	}

	/* Prevent compiler warnings if INCLUDE_xTaskGetIdleTaskHandle is set to 0,
	meaning xIdleTaskHandle is not used anywhere else. */
	( void ) xIdleTaskHandle;
}
/*-----------------------------------------------------------*/

void vTaskEndScheduler( void )
{
	/* Stop the scheduler interrupts and call the portable scheduler end
	routine so the original ISRs can be restored if necessary.  The port
	layer must ensure interrupts enable	bit is left in the correct state. */
	portDISABLE_INTERRUPTS();
	xSchedulerRunning = pdFALSE;
	vPortEndScheduler();
}
/*----------------------------------------------------------*/
/*该函数用于暂停调度器*/
void vTaskSuspendAll( void )
{
	/*uxSchedulerSuspended*///暂停调度，则不再允许上下文切换
	++uxSchedulerSuspended;
}
/*----------------------------------------------------------*/

#if ( configUSE_TICKLESS_IDLE != 0 )

	static TickType_t prvGetExpectedIdleTime( void )
	{
	TickType_t xReturn;
	UBaseType_t uxHigherPriorityReadyTasks = pdFALSE;

		/* uxHigherPriorityReadyTasks takes care of the case where
		configUSE_PREEMPTION is 0, so there may be tasks above the idle priority
		task that are in the Ready state, even though the idle task is
		running. */
		#if( configUSE_PORT_OPTIMISED_TASK_SELECTION == 0 )
		{
			if( uxTopReadyPriority > tskIDLE_PRIORITY )
			{
				uxHigherPriorityReadyTasks = pdTRUE;
			}
		}
		#else
		{
			const UBaseType_t uxLeastSignificantBit = ( UBaseType_t ) 0x01;

			/* When port optimised task selection is used the uxTopReadyPriority
			variable is used as a bit map.  If bits other than the least
			significant bit are set then there are tasks that have a priority
			above the idle priority that are in the Ready state.  This takes
			care of the case where the co-operative scheduler is in use. */
			if( uxTopReadyPriority > uxLeastSignificantBit )
			{
				uxHigherPriorityReadyTasks = pdTRUE;
			}
		}
		#endif

		if( pxCurrentTCB->uxPriority > tskIDLE_PRIORITY )
		{
			xReturn = 0;
		}
		else if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ tskIDLE_PRIORITY ] ) ) > 1 )
		{
			/* There are other idle priority tasks in the ready state.  If
			time slicing is used then the very next tick interrupt must be
			processed. */
			xReturn = 0;
		}
		else if( uxHigherPriorityReadyTasks != pdFALSE )
		{
			/* There are tasks in the Ready state that have a priority above the
			idle priority.  This path can only be reached if
			configUSE_PREEMPTION is 0. */
			xReturn = 0;
		}
		else
		{
			xReturn = xNextTaskUnblockTime - xTickCount;
		}

		return xReturn;
	}

#endif /* configUSE_TICKLESS_IDLE */
/*----------------------------------------------------------*/
/*该函数用于将调度器重新唤醒,调用了多少次vTaskSuspendAll就要调用多少次xTaskResumeAll才能唤醒调度器*/
BaseType_t xTaskResumeAll( void )
{
TCB_t *pxTCB = NULL;
BaseType_t xAlreadyYielded = pdFALSE;

	/* 确定当前调度器是暂停状态，如果uxSchedulerSuspended值为0则调度器不是暂停状态 */
	configASSERT( uxSchedulerSuspended );

	/* It is possible that an ISR caused a task to be removed from an event
	list while the scheduler was suspended.  If this was the case then the
	removed task will have been added to the xPendingReadyList.  Once the
	scheduler has been resumed it is safe to move all the pending ready
	tasks from this list into their appropriate ready list. */
	taskENTER_CRITICAL();
	{
		--uxSchedulerSuspended;
		/* 只有当uxSchedulerSuspended为0即pdFALSE时才说明打开了调度器*/
		if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
		{
			if( uxCurrentNumberOfTasks > ( UBaseType_t ) 0U )
			{
				/* 将任何已就绪的任务从待处理列表移至相应的就绪列表 */
				while( listLIST_IS_EMPTY( &xPendingReadyList ) == pdFALSE )
				{
					pxTCB = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( ( &xPendingReadyList ) );
					( void ) uxListRemove( &( pxTCB->xEventListItem ) );
					( void ) uxListRemove( &( pxTCB->xStateListItem ) );
					prvAddTaskToReadyList( pxTCB );

					/* 如果移动的任务的优先级高于当前任务，则必须执行让步 */
					if( pxTCB->uxPriority >= pxCurrentTCB->uxPriority )
					{
						xYieldPending = pdTRUE;
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}

				if( pxTCB != NULL )
				{
					/* 重新计算下一个唤醒任务的时间 */
					prvResetNextTaskUnblockTime();
				}

				/* 如果在调度程序暂停期间发生任何滴答，则应立即处理它们。这可确保滴答计数不会下滑，并且任何延迟的任务都会在正确的时间恢复。 */
				{
					UBaseType_t uxPendedCounts = uxPendedTicks; /* Non-volatile copy. */

					if( uxPendedCounts > ( UBaseType_t ) 0U )
					{
						do
						{
							if( xTaskIncrementTick() != pdFALSE )
							{
								xYieldPending = pdTRUE;
							}
							else
							{
								mtCOVERAGE_TEST_MARKER();
							}
							--uxPendedCounts;
						} while( uxPendedCounts > ( UBaseType_t ) 0U );

						uxPendedTicks = 0;
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}

				if( xYieldPending != pdFALSE )
				{
					#if( configUSE_PREEMPTION != 0 )
					{
						xAlreadyYielded = pdTRUE;
					}
					#endif
					taskYIELD_IF_USING_PREEMPTION();
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
	taskEXIT_CRITICAL();

	return xAlreadyYielded;
}
/*-----------------------------------------------------------*/

/* 该函数返回当前系统的tick计数时间值*/
TickType_t xTaskGetTickCount( void )
{
TickType_t xTicks;

	/* Critical section required if running on a 16 bit processor. */
	portTICK_TYPE_ENTER_CRITICAL();
	{
		xTicks = xTickCount;
	}
	portTICK_TYPE_EXIT_CRITICAL();

	return xTicks;
}
/*-----------------------------------------------------------*/
/* 该函数返回当前系统的tick计数时间值（用于中断中的）*/
TickType_t xTaskGetTickCountFromISR( void )
{
TickType_t xReturn;
UBaseType_t uxSavedInterruptStatus;

	/* RTOS ports that support interrupt nesting have the concept of a maximum
	system call (or maximum API call) interrupt priority.  Interrupts that are
	above the maximum system call priority are kept permanently enabled, even
	when the RTOS kernel is in a critical section, but cannot make any calls to
	FreeRTOS API functions.  If configASSERT() is defined in FreeRTOSConfig.h
	then portASSERT_IF_INTERRUPT_PRIORITY_INVALID() will result in an assertion
	failure if a FreeRTOS API function is called from an interrupt that has been
	assigned a priority above the configured maximum system call priority.
	Only FreeRTOS functions that end in FromISR can be called from interrupts
	that have been assigned a priority at or (logically) below the maximum
	system call	interrupt priority.  FreeRTOS maintains a separate interrupt
	safe API to ensure interrupt entry is as fast and as simple as possible.
	More information (albeit Cortex-M specific) is provided on the following
	link: http://www.freertos.org/RTOS-Cortex-M3-M4.html */
	portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

	uxSavedInterruptStatus = portTICK_TYPE_SET_INTERRUPT_MASK_FROM_ISR();
	{
		xReturn = xTickCount;
	}
	portTICK_TYPE_CLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

	return xReturn;
}
/*-----------------------------------------------------------*/
/*该函数用于获取当前系统总任务数*/
UBaseType_t uxTaskGetNumberOfTasks( void )
{
	/* A critical section is not required because the variables are of type
	BaseType_t. */
	return uxCurrentNumberOfTasks;
}
/*-----------------------------------------------------------*/
/*该函数用于获取任务控制块的名字*/
char *pcTaskGetName( TaskHandle_t xTaskToQuery ) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
{
TCB_t *pxTCB;

	/* If null is passed in here then the name of the calling task is being
	queried. */
	pxTCB = prvGetTCBFromHandle( xTaskToQuery );
	configASSERT( pxTCB );
	return &( pxTCB->pcTaskName[ 0 ] );
}
/*-----------------------------------------------------------*/
/*该函数用从链表中获取目标名字的任务控制块的任务句柄*/
#if ( INCLUDE_xTaskGetHandle == 1 )
	static TCB_t *prvSearchForNameWithinSingleList( List_t *pxList, const char pcNameToQuery[] )
	{
	TCB_t *pxNextTCB, *pxFirstTCB, *pxReturn = NULL;
	UBaseType_t x;
	char cNextChar;

		/* This function is called with the scheduler suspended. */

		if( listCURRENT_LIST_LENGTH( pxList ) > ( UBaseType_t ) 0 )
		{
			listGET_OWNER_OF_NEXT_ENTRY( pxFirstTCB, pxList );

			do
			{
				listGET_OWNER_OF_NEXT_ENTRY( pxNextTCB, pxList );

				/* Check each character in the name looking for a match or
				mismatch. */
				for( x = ( UBaseType_t ) 0; x < ( UBaseType_t ) configMAX_TASK_NAME_LEN; x++ )
				{
					cNextChar = pxNextTCB->pcTaskName[ x ];

					if( cNextChar != pcNameToQuery[ x ] )
					{
						/* Characters didn't match. */
						break;
					}
					else if( cNextChar == 0x00 )
					{
						/* Both strings terminated, a match must have been
						found. */
						pxReturn = pxNextTCB;
						break;
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}

				if( pxReturn != NULL )
				{
					/* The handle has been found. */
					break;
				}

			} while( pxNextTCB != pxFirstTCB );
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		return pxReturn;
	}

#endif /* INCLUDE_xTaskGetHandle */
/*-----------------------------------------------------------*/

#if ( INCLUDE_xTaskGetHandle == 1 )

	TaskHandle_t xTaskGetHandle( const char *pcNameToQuery ) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
	{
	UBaseType_t uxQueue = configMAX_PRIORITIES;
	TCB_t* pxTCB;

		/* Task names will be truncated to configMAX_TASK_NAME_LEN - 1 bytes. */
		configASSERT( strlen( pcNameToQuery ) < configMAX_TASK_NAME_LEN );

		vTaskSuspendAll();
		{
			/* Search the ready lists. */
			do
			{
				uxQueue--;
				pxTCB = prvSearchForNameWithinSingleList( ( List_t * ) &( pxReadyTasksLists[ uxQueue ] ), pcNameToQuery );

				if( pxTCB != NULL )
				{
					/* Found the handle. */
					break;
				}

			} while( uxQueue > ( UBaseType_t ) tskIDLE_PRIORITY ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */

			/* Search the delayed lists. */
			if( pxTCB == NULL )
			{
				pxTCB = prvSearchForNameWithinSingleList( ( List_t * ) pxDelayedTaskList, pcNameToQuery );
			}

			if( pxTCB == NULL )
			{
				pxTCB = prvSearchForNameWithinSingleList( ( List_t * ) pxOverflowDelayedTaskList, pcNameToQuery );
			}

			#if ( INCLUDE_vTaskSuspend == 1 )
			{
				if( pxTCB == NULL )
				{
					/* Search the suspended list. */
					pxTCB = prvSearchForNameWithinSingleList( &xSuspendedTaskList, pcNameToQuery );
				}
			}
			#endif

			#if( INCLUDE_vTaskDelete == 1 )
			{
				if( pxTCB == NULL )
				{
					/* Search the deleted list. */
					pxTCB = prvSearchForNameWithinSingleList( &xTasksWaitingTermination, pcNameToQuery );
				}
			}
			#endif
		}
		( void ) xTaskResumeAll();

		return ( TaskHandle_t ) pxTCB;
	}

#endif /* INCLUDE_xTaskGetHandle */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

	UBaseType_t uxTaskGetSystemState( TaskStatus_t * const pxTaskStatusArray, const UBaseType_t uxArraySize, uint32_t * const pulTotalRunTime )
	{
	UBaseType_t uxTask = 0, uxQueue = configMAX_PRIORITIES;

		vTaskSuspendAll();
		{
			/* Is there a space in the array for each task in the system? */
			if( uxArraySize >= uxCurrentNumberOfTasks )
			{
				/* Fill in an TaskStatus_t structure with information on each
				task in the Ready state. */
				do
				{
					uxQueue--;
					uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), &( pxReadyTasksLists[ uxQueue ] ), eReady );

				} while( uxQueue > ( UBaseType_t ) tskIDLE_PRIORITY ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */

				/* Fill in an TaskStatus_t structure with information on each
				task in the Blocked state. */
				uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), ( List_t * ) pxDelayedTaskList, eBlocked );
				uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), ( List_t * ) pxOverflowDelayedTaskList, eBlocked );

				#if( INCLUDE_vTaskDelete == 1 )
				{
					/* Fill in an TaskStatus_t structure with information on
					each task that has been deleted but not yet cleaned up. */
					uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), &xTasksWaitingTermination, eDeleted );
				}
				#endif

				#if ( INCLUDE_vTaskSuspend == 1 )
				{
					/* Fill in an TaskStatus_t structure with information on
					each task in the Suspended state. */
					uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), &xSuspendedTaskList, eSuspended );
				}
				#endif

				#if ( configGENERATE_RUN_TIME_STATS == 1)
				{
					if( pulTotalRunTime != NULL )
					{
						#ifdef portALT_GET_RUN_TIME_COUNTER_VALUE
							portALT_GET_RUN_TIME_COUNTER_VALUE( ( *pulTotalRunTime ) );
						#else
							*pulTotalRunTime = portGET_RUN_TIME_COUNTER_VALUE();
						#endif
					}
				}
				#else
				{
					if( pulTotalRunTime != NULL )
					{
						*pulTotalRunTime = 0;
					}
				}
				#endif
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		( void ) xTaskResumeAll();

		return uxTask;
	}

#endif /* configUSE_TRACE_FACILITY */
/*----------------------------------------------------------*/

#if ( INCLUDE_xTaskGetIdleTaskHandle == 1 )

	TaskHandle_t xTaskGetIdleTaskHandle( void )
	{
		/* If xTaskGetIdleTaskHandle() is called before the scheduler has been
		started, then xIdleTaskHandle will be NULL. */
		configASSERT( ( xIdleTaskHandle != NULL ) );
		return xIdleTaskHandle;
	}

#endif /* INCLUDE_xTaskGetIdleTaskHandle */
/*----------------------------------------------------------*/

/* This conditional compilation should use inequality to 0, not equality to 1.
This is to ensure vTaskStepTick() is available when user defined low power mode
implementations require configUSE_TICKLESS_IDLE to be set to a value other than
1. *///vTaskStepTick 函数在系统进入低功耗模式后，通过跳过一些不必要的滴答，快速更新滴答计数器 xTickCount，从而避免浪费时间和资源
#if ( configUSE_TICKLESS_IDLE != 0 )

	void vTaskStepTick( const TickType_t xTicksToJump )
	{
		/* Correct the tick count value after a period during which the tick
		was suppressed.  Note this does *not* call the tick hook function for
		each stepped tick. */
		configASSERT( ( xTickCount + xTicksToJump ) <= xNextTaskUnblockTime );
		xTickCount += xTicksToJump;
		traceINCREASE_TICK_COUNT( xTicksToJump );
	}

#endif /* configUSE_TICKLESS_IDLE */
/*----------------------------------------------------------*/

#if ( INCLUDE_xTaskAbortDelay == 1 )

	BaseType_t xTaskAbortDelay( TaskHandle_t xTask )
	{
	TCB_t *pxTCB = ( TCB_t * ) xTask;
	BaseType_t xReturn = pdFALSE;

		configASSERT( pxTCB );

		vTaskSuspendAll();
		{
			/* A task can only be prematurely removed from the Blocked state if
			it is actually in the Blocked state. */
			if( eTaskGetState( xTask ) == eBlocked )
			{
				/* Remove the reference to the task from the blocked list.  An
				interrupt won't touch the xStateListItem because the
				scheduler is suspended. */
				( void ) uxListRemove( &( pxTCB->xStateListItem ) );

				/* Is the task waiting on an event also?  If so remove it from
				the event list too.  Interrupts can touch the event list item,
				even though the scheduler is suspended, so a critical section
				is used. */
				taskENTER_CRITICAL();
				{
					if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
					{
						( void ) uxListRemove( &( pxTCB->xEventListItem ) );
						pxTCB->ucDelayAborted = pdTRUE;
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}
				taskEXIT_CRITICAL();

				/* Place the unblocked task into the appropriate ready list. */
				prvAddTaskToReadyList( pxTCB );

				/* A task being unblocked cannot cause an immediate context
				switch if preemption is turned off. */
				#if (  configUSE_PREEMPTION == 1 )
				{
					/* Preemption is on, but a context switch should only be
					performed if the unblocked task has a priority that is
					equal to or higher than the currently executing task. */
					if( pxTCB->uxPriority > pxCurrentTCB->uxPriority )
					{
						/* Pend the yield to be performed when the scheduler
						is unsuspended. */
						xYieldPending = pdTRUE;
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}
				#endif /* configUSE_PREEMPTION */
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		xTaskResumeAll();

		return xReturn;
	}

#endif /* INCLUDE_xTaskAbortDelay */
/*----------------------------------------------------------*/
/*该函数用于增加tick计数，时钟中断来临时，systick中断处理函数会调用该函数，视情况选择是否设置portNVIC_PENDSVSET_BIT进行任务调度*/
BaseType_t xTaskIncrementTick( void )
{
TCB_t * pxTCB;
TickType_t xItemValue;
BaseType_t xSwitchRequired = pdFALSE;

	/* Called by the portable layer each time a tick interrupt occurs.
	Increments the tick then checks to see if the new tick value will cause any
	tasks to be unblocked. */
	traceTASK_INCREMENT_TICK( xTickCount );
	if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )/*如果当前调度器在运行中*/
	{	
		const TickType_t xConstTickCount = xTickCount + 1;

		/* 增加系统tick计数，若溢出则切换当前延迟链表指针指向 */
		xTickCount = xConstTickCount;

		if( xConstTickCount == ( TickType_t ) 0U )
		{
			taskSWITCH_DELAYED_LISTS();
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		/* 查看此滴答计数增加是否已导致下个阻塞任务要被解除阻塞。任务按其唤醒时间的顺序存储在队列中，这意味着一旦找到一个任务，其阻塞时间尚未过期，就无需再查看列表。 */
		if( xConstTickCount >= xNextTaskUnblockTime )
		{
			for( ;; )
			{
				if( listLIST_IS_EMPTY( pxDelayedTaskList ) != pdFALSE )
				{
					/* 延迟列表为空。将 xNextTaskUnblockTime 设置为最大可能值 */
					xNextTaskUnblockTime = portMAX_DELAY; /*lint !e961 MISRA exception as the casts are only redundant for some ports. */
					break;
				}
				else
				{
					/* 延迟列表不为空，获取延迟列表头部项的值。这是延迟列表头部任务必须从阻塞状态中移除的时间 */
					pxTCB = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxDelayedTaskList );
					xItemValue = listGET_LIST_ITEM_VALUE( &( pxTCB->xStateListItem ) );

					if( xConstTickCount < xItemValue )
					{
						/* 更新xNextTaskUnblockTime并退出循环 */
						xNextTaskUnblockTime = xItemValue;
						break;
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

					/* 将该链表项从阻塞延迟链表中移除 */
					( void ) uxListRemove( &( pxTCB->xStateListItem ) );

					/* Is the task waiting on an event also?  If so remove
					it from the event list. */
					if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
					{
						( void ) uxListRemove( &( pxTCB->xEventListItem ) );
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

					/* 将该就绪任务加入就绪链表 */
					prvAddTaskToReadyList( pxTCB );

					/* 如果关闭抢占，则解除阻塞的任务不会引起立即的上下文切换。 */
					#if (  configUSE_PREEMPTION == 1 )
					{
						/* 抢占已打开，但仅当未阻塞任务的优先级等于或高于当前正在执行的任务时，才应执行上下文切换。 */
						if( pxTCB->uxPriority >= pxCurrentTCB->uxPriority )
						{
							xSwitchRequired = pdTRUE;
						}
						else
						{
							mtCOVERAGE_TEST_MARKER();
						}
					}
					#endif /* configUSE_PREEMPTION */
				}
			}
		}

		/* 如果开启了抢占功能，且应用程序编写者没有明确关闭时间片，那么与当前正在运行的任务具有同等优先级的任务将共享处理时间（时间片）。 */
		#if ( ( configUSE_PREEMPTION == 1 ) && ( configUSE_TIME_SLICING == 1 ) )
		{
			if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ pxCurrentTCB->uxPriority ] ) ) > ( UBaseType_t ) 1 )
			{
				xSwitchRequired = pdTRUE;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		#endif /* ( ( configUSE_PREEMPTION == 1 ) && ( configUSE_TIME_SLICING == 1 ) ) */

		#if ( configUSE_TICK_HOOK == 1 )
		{
			/* Guard against the tick hook being called when the pended tick
			count is being unwound (when the scheduler is being unlocked). */
			if( uxPendedTicks == ( UBaseType_t ) 0U )
			{
				vApplicationTickHook();
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		#endif /* configUSE_TICK_HOOK */
	}
	else/*下面这部分是，tick到达但是目前调度器暂停的情况，uxPendedTicks用于记录调度器暂停过程中tick增加的次数*/
	{
		++uxPendedTicks;

		/* The tick hook gets called at regular intervals, even if the
		scheduler is locked. */
		#if ( configUSE_TICK_HOOK == 1 )
		{
			vApplicationTickHook();
		}
		#endif
	}

	#if ( configUSE_PREEMPTION == 1 )
	{
		if( xYieldPending != pdFALSE )
		{
			xSwitchRequired = pdTRUE;
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
	#endif /* configUSE_PREEMPTION */

	return xSwitchRequired;
}
/*-----------------------------------------------------------*/

#if ( configUSE_APPLICATION_TASK_TAG == 1 )

	void vTaskSetApplicationTaskTag( TaskHandle_t xTask, TaskHookFunction_t pxHookFunction )
	{
	TCB_t *xTCB;

		/* If xTask is NULL then it is the task hook of the calling task that is
		getting set. */
		if( xTask == NULL )
		{
			xTCB = ( TCB_t * ) pxCurrentTCB;
		}
		else
		{
			xTCB = ( TCB_t * ) xTask;
		}

		/* Save the hook function in the TCB.  A critical section is required as
		the value can be accessed from an interrupt. */
		taskENTER_CRITICAL();
			xTCB->pxTaskTag = pxHookFunction;
		taskEXIT_CRITICAL();
	}

#endif /* configUSE_APPLICATION_TASK_TAG */
/*-----------------------------------------------------------*/

#if ( configUSE_APPLICATION_TASK_TAG == 1 )

	TaskHookFunction_t xTaskGetApplicationTaskTag( TaskHandle_t xTask )
	{
	TCB_t *xTCB;
	TaskHookFunction_t xReturn;

		/* If xTask is NULL then we are setting our own task hook. */
		if( xTask == NULL )
		{
			xTCB = ( TCB_t * ) pxCurrentTCB;
		}
		else
		{
			xTCB = ( TCB_t * ) xTask;
		}

		/* Save the hook function in the TCB.  A critical section is required as
		the value can be accessed from an interrupt. */
		taskENTER_CRITICAL();
		{
			xReturn = xTCB->pxTaskTag;
		}
		taskEXIT_CRITICAL();

		return xReturn;
	}

#endif /* configUSE_APPLICATION_TASK_TAG */
/*-----------------------------------------------------------*/

#if ( configUSE_APPLICATION_TASK_TAG == 1 )

	BaseType_t xTaskCallApplicationTaskHook( TaskHandle_t xTask, void *pvParameter )
	{
	TCB_t *xTCB;
	BaseType_t xReturn;

		/* If xTask is NULL then we are calling our own task hook. */
		if( xTask == NULL )
		{
			xTCB = ( TCB_t * ) pxCurrentTCB;
		}
		else
		{
			xTCB = ( TCB_t * ) xTask;
		}

		if( xTCB->pxTaskTag != NULL )
		{
			xReturn = xTCB->pxTaskTag( pvParameter );
		}
		else
		{
			xReturn = pdFAIL;
		}

		return xReturn;
	}

#endif /* configUSE_APPLICATION_TASK_TAG */
/*-----------------------------------------------------------*/
/*任务切换函数，调度器不在运行则不允许进行上下文切换*/
void vTaskSwitchContext( void )
{
	if( uxSchedulerSuspended != ( UBaseType_t ) pdFALSE )
	{
		/* 调度器目前处于暂停状态，不允许上下文切换 */
		xYieldPending = pdTRUE;
	}
	else
	{
		xYieldPending = pdFALSE;
		traceTASK_SWITCHED_OUT();

		#if ( configGENERATE_RUN_TIME_STATS == 1 )
		{
				#ifdef portALT_GET_RUN_TIME_COUNTER_VALUE
					portALT_GET_RUN_TIME_COUNTER_VALUE( ulTotalRunTime );
				#else
					ulTotalRunTime = portGET_RUN_TIME_COUNTER_VALUE();
				#endif

				/* Add the amount of time the task has been running to the
				accumulated time so far.  The time the task started running was
				stored in ulTaskSwitchedInTime.  Note that there is no overflow
				protection here so count values are only valid until the timer
				overflows.  The guard against negative values is to protect
				against suspect run time stat counter implementations - which
				are provided by the application, not the kernel. */
				if( ulTotalRunTime > ulTaskSwitchedInTime )
				{
					pxCurrentTCB->ulRunTimeCounter += ( ulTotalRunTime - ulTaskSwitchedInTime );
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
				ulTaskSwitchedInTime = ulTotalRunTime;
		}
		#endif /* configGENERATE_RUN_TIME_STATS */

		/* Check for stack overflow, if configured. */
		taskCHECK_FOR_STACK_OVERFLOW();

		/* 选择一个新任务运行，并将pxCurrentTCB指向它 */
		taskSELECT_HIGHEST_PRIORITY_TASK();
		traceTASK_SWITCHED_IN();

		#if ( configUSE_NEWLIB_REENTRANT == 1 )
		{
			/* Switch Newlib's _impure_ptr variable to point to the _reent
			structure specific to this task. */
			_impure_ptr = &( pxCurrentTCB->xNewLib_reent );
		}
		#endif /* configUSE_NEWLIB_REENTRANT */
	}
}
/*-----------------------------------------------------------*/
/*该函数用于将当前任务插入等待目标事件（可以是消息、信号量、事件组等）的链表，并将当前任务加入延时链表，任务可以被设置成永久阻塞地等待事件*/
void vTaskPlaceOnEventList( List_t * const pxEventList, const TickType_t xTicksToWait )
{
	configASSERT( pxEventList );

	/* THIS FUNCTION MUST BE CALLED WITH EITHER INTERRUPTS DISABLED OR THE
	SCHEDULER SUSPENDED AND THE QUEUE BEING ACCESSED LOCKED. */

	/* 将 TCB 的事件列表项放入适当的事件列表中。该列表按优先级顺序排列，因此优先级最高的任务将首先被事件唤醒。包含事件列表的队列已锁定，以防止同时访问中断。 */
	vListInsert( pxEventList, &( pxCurrentTCB->xEventListItem ) );

	prvAddCurrentTaskToDelayedList( xTicksToWait, pdTRUE );
}
/*-----------------------------------------------------------*/
/*该函数用于将当前任务加入到事件组的阻塞等待链表中，并将当前任务加入到延时链表中*/
void vTaskPlaceOnUnorderedEventList( List_t * pxEventList, const TickType_t xItemValue, const TickType_t xTicksToWait )
{
	configASSERT( pxEventList );

	/*这个函数只有在调度器被挂起时才能使用 */
	configASSERT( uxSchedulerSuspended != 0 );

	/* 将相应的值存储到任务块中时间链表项中 */
	listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xEventListItem ), xItemValue | taskEVENT_LIST_ITEM_VALUE_IN_USE );

	/* 将 TCB 的事件列表项放在相应事件组的阻塞链表的末尾 */
	vListInsertEnd( pxEventList, &( pxCurrentTCB->xEventListItem ) );

	prvAddCurrentTaskToDelayedList( xTicksToWait, pdTRUE );
}
/*-----------------------------------------------------------*/

#if( configUSE_TIMERS == 1 )
/*该函数用于将当前任务加入指定的事件链表，并将当前任务加入延时链表，xWaitIndefinitely为pdTRUE则会无限阻塞*/
	void vTaskPlaceOnEventListRestricted( List_t * const pxEventList, TickType_t xTicksToWait, const BaseType_t xWaitIndefinitely )
	{
		configASSERT( pxEventList );

		/* This function should not be called by application code hence the
		'Restricted' in its name.  It is not part of the public API.  It is
		designed for use by kernel code, and has special calling requirements -
		it should be called with the scheduler suspended. */


		/* Place the event list item of the TCB in the appropriate event list.
		In this case it is assume that this is the only task that is going to
		be waiting on this event list, so the faster vListInsertEnd() function
		can be used in place of vListInsert. */
		vListInsertEnd( pxEventList, &( pxCurrentTCB->xEventListItem ) );

		/* If the task should block indefinitely then set the block time to a
		value that will be recognised as an indefinite delay inside the
		prvAddCurrentTaskToDelayedList() function. */
		if( xWaitIndefinitely != pdFALSE )
		{
			xTicksToWait = portMAX_DELAY;
		}

		traceTASK_DELAY_UNTIL( ( xTickCount + xTicksToWait ) );
		prvAddCurrentTaskToDelayedList( xTicksToWait, xWaitIndefinitely );
	}

#endif /* configUSE_TIMERS */
/*-----------------------------------------------------------*/
/*该函数用于将pxEventList第一个链表项移出并加入就绪队列（也即优先级最高的那个链表项）*/
BaseType_t xTaskRemoveFromEventList( const List_t * const pxEventList )
{
TCB_t *pxUnblockedTCB;
BaseType_t xReturn;

	/* THIS FUNCTION MUST BE CALLED FROM A CRITICAL SECTION.  It can also be
	called from a critical section within an ISR. */

	/* The event list is sorted in priority order, so the first in the list can
	be removed as it is known to be the highest priority.  Remove the TCB from
	the delayed list, and add it to the ready list.

	If an event is for a queue that is locked then this function will never
	get called - the lock count on the queue will get modified instead.  This
	means exclusive access to the event list is guaranteed here.
	此函数假定已进行检查以确保 pxEventList 不为空 */
	pxUnblockedTCB = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxEventList );
	configASSERT( pxUnblockedTCB );
	( void ) uxListRemove( &( pxUnblockedTCB->xEventListItem ) );

	if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
	{
		( void ) uxListRemove( &( pxUnblockedTCB->xStateListItem ) );
		prvAddTaskToReadyList( pxUnblockedTCB );
	}
	else
	{
		/* The delayed and ready lists cannot be accessed, so hold this task
		pending until the scheduler is resumed. */
		vListInsertEnd( &( xPendingReadyList ), &( pxUnblockedTCB->xEventListItem ) );
	}

	if( pxUnblockedTCB->uxPriority > pxCurrentTCB->uxPriority )
	{
		/* Return true if the task removed from the event list has a higher
		priority than the calling task.  This allows the calling task to know if
		it should force a context switch now. */
		xReturn = pdTRUE;

		/* Mark that a yield is pending in case the user is not using the
		"xHigherPriorityTaskWoken" parameter to an ISR safe FreeRTOS function. */
		xYieldPending = pdTRUE;
	}
	else
	{
		xReturn = pdFALSE;
	}

	#if( configUSE_TICKLESS_IDLE != 0 )
	{
		/* If a task is blocked on a kernel object then xNextTaskUnblockTime
		might be set to the blocked task's time out time.  If the task is
		unblocked for a reason other than a timeout xNextTaskUnblockTime is
		normally left unchanged, because it is automatically reset to a new
		value when the tick count equals xNextTaskUnblockTime.  However if
		tickless idling is used it might be more important to enter sleep mode
		at the earliest possible time - so reset xNextTaskUnblockTime here to
		ensure it is updated at the earliest possible time. */
		prvResetNextTaskUnblockTime();
	}
	#endif

	return xReturn;
}
/*-----------------------------------------------------------*/
/*该函数用于将阻塞在事件组（这里等待的事件是事件组）上的任务移除，并将其从延迟链表移除并添加到就绪链表*/
BaseType_t xTaskRemoveFromUnorderedEventList( ListItem_t * pxEventListItem, const TickType_t xItemValue )
{
TCB_t *pxUnblockedTCB;
BaseType_t xReturn;

	/* 确保这个函数只有在调度器被暂停时才被调用 */
	configASSERT( uxSchedulerSuspended != pdFALSE );

	/* 更新等待事件链表上的链表项的值（这些链表项属于任务结构体成员） */
	listSET_LIST_ITEM_VALUE( pxEventListItem, xItemValue | taskEVENT_LIST_ITEM_VALUE_IN_USE );

	/* Remove the event list form the event flag.  Interrupts do not access
	event flags. */
	pxUnblockedTCB = ( TCB_t * ) listGET_LIST_ITEM_OWNER( pxEventListItem );
	configASSERT( pxUnblockedTCB );
	( void ) uxListRemove( pxEventListItem );

	/* 从延迟列表中删除任务并将其添加到就绪列表。调度程序被暂停，因此中断将不会访问就绪列表。 */
	( void ) uxListRemove( &( pxUnblockedTCB->xStateListItem ) );
	prvAddTaskToReadyList( pxUnblockedTCB );

	if( pxUnblockedTCB->uxPriority > pxCurrentTCB->uxPriority )
	{
		/* 如果从事件组阻塞链表中删除的任务比当前任务具有更高的优先级，则返回 true。这允许调用任务知道是否应立即强制进行上下文切换。 */
		xReturn = pdTRUE;

		/* Mark that a yield is pending in case the user is not using the
		"xHigherPriorityTaskWoken" parameter to an ISR safe FreeRTOS function. */
		xYieldPending = pdTRUE;
	}
	else
	{
		xReturn = pdFALSE;
	}

	return xReturn;
}
/*-----------------------------------------------------------*/
/*该函数用于设置等待时间的结构体*/
void vTaskSetTimeOutState( TimeOut_t * const pxTimeOut )
{
	configASSERT( pxTimeOut );
	pxTimeOut->xOverflowCount = xNumOfOverflows;
	pxTimeOut->xTimeOnEntering = xTickCount;
}
/*-----------------------------------------------------------*/
/*该函数用于检查任务等待消息队列满足要求的等待时间是否到达，pxTimeOut是之前记录的时间结构体，pxTicksToWait是可以等待的时间，返回pdTRUE表示到达时间了*/
BaseType_t xTaskCheckForTimeOut( TimeOut_t * const pxTimeOut, TickType_t * const pxTicksToWait )
{
BaseType_t xReturn;

	configASSERT( pxTimeOut );
	configASSERT( pxTicksToWait );

	taskENTER_CRITICAL();
	{
		/* Minor optimisation.  The tick count cannot change in this block. */
		const TickType_t xConstTickCount = xTickCount;

		#if( INCLUDE_xTaskAbortDelay == 1 )
			if( pxCurrentTCB->ucDelayAborted != pdFALSE )
			{
				/* The delay was aborted, which is not the same as a time out,
				but has the same result. */
				pxCurrentTCB->ucDelayAborted = pdFALSE;
				xReturn = pdTRUE;
			}
			else
		#endif

		#if ( INCLUDE_vTaskSuspend == 1 )
			if( *pxTicksToWait == portMAX_DELAY )
			{
				/* 如果 INCLUDE_vTaskSuspend 设置为 1，且指定的阻塞时间是最大阻塞时间，则任务应无限期阻塞，因此永远不会超时。 */
				xReturn = pdFALSE;
			}
			else
		#endif

		if( ( xNumOfOverflows != pxTimeOut->xOverflowCount ) && ( xConstTickCount >= pxTimeOut->xTimeOnEntering ) ) /*lint !e525 Indentation preferred as is to make code within pre-processor directives clearer. */
		{
			/* 如果当前溢出次数比之前调用vTaskSetTimeOutState()的溢出次数还多，且当前时间值比之前调用时还大,则说明用户指定的延迟时间一定达到了，因为这个延迟时间不可能超过portMAX_DELAY */
			xReturn = pdTRUE;
		}
		else if( ( ( TickType_t ) ( xConstTickCount - pxTimeOut->xTimeOnEntering ) ) < *pxTicksToWait ) /*lint !e961 Explicit casting is only redundant with some compilers, whereas others require it to prevent integer conversion errors. */
		{
			/* 没有到达等待时间，更新参数 */
			*pxTicksToWait -= ( xConstTickCount - pxTimeOut->xTimeOnEntering );
			vTaskSetTimeOutState( pxTimeOut );
			xReturn = pdFALSE;
		}
		else
		{
			xReturn = pdTRUE;
		}
	}
	taskEXIT_CRITICAL();

	return xReturn;
}
/*-----------------------------------------------------------*/

void vTaskMissedYield( void )
{
	xYieldPending = pdTRUE;
}
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

	UBaseType_t uxTaskGetTaskNumber( TaskHandle_t xTask )
	{
	UBaseType_t uxReturn;
	TCB_t *pxTCB;

		if( xTask != NULL )
		{
			pxTCB = ( TCB_t * ) xTask;
			uxReturn = pxTCB->uxTaskNumber;
		}
		else
		{
			uxReturn = 0U;
		}

		return uxReturn;
	}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

	void vTaskSetTaskNumber( TaskHandle_t xTask, const UBaseType_t uxHandle )
	{
	TCB_t *pxTCB;

		if( xTask != NULL )
		{
			pxTCB = ( TCB_t * ) xTask;
			pxTCB->uxTaskNumber = uxHandle;
		}
	}

#endif /* configUSE_TRACE_FACILITY */

/*
 * IDLE任务对应的函数，portTASK_FUNCTION是宏定义，转换后为void prvIdleTask( void *pvParameters )
 */
static portTASK_FUNCTION( prvIdleTask, pvParameters )
{
	/* Stop warnings. */
	( void ) pvParameters;

	/** 这是 RTOS 空闲任务，在调度程序启动时自动创建。 **/

	for( ;; )
	{
		/* See if any tasks have deleted themselves - if so then the idle task
		is responsible for freeing the deleted task's TCB and stack. */
		prvCheckTasksWaitingTermination();

		#if ( configUSE_PREEMPTION == 0 )
		{
			/* 如果不使用抢占，需要不断强制任务切换，以查看是否有其他任务可用。如果使用抢占，则不需要这样做，因为任何可用的任务都会自动获得处理器。 */
			taskYIELD();
		}
		#endif /* configUSE_PREEMPTION */

		#if ( ( configUSE_PREEMPTION == 1 ) && ( configIDLE_SHOULD_YIELD == 1 ) )
		{
			/* 使用抢占时，优先级相同的任务将被分时间片。如果共享空闲优先级的任务已准备好运行，则空闲任务应在时间片结束前让出。
			这里不需要临界区，因为只是从列表中读取，偶尔的错误值不会有影响。如果空闲优先级的就绪列表包含多个任务，则空闲任务以外的任务已准备好执行。*/
			if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ tskIDLE_PRIORITY ] ) ) > ( UBaseType_t ) 1 )
			{
				taskYIELD();
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		#endif /* ( ( configUSE_PREEMPTION == 1 ) && ( configIDLE_SHOULD_YIELD == 1 ) ) */

		#if ( configUSE_IDLE_HOOK == 1 )
		{
			extern void vApplicationIdleHook( void );

			/* Call the user defined function from within the idle task.  This
			allows the application designer to add background functionality
			without the overhead of a separate task.
			NOTE: vApplicationIdleHook() MUST NOT, UNDER ANY CIRCUMSTANCES,
			CALL A FUNCTION THAT MIGHT BLOCK. */
			vApplicationIdleHook();
		}
		#endif /* configUSE_IDLE_HOOK */

		/* This conditional compilation should use inequality to 0, not equality
		to 1.  This is to ensure portSUPPRESS_TICKS_AND_SLEEP() is called when
		user defined low power mode	implementations require
		configUSE_TICKLESS_IDLE to be set to a value other than 1. */
		#if ( configUSE_TICKLESS_IDLE != 0 )
		{
		TickType_t xExpectedIdleTime;

			/* It is not desirable to suspend then resume the scheduler on
			each iteration of the idle task.  Therefore, a preliminary
			test of the expected idle time is performed without the
			scheduler suspended.  The result here is not necessarily
			valid. */
			xExpectedIdleTime = prvGetExpectedIdleTime();

			if( xExpectedIdleTime >= configEXPECTED_IDLE_TIME_BEFORE_SLEEP )
			{
				vTaskSuspendAll();
				{
					/* Now the scheduler is suspended, the expected idle
					time can be sampled again, and this time its value can
					be used. */
					configASSERT( xNextTaskUnblockTime >= xTickCount );
					xExpectedIdleTime = prvGetExpectedIdleTime();

					if( xExpectedIdleTime >= configEXPECTED_IDLE_TIME_BEFORE_SLEEP )
					{
						traceLOW_POWER_IDLE_BEGIN();
						portSUPPRESS_TICKS_AND_SLEEP( xExpectedIdleTime );
						traceLOW_POWER_IDLE_END();
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}
				( void ) xTaskResumeAll();
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		#endif /* configUSE_TICKLESS_IDLE */
	}
}
/*-----------------------------------------------------------*/

//该函数在准备进入睡眠模式时，用来确认是否可以安全地进入某种睡眠模式
#if( configUSE_TICKLESS_IDLE != 0 )

	eSleepModeStatus eTaskConfirmSleepModeStatus( void )
	{
	/* The idle task exists in addition to the application tasks.这是一个常量，表示系统中有一个非用户任务（即空闲任务，系统管理用的任务）*/
	const UBaseType_t uxNonApplicationTasks = 1;
	eSleepModeStatus eReturn = eStandardSleep;

		if( listCURRENT_LIST_LENGTH( &xPendingReadyList ) != 0 )
		{
			/* A task was made ready while the scheduler was suspended. */
			eReturn = eAbortSleep;
		}
		else if( xYieldPending != pdFALSE )
		{
			/* A yield was pended while the scheduler was suspended. 如果 xYieldPending 为 pdTRUE，说明有任务需要切换，系统不应该进入睡眠，应该继续处理任务切换*/
			eReturn = eAbortSleep;
		}
		else
		{
			/* If all the tasks are in the suspended list (which might mean they
			have an infinite block time rather than actually being suspended)
			then it is safe to turn all clocks off and just wait for external
			interrupts. *///系统中当前任务的总数减去系统任务（即一个空闲任务）,这样可以得到用户任务的数量。如果所有任务都处于挂起状态，则表示系统没有等待运行的任务，系统可以进入深度睡眠模式
			if( listCURRENT_LIST_LENGTH( &xSuspendedTaskList ) == ( uxCurrentNumberOfTasks - uxNonApplicationTasks ) )
			{
				eReturn = eNoTasksWaitingTimeout;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}

		return eReturn;
	}

#endif /* configUSE_TICKLESS_IDLE */
/*-----------------------------------------------------------*/

#if ( configNUM_THREAD_LOCAL_STORAGE_POINTERS != 0 )

	void vTaskSetThreadLocalStoragePointer( TaskHandle_t xTaskToSet, BaseType_t xIndex, void *pvValue )
	{
	TCB_t *pxTCB;

		if( xIndex < configNUM_THREAD_LOCAL_STORAGE_POINTERS )
		{
			pxTCB = prvGetTCBFromHandle( xTaskToSet );
			pxTCB->pvThreadLocalStoragePointers[ xIndex ] = pvValue;
		}
	}

#endif /* configNUM_THREAD_LOCAL_STORAGE_POINTERS */
/*-----------------------------------------------------------*/

#if ( configNUM_THREAD_LOCAL_STORAGE_POINTERS != 0 )

	void *pvTaskGetThreadLocalStoragePointer( TaskHandle_t xTaskToQuery, BaseType_t xIndex )
	{
	void *pvReturn = NULL;
	TCB_t *pxTCB;

		if( xIndex < configNUM_THREAD_LOCAL_STORAGE_POINTERS )
		{
			pxTCB = prvGetTCBFromHandle( xTaskToQuery );
			pvReturn = pxTCB->pvThreadLocalStoragePointers[ xIndex ];
		}
		else
		{
			pvReturn = NULL;
		}

		return pvReturn;
	}

#endif /* configNUM_THREAD_LOCAL_STORAGE_POINTERS */
/*-----------------------------------------------------------*/

#if ( portUSING_MPU_WRAPPERS == 1 )

	void vTaskAllocateMPURegions( TaskHandle_t xTaskToModify, const MemoryRegion_t * const xRegions )
	{
	TCB_t *pxTCB;

		/* If null is passed in here then we are modifying the MPU settings of
		the calling task. */
		pxTCB = prvGetTCBFromHandle( xTaskToModify );

		vPortStoreTaskMPUSettings( &( pxTCB->xMPUSettings ), xRegions, NULL, 0 );
	}

#endif /* portUSING_MPU_WRAPPERS */
/*-----------------------------------------------------------*/
/*该函数用于初始化任务相关的所有链表的函数，用于初始化调度程序使用的所有链表的函数，在创建第一个任务时会自动调用此函数。*/
static void prvInitialiseTaskLists( void )
{
UBaseType_t uxPriority;

	for( uxPriority = ( UBaseType_t ) 0U; uxPriority < ( UBaseType_t ) configMAX_PRIORITIES; uxPriority++ )
	{
		vListInitialise( &( pxReadyTasksLists[ uxPriority ] ) );
	}

	vListInitialise( &xDelayedTaskList1 );
	vListInitialise( &xDelayedTaskList2 );
	vListInitialise( &xPendingReadyList );

	#if ( INCLUDE_vTaskDelete == 1 )
	{
		vListInitialise( &xTasksWaitingTermination );
	}
	#endif /* INCLUDE_vTaskDelete */

	#if ( INCLUDE_vTaskSuspend == 1 )
	{
		vListInitialise( &xSuspendedTaskList );
	}
	#endif /* INCLUDE_vTaskSuspend */

	/* 初始化任务延时链表的指针 */
	pxDelayedTaskList = &xDelayedTaskList1;
	pxOverflowDelayedTaskList = &xDelayedTaskList2;
}
/*-----------------------------------------------------------*/
/*该函数用于检查当前是否有任务待删除，这个函数只会被IDLE任务的函数调用*/
static void prvCheckTasksWaitingTermination( void )
{

	#if ( INCLUDE_vTaskDelete == 1 )
	{
		BaseType_t xListIsEmpty;

		while( uxDeletedTasksWaitingCleanUp > ( UBaseType_t ) 0U )
		{
			vTaskSuspendAll();
			{
				xListIsEmpty = listLIST_IS_EMPTY( &xTasksWaitingTermination );
			}
			( void ) xTaskResumeAll();

			if( xListIsEmpty == pdFALSE )
			{
				TCB_t *pxTCB;

				taskENTER_CRITICAL();
				{
					pxTCB = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( ( &xTasksWaitingTermination ) );
					( void ) uxListRemove( &( pxTCB->xStateListItem ) );
					--uxCurrentNumberOfTasks;
					--uxDeletedTasksWaitingCleanUp;
				}
				taskEXIT_CRITICAL();

				prvDeleteTCB( pxTCB );
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
	}
	#endif /* INCLUDE_vTaskDelete */
}
/*-----------------------------------------------------------*/

#if( configUSE_TRACE_FACILITY == 1 )

	void vTaskGetInfo( TaskHandle_t xTask, TaskStatus_t *pxTaskStatus, BaseType_t xGetFreeStackSpace, eTaskState eState )
	{
	TCB_t *pxTCB;

		/* xTask is NULL then get the state of the calling task. */
		pxTCB = prvGetTCBFromHandle( xTask );

		pxTaskStatus->xHandle = ( TaskHandle_t ) pxTCB;
		pxTaskStatus->pcTaskName = ( const char * ) &( pxTCB->pcTaskName [ 0 ] );
		pxTaskStatus->uxCurrentPriority = pxTCB->uxPriority;
		pxTaskStatus->pxStackBase = pxTCB->pxStack;
		pxTaskStatus->xTaskNumber = pxTCB->uxTCBNumber;

		#if ( INCLUDE_vTaskSuspend == 1 )
		{
			/* If the task is in the suspended list then there is a chance it is
			actually just blocked indefinitely - so really it should be reported as
			being in the Blocked state. */
			if( pxTaskStatus->eCurrentState == eSuspended )
			{
				vTaskSuspendAll();
				{
					if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
					{
						pxTaskStatus->eCurrentState = eBlocked;
					}
				}
				xTaskResumeAll();
			}
		}
		#endif /* INCLUDE_vTaskSuspend */

		#if ( configUSE_MUTEXES == 1 )
		{
			pxTaskStatus->uxBasePriority = pxTCB->uxBasePriority;
		}
		#else
		{
			pxTaskStatus->uxBasePriority = 0;
		}
		#endif

		#if ( configGENERATE_RUN_TIME_STATS == 1 )
		{
			pxTaskStatus->ulRunTimeCounter = pxTCB->ulRunTimeCounter;
		}
		#else
		{
			pxTaskStatus->ulRunTimeCounter = 0;
		}
		#endif

		/* Obtaining the task state is a little fiddly, so is only done if the value
		of eState passed into this function is eInvalid - otherwise the state is
		just set to whatever is passed in. */
		if( eState != eInvalid )
		{
			pxTaskStatus->eCurrentState = eState;
		}
		else
		{
			pxTaskStatus->eCurrentState = eTaskGetState( xTask );
		}

		/* Obtaining the stack space takes some time, so the xGetFreeStackSpace
		parameter is provided to allow it to be skipped. */
		if( xGetFreeStackSpace != pdFALSE )
		{
			#if ( portSTACK_GROWTH > 0 )
			{
				pxTaskStatus->usStackHighWaterMark = prvTaskCheckFreeStackSpace( ( uint8_t * ) pxTCB->pxEndOfStack );
			}
			#else
			{
				pxTaskStatus->usStackHighWaterMark = prvTaskCheckFreeStackSpace( ( uint8_t * ) pxTCB->pxStack );
			}
			#endif
		}
		else
		{
			pxTaskStatus->usStackHighWaterMark = 0;
		}
	}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

	static UBaseType_t prvListTasksWithinSingleList( TaskStatus_t *pxTaskStatusArray, List_t *pxList, eTaskState eState )
	{
	volatile TCB_t *pxNextTCB, *pxFirstTCB;
	UBaseType_t uxTask = 0;

		if( listCURRENT_LIST_LENGTH( pxList ) > ( UBaseType_t ) 0 )
		{
			listGET_OWNER_OF_NEXT_ENTRY( pxFirstTCB, pxList );

			/* Populate an TaskStatus_t structure within the
			pxTaskStatusArray array for each task that is referenced from
			pxList.  See the definition of TaskStatus_t in task.h for the
			meaning of each TaskStatus_t structure member. */
			do
			{
				listGET_OWNER_OF_NEXT_ENTRY( pxNextTCB, pxList );
				vTaskGetInfo( ( TaskHandle_t ) pxNextTCB, &( pxTaskStatusArray[ uxTask ] ), pdTRUE, eState );
				uxTask++;
			} while( pxNextTCB != pxFirstTCB );
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		return uxTask;
	}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) )

	static uint16_t prvTaskCheckFreeStackSpace( const uint8_t * pucStackByte )
	{
	uint32_t ulCount = 0U;

		while( *pucStackByte == ( uint8_t ) tskSTACK_FILL_BYTE )
		{
			pucStackByte -= portSTACK_GROWTH;
			ulCount++;
		}

		ulCount /= ( uint32_t ) sizeof( StackType_t ); /*lint !e961 Casting is not redundant on smaller architectures. */

		return ( uint16_t ) ulCount;
	}

#endif /* ( ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) ) */
/*-----------------------------------------------------------*/

#if ( INCLUDE_uxTaskGetStackHighWaterMark == 1 )

	UBaseType_t uxTaskGetStackHighWaterMark( TaskHandle_t xTask )
	{
	TCB_t *pxTCB;
	uint8_t *pucEndOfStack;
	UBaseType_t uxReturn;

		pxTCB = prvGetTCBFromHandle( xTask );

		#if portSTACK_GROWTH < 0
		{
			pucEndOfStack = ( uint8_t * ) pxTCB->pxStack;
		}
		#else
		{
			pucEndOfStack = ( uint8_t * ) pxTCB->pxEndOfStack;
		}
		#endif

		uxReturn = ( UBaseType_t ) prvTaskCheckFreeStackSpace( pucEndOfStack );

		return uxReturn;
	}

#endif /* INCLUDE_uxTaskGetStackHighWaterMark */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskDelete == 1 )
/*任务栈和TCB内存空间回收函数*/
	static void prvDeleteTCB( TCB_t *pxTCB )
	{
		portCLEAN_UP_TCB( pxTCB );

		/* Free up the memory allocated by the scheduler for the task.  It is up
		to the task to free any memory allocated at the application level. */
		#if ( configUSE_NEWLIB_REENTRANT == 1 )
		{
			_reclaim_reent( &( pxTCB->xNewLib_reent ) );
		}
		#endif /* configUSE_NEWLIB_REENTRANT */

		#if( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 0 ) && ( portUSING_MPU_WRAPPERS == 0 ) )
		{
			/* 该任务只能动态分配 - 释放堆栈和 TCB。 */
			vPortFree( pxTCB->pxStack );
			vPortFree( pxTCB );
		}
		#elif( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE == 1 )
		{
			/* The task could have been allocated statically or dynamically, so
			check what was statically allocated before trying to free the
			memory. */
			if( pxTCB->ucStaticallyAllocated == tskDYNAMICALLY_ALLOCATED_STACK_AND_TCB )
			{
				/* Both the stack and TCB were allocated dynamically, so both
				must be freed. */
				vPortFree( pxTCB->pxStack );
				vPortFree( pxTCB );
			}
			else if( pxTCB->ucStaticallyAllocated == tskSTATICALLY_ALLOCATED_STACK_ONLY )
			{
				/* Only the stack was statically allocated, so the TCB is the
				only memory that must be freed. */
				vPortFree( pxTCB );
			}
			else
			{
				/* Neither the stack nor the TCB were allocated dynamically, so
				nothing needs to be freed. */
				configASSERT( pxTCB->ucStaticallyAllocated == tskSTATICALLY_ALLOCATED_STACK_AND_TCB	)
				mtCOVERAGE_TEST_MARKER();
			}
		}
		#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
	}

#endif /* INCLUDE_vTaskDelete */
/*-----------------------------------------------------------*/
/*获取下一个任务唤醒的时间*/
static void prvResetNextTaskUnblockTime( void )
{
TCB_t *pxTCB;

	if( listLIST_IS_EMPTY( pxDelayedTaskList ) != pdFALSE )
	{
		/* 当前延迟列表为空。将 xNextTaskUnblockTime 设置为最大可能值 */
		xNextTaskUnblockTime = portMAX_DELAY;
	}
	else
	{
		/* 新的当前延迟列表不为空，获取延迟列表头部项的值。这是延迟列表头部任务应从阻塞状态中移除的时间 */
		( pxTCB ) = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxDelayedTaskList );
		xNextTaskUnblockTime = listGET_LIST_ITEM_VALUE( &( ( pxTCB )->xStateListItem ) );
	}
}
/*-----------------------------------------------------------*/

#if ( ( INCLUDE_xTaskGetCurrentTaskHandle == 1 ) || ( configUSE_MUTEXES == 1 ) )
/*该函数用于获取当前正在运行的任务*/
	TaskHandle_t xTaskGetCurrentTaskHandle( void )
	{
	TaskHandle_t xReturn;

		/* A critical section is not required as this is not called from
		an interrupt and the current TCB will always be the same for any
		individual execution thread. */
		xReturn = pxCurrentTCB;

		return xReturn;
	}

#endif /* ( ( INCLUDE_xTaskGetCurrentTaskHandle == 1 ) || ( configUSE_MUTEXES == 1 ) ) */
/*-----------------------------------------------------------*/

#if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
/*该函数用于获取系统调度器的状态*/
	BaseType_t xTaskGetSchedulerState( void )
	{
	BaseType_t xReturn;

		if( xSchedulerRunning == pdFALSE )
		{
			xReturn = taskSCHEDULER_NOT_STARTED;
		}
		else
		{
			if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
			{
				xReturn = taskSCHEDULER_RUNNING;
			}
			else
			{
				xReturn = taskSCHEDULER_SUSPENDED;
			}
		}

		return xReturn;
	}

#endif /* ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) ) */
/*-----------------------------------------------------------*/
/*该函数用于实现互斥量的优先级翻转机制*/
#if ( configUSE_MUTEXES == 1 )
	void vTaskPriorityInherit( TaskHandle_t const pxMutexHolder )
	{
	TCB_t * const pxTCB = ( TCB_t * ) pxMutexHolder;

		/* 如果在队列被锁定时通过中断返回了互斥锁，则互斥锁持有者现在可能为 NULL。 */
		if( pxMutexHolder != NULL )
		{
			/* 如果互斥锁的持有者的优先级低于试图获取互斥锁的任务的优先级，那么它将暂时继承试图获取互斥锁的任务的优先级。 */
			if( pxTCB->uxPriority < pxCurrentTCB->uxPriority )
			{
				/* 调整互斥锁持有者状态以适应其新优先级，仅当该值未用于其他用途时才重置事件列表项值。事件列表项的项值通常用于保存其所属任务的优先级相关值（编码以允许以相反的优先级顺序保存）*/
				if( ( listGET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ) ) & taskEVENT_LIST_ITEM_VALUE_IN_USE ) == 0UL )
				{
					listSET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ), ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) pxCurrentTCB->uxPriority ); 
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}

				/* 如果正在修改的任务处于就绪状态，则需要将其移至新列表，实现优先级继承 */
				if( listIS_CONTAINED_WITHIN( &( pxReadyTasksLists[ pxTCB->uxPriority ] ), &( pxTCB->xStateListItem ) ) != pdFALSE )
				{
					if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
					{
						taskRESET_READY_PRIORITY( pxTCB->uxPriority );
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

					/* Inherit the priority before being moved into the new list. */
					pxTCB->uxPriority = pxCurrentTCB->uxPriority;
					prvAddTaskToReadyList( pxTCB );
				}
				else
				{
					/* Just inherit the priority. */
					pxTCB->uxPriority = pxCurrentTCB->uxPriority;
				}

				traceTASK_PRIORITY_INHERIT( pxTCB, pxCurrentTCB->uxPriority );
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
	}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if ( configUSE_MUTEXES == 1 )
/*该函数用于将之前的优先级继承恢复*/
	BaseType_t xTaskPriorityDisinherit( TaskHandle_t const pxMutexHolder )
	{
	TCB_t * const pxTCB = ( TCB_t * ) pxMutexHolder;
	BaseType_t xReturn = pdFALSE;

		if( pxMutexHolder != NULL )
		{
			/* A task can only have an inherited priority if it holds the mutex.
			If the mutex is held by a task then it cannot be given from an
			interrupt, and if a mutex is given by the holding task then it must
			be the running state task. */
			configASSERT( pxTCB == pxCurrentTCB );

			configASSERT( pxTCB->uxMutexesHeld );
			( pxTCB->uxMutexesHeld )--;

			/* 互斥锁的持有者是否继承了另一个任务的优先级 */
			if( pxTCB->uxPriority != pxTCB->uxBasePriority )
			{
				/* 仅当没有任何其他互斥锁时才取消继承。*/
				if( pxTCB->uxMutexesHeld == ( UBaseType_t ) 0 )
				{
					/* A task can only have an inherited priority if it holds
					the mutex.  If the mutex is held by a task then it cannot be
					given from an interrupt, and if a mutex is given by the
					holding	task then it must be the running state task.  Remove
					the	holding task from the ready	list. */
					if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
					{
						taskRESET_READY_PRIORITY( pxTCB->uxPriority );
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

					/* Disinherit the priority before adding the task into the
					new	ready list. */
					traceTASK_PRIORITY_DISINHERIT( pxTCB, pxTCB->uxBasePriority );
					pxTCB->uxPriority = pxTCB->uxBasePriority;

					/* Reset the event list item value.  It cannot be in use for
					any other purpose if this task is running, and it must be
					running to give back the mutex. */
					listSET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ), ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) pxTCB->uxPriority ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */
					prvAddTaskToReadyList( pxTCB );

					/* Return true to indicate that a context switch is required.
					This is only actually required in the corner case whereby
					multiple mutexes were held and the mutexes were given back
					in an order different to that in which they were taken.
					If a context switch did not occur when the first mutex was
					returned, even if a task was waiting on it, then a context
					switch should occur when the last mutex is returned whether
					a task is waiting on it or not. */
					xReturn = pdTRUE;
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
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		return xReturn;
	}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if ( portCRITICAL_NESTING_IN_TCB == 1 )

	void vTaskEnterCritical( void )
	{
		portDISABLE_INTERRUPTS();

		if( xSchedulerRunning != pdFALSE )
		{
			( pxCurrentTCB->uxCriticalNesting )++;

			/* This is not the interrupt safe version of the enter critical
			function so	assert() if it is being called from an interrupt
			context.  Only API functions that end in "FromISR" can be used in an
			interrupt.  Only assert if the critical nesting count is 1 to
			protect against recursive calls if the assert function also uses a
			critical section. */
			if( pxCurrentTCB->uxCriticalNesting == 1 )
			{
				portASSERT_IF_IN_ISR();
			}
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}

#endif /* portCRITICAL_NESTING_IN_TCB */
/*-----------------------------------------------------------*/

#if ( portCRITICAL_NESTING_IN_TCB == 1 )

	void vTaskExitCritical( void )
	{
		if( xSchedulerRunning != pdFALSE )
		{
			if( pxCurrentTCB->uxCriticalNesting > 0U )
			{
				( pxCurrentTCB->uxCriticalNesting )--;

				if( pxCurrentTCB->uxCriticalNesting == 0U )
				{
					portENABLE_INTERRUPTS();
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
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}

#endif /* portCRITICAL_NESTING_IN_TCB */
/*-----------------------------------------------------------*/

#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) )

	static char *prvWriteNameToBuffer( char *pcBuffer, const char *pcTaskName )
	{
	size_t x;

		/* Start by copying the entire string. */
		strcpy( pcBuffer, pcTaskName );

		/* Pad the end of the string with spaces to ensure columns line up when
		printed out. */
		for( x = strlen( pcBuffer ); x < ( size_t ) ( configMAX_TASK_NAME_LEN - 1 ); x++ )
		{
			pcBuffer[ x ] = ' ';
		}

		/* Terminate. */
		pcBuffer[ x ] = 0x00;

		/* Return the new end of string. */
		return &( pcBuffer[ x ] );
	}

#endif /* ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) */
/*-----------------------------------------------------------*/

#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) )

	void vTaskList( char * pcWriteBuffer )
	{
	TaskStatus_t *pxTaskStatusArray;
	volatile UBaseType_t uxArraySize, x;
	char cStatus;

		/*
		 * PLEASE NOTE:
		 *
		 * This function is provided for convenience only, and is used by many
		 * of the demo applications.  Do not consider it to be part of the
		 * scheduler.
		 *
		 * vTaskList() calls uxTaskGetSystemState(), then formats part of the
		 * uxTaskGetSystemState() output into a human readable table that
		 * displays task names, states and stack usage.
		 *
		 * vTaskList() has a dependency on the sprintf() C library function that
		 * might bloat the code size, use a lot of stack, and provide different
		 * results on different platforms.  An alternative, tiny, third party,
		 * and limited functionality implementation of sprintf() is provided in
		 * many of the FreeRTOS/Demo sub-directories in a file called
		 * printf-stdarg.c (note printf-stdarg.c does not provide a full
		 * snprintf() implementation!).
		 *
		 * It is recommended that production systems call uxTaskGetSystemState()
		 * directly to get access to raw stats data, rather than indirectly
		 * through a call to vTaskList().
		 */


		/* Make sure the write buffer does not contain a string. */
		*pcWriteBuffer = 0x00;

		/* Take a snapshot of the number of tasks in case it changes while this
		function is executing. */
		uxArraySize = uxCurrentNumberOfTasks;

		/* Allocate an array index for each task.  NOTE!  if
		configSUPPORT_DYNAMIC_ALLOCATION is set to 0 then pvPortMalloc() will
		equate to NULL. */
		pxTaskStatusArray = pvPortMalloc( uxCurrentNumberOfTasks * sizeof( TaskStatus_t ) );

		if( pxTaskStatusArray != NULL )
		{
			/* Generate the (binary) data. */
			uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, NULL );

			/* Create a human readable table from the binary data. */
			for( x = 0; x < uxArraySize; x++ )
			{
				switch( pxTaskStatusArray[ x ].eCurrentState )
				{
					case eReady:		cStatus = tskREADY_CHAR;
										break;

					case eBlocked:		cStatus = tskBLOCKED_CHAR;
										break;

					case eSuspended:	cStatus = tskSUSPENDED_CHAR;
										break;

					case eDeleted:		cStatus = tskDELETED_CHAR;
										break;

					default:			/* Should not get here, but it is included
										to prevent static checking errors. */
										cStatus = 0x00;
										break;
				}

				/* Write the task name to the string, padding with spaces so it
				can be printed in tabular form more easily. */
				pcWriteBuffer = prvWriteNameToBuffer( pcWriteBuffer, pxTaskStatusArray[ x ].pcTaskName );

				/* Write the rest of the string. */
				sprintf( pcWriteBuffer, "\t%c\t%u\t%u\t%u\r\n", cStatus, ( unsigned int ) pxTaskStatusArray[ x ].uxCurrentPriority, ( unsigned int ) pxTaskStatusArray[ x ].usStackHighWaterMark, ( unsigned int ) pxTaskStatusArray[ x ].xTaskNumber );
				pcWriteBuffer += strlen( pcWriteBuffer );
			}

			/* Free the array again.  NOTE!  If configSUPPORT_DYNAMIC_ALLOCATION
			is 0 then vPortFree() will be #defined to nothing. */
			vPortFree( pxTaskStatusArray );
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}

#endif /* ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) ) */
/*----------------------------------------------------------*/

#if ( ( configGENERATE_RUN_TIME_STATS == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) )

	void vTaskGetRunTimeStats( char *pcWriteBuffer )
	{
	TaskStatus_t *pxTaskStatusArray;
	volatile UBaseType_t uxArraySize, x;
	uint32_t ulTotalTime, ulStatsAsPercentage;

		#if( configUSE_TRACE_FACILITY != 1 )
		{
			#error configUSE_TRACE_FACILITY must also be set to 1 in FreeRTOSConfig.h to use vTaskGetRunTimeStats().
		}
		#endif

		/*
		 * PLEASE NOTE:
		 *
		 * This function is provided for convenience only, and is used by many
		 * of the demo applications.  Do not consider it to be part of the
		 * scheduler.
		 *
		 * vTaskGetRunTimeStats() calls uxTaskGetSystemState(), then formats part
		 * of the uxTaskGetSystemState() output into a human readable table that
		 * displays the amount of time each task has spent in the Running state
		 * in both absolute and percentage terms.
		 *
		 * vTaskGetRunTimeStats() has a dependency on the sprintf() C library
		 * function that might bloat the code size, use a lot of stack, and
		 * provide different results on different platforms.  An alternative,
		 * tiny, third party, and limited functionality implementation of
		 * sprintf() is provided in many of the FreeRTOS/Demo sub-directories in
		 * a file called printf-stdarg.c (note printf-stdarg.c does not provide
		 * a full snprintf() implementation!).
		 *
		 * It is recommended that production systems call uxTaskGetSystemState()
		 * directly to get access to raw stats data, rather than indirectly
		 * through a call to vTaskGetRunTimeStats().
		 */

		/* Make sure the write buffer does not contain a string. */
		*pcWriteBuffer = 0x00;

		/* Take a snapshot of the number of tasks in case it changes while this
		function is executing. */
		uxArraySize = uxCurrentNumberOfTasks;

		/* Allocate an array index for each task.  NOTE!  If
		configSUPPORT_DYNAMIC_ALLOCATION is set to 0 then pvPortMalloc() will
		equate to NULL. */
		pxTaskStatusArray = pvPortMalloc( uxCurrentNumberOfTasks * sizeof( TaskStatus_t ) );

		if( pxTaskStatusArray != NULL )
		{
			/* Generate the (binary) data. */
			uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, &ulTotalTime );

			/* For percentage calculations. */
			ulTotalTime /= 100UL;

			/* Avoid divide by zero errors. */
			if( ulTotalTime > 0 )
			{
				/* Create a human readable table from the binary data. */
				for( x = 0; x < uxArraySize; x++ )
				{
					/* What percentage of the total run time has the task used?
					This will always be rounded down to the nearest integer.
					ulTotalRunTimeDiv100 has already been divided by 100. */
					ulStatsAsPercentage = pxTaskStatusArray[ x ].ulRunTimeCounter / ulTotalTime;

					/* Write the task name to the string, padding with
					spaces so it can be printed in tabular form more
					easily. */
					pcWriteBuffer = prvWriteNameToBuffer( pcWriteBuffer, pxTaskStatusArray[ x ].pcTaskName );

					if( ulStatsAsPercentage > 0UL )
					{
						#ifdef portLU_PRINTF_SPECIFIER_REQUIRED
						{
							sprintf( pcWriteBuffer, "\t%lu\t\t%lu%%\r\n", pxTaskStatusArray[ x ].ulRunTimeCounter, ulStatsAsPercentage );
						}
						#else
						{
							/* sizeof( int ) == sizeof( long ) so a smaller
							printf() library can be used. */
							sprintf( pcWriteBuffer, "\t%u\t\t%u%%\r\n", ( unsigned int ) pxTaskStatusArray[ x ].ulRunTimeCounter, ( unsigned int ) ulStatsAsPercentage );
						}
						#endif
					}
					else
					{
						/* If the percentage is zero here then the task has
						consumed less than 1% of the total run time. */
						#ifdef portLU_PRINTF_SPECIFIER_REQUIRED
						{
							sprintf( pcWriteBuffer, "\t%lu\t\t<1%%\r\n", pxTaskStatusArray[ x ].ulRunTimeCounter );
						}
						#else
						{
							/* sizeof( int ) == sizeof( long ) so a smaller
							printf() library can be used. */
							sprintf( pcWriteBuffer, "\t%u\t\t<1%%\r\n", ( unsigned int ) pxTaskStatusArray[ x ].ulRunTimeCounter );
						}
						#endif
					}

					pcWriteBuffer += strlen( pcWriteBuffer );
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}

			/* Free the array again.  NOTE!  If configSUPPORT_DYNAMIC_ALLOCATION
			is 0 then vPortFree() will be #defined to nothing. */
			vPortFree( pxTaskStatusArray );
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}

#endif /* ( ( configGENERATE_RUN_TIME_STATS == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) ) */
/*-----------------------------------------------------------*/
/*该函数用于重新设置任务控制块中事件链表项的值，将其设置为( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) pxCurrentTCB->uxPriority，说明此时已经不再用于等待事件了，返回值为当前任务等待的目标事件*/
TickType_t uxTaskResetEventItemValue( void )
{
TickType_t uxReturn;

	uxReturn = listGET_LIST_ITEM_VALUE( &( pxCurrentTCB->xEventListItem ) );

	/* 重新设置链表项的值，( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) pxCurrentTCB->uxPriority保证具有高优先级的任务会排在事件列表的前面，会先被唤醒 */
	listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xEventListItem ), ( ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) pxCurrentTCB->uxPriority ) );

	return uxReturn;
}
/*-----------------------------------------------------------*/

#if ( configUSE_MUTEXES == 1 )
/*该函数用将当前正在运行任务持有的互斥量加1*/
	void *pvTaskIncrementMutexHeldCount( void )
	{
		/* If xSemaphoreCreateMutex() is called before any tasks have been created
		then pxCurrentTCB will be NULL. */
		if( pxCurrentTCB != NULL )
		{
			( pxCurrentTCB->uxMutexesHeld )++;
		}

		return pxCurrentTCB;
	}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

/* 任务通知值获取函数（用于信号量），因为任务通知值只属于任务本身，所以它是私有的，xClearCountOnExit设置读取后是否清0，xTicksToWait设置阻塞时间 */
#if( configUSE_TASK_NOTIFICATIONS == 1 )
	uint32_t ulTaskNotifyTake( BaseType_t xClearCountOnExit, TickType_t xTicksToWait )
	{
	uint32_t ulReturn;

		taskENTER_CRITICAL();
		{
			/* 只有在任务通知值为0时才可能阻塞，因为是将通知用作于信号量 */
			if( pxCurrentTCB->ulNotifiedValue == 0UL )
			{
				/* Mark this task as waiting for a notification. */
				pxCurrentTCB->ucNotifyState = taskWAITING_NOTIFICATION;

				if( xTicksToWait > ( TickType_t ) 0 )
				{
					prvAddCurrentTaskToDelayedList( xTicksToWait, pdTRUE );
					traceTASK_NOTIFY_TAKE_BLOCK();

					/* All ports are written to allow a yield in a critical
					section (some will yield immediately, others wait until the
					critical section exits) - but it is not something that
					application code should ever do. */
					portYIELD_WITHIN_API();
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
		}
		taskEXIT_CRITICAL();

		taskENTER_CRITICAL();
		{
			traceTASK_NOTIFY_TAKE();
			ulReturn = pxCurrentTCB->ulNotifiedValue;

			if( ulReturn != 0UL )
			{
				if( xClearCountOnExit != pdFALSE )
				{
					pxCurrentTCB->ulNotifiedValue = 0UL;
				}
				else
				{
					pxCurrentTCB->ulNotifiedValue = ulReturn - 1;
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}

			pxCurrentTCB->ucNotifyState = taskNOT_WAITING_NOTIFICATION;
		}
		taskEXIT_CRITICAL();

		return ulReturn;
	}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

/* 任务通知值获取函数（该函数一般用于将任务通知用作事件或消息时），因为任务通知值只属于任务本身，所以它是私有的，ulBitsToClearOnEntry用来设置进入时需要清除的位，ulBitsToClearOnExit用来设置退出时需要清除的位，
	pulNotificationValue获取原本的任务通知值，xTicksToWait设置阻塞时间 */
#if( configUSE_TASK_NOTIFICATIONS == 1 )
	BaseType_t xTaskNotifyWait( uint32_t ulBitsToClearOnEntry, uint32_t ulBitsToClearOnExit, uint32_t *pulNotificationValue, TickType_t xTicksToWait )
	{
	BaseType_t xReturn;

		taskENTER_CRITICAL();
		{
			/* 任务通知的状态为已经收到任务通知值才会读取 */
			if( pxCurrentTCB->ucNotifyState != taskNOTIFICATION_RECEIVED )
			{
				/* 根据ulBitsToClearOnEntry清除相应位 */
				pxCurrentTCB->ulNotifiedValue &= ~ulBitsToClearOnEntry;

				/* 将任务通知的状态切换为taskWAITING_NOTIFICATION */
				pxCurrentTCB->ucNotifyState = taskWAITING_NOTIFICATION;

				if( xTicksToWait > ( TickType_t ) 0 )
				{
					prvAddCurrentTaskToDelayedList( xTicksToWait, pdTRUE );
					traceTASK_NOTIFY_WAIT_BLOCK();

					/* All ports are written to allow a yield in a critical
					section (some will yield immediately, others wait until the
					critical section exits) - but it is not something that
					application code should ever do. */
					portYIELD_WITHIN_API();
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
		}
		taskEXIT_CRITICAL();

		taskENTER_CRITICAL();
		{
			traceTASK_NOTIFY_WAIT();

			if( pulNotificationValue != NULL )
			{
				/* 保存原本的任务通知值 */
				*pulNotificationValue = pxCurrentTCB->ulNotifiedValue;
			}

			if( pxCurrentTCB->ucNotifyState == taskWAITING_NOTIFICATION )
			{
				/* 运行到此处说明延时时间到了，但仍然没有收到任务通知值 */
				xReturn = pdFALSE;
			}
			else
			{
				/* 已经收到任务通知值，根据ulBitsToClearOnExit清除相关位 */
				pxCurrentTCB->ulNotifiedValue &= ~ulBitsToClearOnExit;
				xReturn = pdTRUE;
			}

			pxCurrentTCB->ucNotifyState = taskNOT_WAITING_NOTIFICATION;
		}
		taskEXIT_CRITICAL();

		return xReturn;
	}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

/* 任务通知发送函数，xTaskToNotify为任务句柄，ulValue为发送的值，eAction为更新通知的方式，pulPreviousNotificationValue用于获取原本的任务通知的值，若传入NULL则不获取 */
#if( configUSE_TASK_NOTIFICATIONS == 1 )
	BaseType_t xTaskGenericNotify( TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction, uint32_t *pulPreviousNotificationValue )
	{
	TCB_t * pxTCB;
	BaseType_t xReturn = pdPASS;
	uint8_t ucOriginalNotifyState;

		configASSERT( xTaskToNotify );
		pxTCB = ( TCB_t * ) xTaskToNotify;

		taskENTER_CRITICAL();
		{
			if( pulPreviousNotificationValue != NULL )
			{
				*pulPreviousNotificationValue = pxTCB->ulNotifiedValue;
			}

			ucOriginalNotifyState = pxTCB->ucNotifyState;

			pxTCB->ucNotifyState = taskNOTIFICATION_RECEIVED;

			switch( eAction )
			{
				case eSetBits	:
					pxTCB->ulNotifiedValue |= ulValue;
					break;

				case eIncrement	:
					( pxTCB->ulNotifiedValue )++;
					break;

				case eSetValueWithOverwrite	:
					pxTCB->ulNotifiedValue = ulValue;
					break;

				case eSetValueWithoutOverwrite :
					if( ucOriginalNotifyState != taskNOTIFICATION_RECEIVED )
					{
						pxTCB->ulNotifiedValue = ulValue;
					}
					else
					{
						/* The value could not be written to the task. */
						xReturn = pdFAIL;
					}
					break;

				case eNoAction:
					/*不更新任务通知值 */
					break;
			}

			traceTASK_NOTIFY();

			/* 如果任务正因为等待任务通知值而阻塞，则将其从阻塞列表移除 */
			if( ucOriginalNotifyState == taskWAITING_NOTIFICATION )
			{
				( void ) uxListRemove( &( pxTCB->xStateListItem ) );
				prvAddTaskToReadyList( pxTCB );

				/* The task should not have been on an event list. */
				configASSERT( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) == NULL );

				#if( configUSE_TICKLESS_IDLE != 0 )
				{
					/* If a task is blocked waiting for a notification then
					xNextTaskUnblockTime might be set to the blocked task's time
					out time.  If the task is unblocked for a reason other than
					a timeout xNextTaskUnblockTime is normally left unchanged,
					because it will automatically get reset to a new value when
					the tick count equals xNextTaskUnblockTime.  However if
					tickless idling is used it might be more important to enter
					sleep mode at the earliest possible time - so reset
					xNextTaskUnblockTime here to ensure it is updated at the
					earliest possible time. */
					prvResetNextTaskUnblockTime();
				}
				#endif

				if( pxTCB->uxPriority > pxCurrentTCB->uxPriority )
				{
					/* The notified task has a priority above the currently
					executing task so a yield is required. */
					taskYIELD_IF_USING_PREEMPTION();
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
		}
		taskEXIT_CRITICAL();

		return xReturn;
	}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

/* 该函数用于在中断中向一个任务发送任务通知值 */
#if( configUSE_TASK_NOTIFICATIONS == 1 )
	BaseType_t xTaskGenericNotifyFromISR( TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction, uint32_t *pulPreviousNotificationValue, BaseType_t *pxHigherPriorityTaskWoken )
	{
	TCB_t * pxTCB;
	uint8_t ucOriginalNotifyState;
	BaseType_t xReturn = pdPASS;
	UBaseType_t uxSavedInterruptStatus;

		configASSERT( xTaskToNotify );

		/* RTOS ports that support interrupt nesting have the concept of a
		maximum	system call (or maximum API call) interrupt priority.
		Interrupts that are	above the maximum system call priority are keep
		permanently enabled, even when the RTOS kernel is in a critical section,
		but cannot make any calls to FreeRTOS API functions.  If configASSERT()
		is defined in FreeRTOSConfig.h then
		portASSERT_IF_INTERRUPT_PRIORITY_INVALID() will result in an assertion
		failure if a FreeRTOS API function is called from an interrupt that has
		been assigned a priority above the configured maximum system call
		priority.  Only FreeRTOS functions that end in FromISR can be called
		from interrupts	that have been assigned a priority at or (logically)
		below the maximum system call interrupt priority.  FreeRTOS maintains a
		separate interrupt safe API to ensure interrupt entry is as fast and as
		simple as possible.  More information (albeit Cortex-M specific) is
		provided on the following link:
		http://www.freertos.org/RTOS-Cortex-M3-M4.html */
		portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

		pxTCB = ( TCB_t * ) xTaskToNotify;

		uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
		{
			if( pulPreviousNotificationValue != NULL )
			{
				*pulPreviousNotificationValue = pxTCB->ulNotifiedValue;
			}

			ucOriginalNotifyState = pxTCB->ucNotifyState;
			pxTCB->ucNotifyState = taskNOTIFICATION_RECEIVED;

			switch( eAction )
			{
				case eSetBits	:
					pxTCB->ulNotifiedValue |= ulValue;
					break;

				case eIncrement	:
					( pxTCB->ulNotifiedValue )++;
					break;

				case eSetValueWithOverwrite	:
					pxTCB->ulNotifiedValue = ulValue;
					break;

				case eSetValueWithoutOverwrite :
					if( ucOriginalNotifyState != taskNOTIFICATION_RECEIVED )
					{
						pxTCB->ulNotifiedValue = ulValue;
					}
					else
					{
						/* The value could not be written to the task. */
						xReturn = pdFAIL;
					}
					break;

				case eNoAction :
					/* The task is being notified without its notify value being
					updated. */
					break;
			}

			traceTASK_NOTIFY_FROM_ISR();

			/* If the task is in the blocked state specifically to wait for a
			notification then unblock it now. */
			if( ucOriginalNotifyState == taskWAITING_NOTIFICATION )
			{
				/* The task should not have been on an event list. */
				configASSERT( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) == NULL );

				if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
				{
					( void ) uxListRemove( &( pxTCB->xStateListItem ) );
					prvAddTaskToReadyList( pxTCB );
				}
				else
				{
					/* The delayed and ready lists cannot be accessed, so hold
					this task pending until the scheduler is resumed. */
					vListInsertEnd( &( xPendingReadyList ), &( pxTCB->xEventListItem ) );
				}

				if( pxTCB->uxPriority > pxCurrentTCB->uxPriority )
				{
					/* The notified task has a priority above the currently
					executing task so a yield is required. */
					if( pxHigherPriorityTaskWoken != NULL )
					{
						*pxHigherPriorityTaskWoken = pdTRUE;
					}
					else
					{
						/* Mark that a yield is pending in case the user is not
						using the "xHigherPriorityTaskWoken" parameter to an ISR
						safe FreeRTOS function. */
						xYieldPending = pdTRUE;
					}
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
		}
		portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

		return xReturn;
	}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/
/*任务通知值发送函数（用于中断中且通知用作信号量）*/
#if( configUSE_TASK_NOTIFICATIONS == 1 )
	void vTaskNotifyGiveFromISR( TaskHandle_t xTaskToNotify, BaseType_t *pxHigherPriorityTaskWoken )
	{
	TCB_t * pxTCB;
	uint8_t ucOriginalNotifyState;
	UBaseType_t uxSavedInterruptStatus;

		configASSERT( xTaskToNotify );

		/* RTOS ports that support interrupt nesting have the concept of a
		maximum	system call (or maximum API call) interrupt priority.
		Interrupts that are	above the maximum system call priority are keep
		permanently enabled, even when the RTOS kernel is in a critical section,
		but cannot make any calls to FreeRTOS API functions.  If configASSERT()
		is defined in FreeRTOSConfig.h then
		portASSERT_IF_INTERRUPT_PRIORITY_INVALID() will result in an assertion
		failure if a FreeRTOS API function is called from an interrupt that has
		been assigned a priority above the configured maximum system call
		priority.  Only FreeRTOS functions that end in FromISR can be called
		from interrupts	that have been assigned a priority at or (logically)
		below the maximum system call interrupt priority.  FreeRTOS maintains a
		separate interrupt safe API to ensure interrupt entry is as fast and as
		simple as possible.  More information (albeit Cortex-M specific) is
		provided on the following link:
		http://www.freertos.org/RTOS-Cortex-M3-M4.html */
		portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

		pxTCB = ( TCB_t * ) xTaskToNotify;

		uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
		{
			ucOriginalNotifyState = pxTCB->ucNotifyState;
			pxTCB->ucNotifyState = taskNOTIFICATION_RECEIVED;

			/* 'Giving' is equivalent to incrementing a count in a counting
			semaphore. */
			( pxTCB->ulNotifiedValue )++;

			traceTASK_NOTIFY_GIVE_FROM_ISR();

			/* If the task is in the blocked state specifically to wait for a
			notification then unblock it now. */
			if( ucOriginalNotifyState == taskWAITING_NOTIFICATION )
			{
				/* The task should not have been on an event list. */
				configASSERT( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) == NULL );

				if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
				{
					( void ) uxListRemove( &( pxTCB->xStateListItem ) );
					prvAddTaskToReadyList( pxTCB );
				}
				else
				{
					/* The delayed and ready lists cannot be accessed, so hold
					this task pending until the scheduler is resumed. */
					vListInsertEnd( &( xPendingReadyList ), &( pxTCB->xEventListItem ) );
				}

				if( pxTCB->uxPriority > pxCurrentTCB->uxPriority )
				{
					/* The notified task has a priority above the currently
					executing task so a yield is required. */
					if( pxHigherPriorityTaskWoken != NULL )
					{
						*pxHigherPriorityTaskWoken = pdTRUE;
					}
					else
					{
						/* Mark that a yield is pending in case the user is not
						using the "xHigherPriorityTaskWoken" parameter in an ISR
						safe FreeRTOS function. */
						xYieldPending = pdTRUE;
					}
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
		}
		portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );
	}

#endif /* configUSE_TASK_NOTIFICATIONS */

/*-----------------------------------------------------------*/
/*该函数用于将任务的通知状态设置为taskNOT_WAITING_NOTIFICATION*/
#if( configUSE_TASK_NOTIFICATIONS == 1 )
	BaseType_t xTaskNotifyStateClear( TaskHandle_t xTask )
	{
	TCB_t *pxTCB;
	BaseType_t xReturn;

		/* If null is passed in here then it is the calling task that is having
		its notification state cleared. */
		pxTCB = prvGetTCBFromHandle( xTask );

		taskENTER_CRITICAL();
		{
			if( pxTCB->ucNotifyState == taskNOTIFICATION_RECEIVED )
			{
				pxTCB->ucNotifyState = taskNOT_WAITING_NOTIFICATION;
				xReturn = pdPASS;
			}
			else
			{
				xReturn = pdFAIL;
			}
		}
		taskEXIT_CRITICAL();

		return xReturn;
	}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

/*该函数用于将当前任务插入到延迟链表中*/
static void prvAddCurrentTaskToDelayedList( TickType_t xTicksToWait, const BaseType_t xCanBlockIndefinitely )
{
TickType_t xTimeToWake;
const TickType_t xConstTickCount = xTickCount;

	#if( INCLUDE_xTaskAbortDelay == 1 )
	{
		/* About to enter a delayed list, so ensure the ucDelayAborted flag is
		reset to pdFALSE so it can be detected as having been set to pdTRUE
		when the task leaves the Blocked state. */
		pxCurrentTCB->ucDelayAborted = pdFALSE;
	}
	#endif

	/* 先将任务从就绪列表中移除，然后再将其添加到延时列表，因为两个列表使用相同的列表项 */
	if( uxListRemove( &( pxCurrentTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
	{
		/* 若返回值为0，则当前就绪链表已被清空，所以要更新优先级位图 */
		portRESET_READY_PRIORITY( pxCurrentTCB->uxPriority, uxTopReadyPriority );
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

	#if ( INCLUDE_vTaskSuspend == 1 )
	{
		if( ( xTicksToWait == portMAX_DELAY ) && ( xCanBlockIndefinitely != pdFALSE ) )
		{
			/* 将任务添加到挂起任务列表，而不是延迟任务列表，以确保它不会被定时事件唤醒。它将无限期地阻塞即被挂起。 */
			vListInsertEnd( &xSuspendedTaskList, &( pxCurrentTCB->xStateListItem ) );
		}
		else
		{
			/* 计算应唤醒任务的时间，这可能会溢出 */
			xTimeToWake = xConstTickCount + xTicksToWait;

			/* 列表项将按照唤醒时间顺序插入 */
			listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xStateListItem ), xTimeToWake );

			if( xTimeToWake < xConstTickCount )
			{
				/* 结果溢出，则插入溢出延迟链表 */
				vListInsert( pxOverflowDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );
			}
			else
			{
				/* 结果没有溢出，插入当前延迟链表 */
				vListInsert( pxDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );

				/* 更新下一个任务唤醒时间xNextTaskUnblockTime的值 */
				if( xTimeToWake < xNextTaskUnblockTime )
				{
					xNextTaskUnblockTime = xTimeToWake;
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
		}
	}
	#else /* INCLUDE_vTaskSuspend *//*下面是系统没有开启挂起任务功能时的情况*/
	{
		/* Calculate the time at which the task should be woken if the event
		does not occur.  This may overflow but this doesn't matter, the kernel
		will manage it correctly. */
		xTimeToWake = xConstTickCount + xTicksToWait;

		/* The list item will be inserted in wake time order. */
		listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xStateListItem ), xTimeToWake );

		if( xTimeToWake < xConstTickCount )
		{
			/* Wake time has overflowed.  Place this item in the overflow list. */
			vListInsert( pxOverflowDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );
		}
		else
		{
			/* The wake time has not overflowed, so the current block list is used. */
			vListInsert( pxDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );

			/* If the task entering the blocked state was placed at the head of the
			list of blocked tasks then xNextTaskUnblockTime needs to be updated
			too. */
			if( xTimeToWake < xNextTaskUnblockTime )
			{
				xNextTaskUnblockTime = xTimeToWake;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}

		/* Avoid compiler warning when INCLUDE_vTaskSuspend is not 1. */
		( void ) xCanBlockIndefinitely;
	}
	#endif /* INCLUDE_vTaskSuspend */
}


#ifdef FREERTOS_MODULE_TEST
	#include "tasks_test_access_functions.h"
#endif

