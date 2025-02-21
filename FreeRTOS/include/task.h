/*
    FreeRTOS V9.0.0 - Copyright (C) 2016 Real Time Engineers Ltd.
    All rights reserved
*/


#ifndef INC_TASK_H
#define INC_TASK_H

#ifndef INC_FREERTOS_H
	#error "include FreeRTOS.h must appear in source files before include task.h"
#endif

#include "list.h"

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------
 * MACROS AND DEFINITIONS
 *----------------------------------------------------------*/

#define tskKERNEL_VERSION_NUMBER "V9.0.0"
#define tskKERNEL_VERSION_MAJOR 9
#define tskKERNEL_VERSION_MINOR 0
#define tskKERNEL_VERSION_BUILD 0

/**
 * task. h
 *
 * Type by which tasks are referenced.  For example, a call to xTaskCreate
 * returns (via a pointer parameter) an TaskHandle_t variable that can then
 * be used as a parameter to vTaskDelete to delete the task.
 *
 * \defgroup TaskHandle_t TaskHandle_t
 * \ingroup Tasks
 */
typedef void * TaskHandle_t;

/*
 * Defines the prototype to which the application task hook function must
 * conform.
 */
typedef BaseType_t (*TaskHookFunction_t)( void * );

/* Task states returned by eTaskGetState. */
typedef enum
{
	eRunning = 0,	/* A task is querying the state of itself, so must be running. */
	eReady,			/* The task being queried is in a read or pending ready list. */
	eBlocked,		/* The task being queried is in the Blocked state. */
	eSuspended,		/* The task being queried is in the Suspended state, or is in the Blocked state with an infinite time out. */
	eDeleted,		/* The task being queried has been deleted, but its TCB has not yet been freed. */
	eInvalid			/* Used as an 'invalid state' value. */
} eTaskState;

/* 用于在vTaskNotify()的传入参数，表明任务通知值的设置方式 */
typedef enum
{
	eNoAction = 0,				/* 不更新任务通知值 */
	eSetBits,					/* 按位设置，此时任务通知相当于事件 */
	eIncrement,					/* 增加任务通知值，此时任务通知相当于信号量 */
	eSetValueWithOverwrite,		/* 即使没有读取原本的任务通知值也会覆盖写入，此时任务通知相当于消息，但是这里的消息大小只能是4字节 */
	eSetValueWithoutOverwrite	/* 必须在读取原本的任务通知值之后才能写入 */
} eNotifyAction;

/*
 * 用于消息队列发送消息时等待时间的结构体
 */
typedef struct xTIME_OUT
{
	BaseType_t xOverflowCount;	//系统当前tick计数溢出的次数
	TickType_t xTimeOnEntering;	//系统当前的tick计数值
} TimeOut_t;

/*
 * Defines the memory ranges allocated to the task when an MPU is used.
 */
typedef struct xMEMORY_REGION
{
	void *pvBaseAddress;
	uint32_t ulLengthInBytes;
	uint32_t ulParameters;
} MemoryRegion_t;

/*
 * Parameters required to create an MPU protected task.
 */
typedef struct xTASK_PARAMETERS
{
	TaskFunction_t pvTaskCode;
	const char * const pcName;	/*lint !e971 Unqualified char types are allowed for strings and single characters only. */
	uint16_t usStackDepth;
	void *pvParameters;
	UBaseType_t uxPriority;
	StackType_t *puxStackBuffer;
	MemoryRegion_t xRegions[ portNUM_CONFIGURABLE_REGIONS ];
} TaskParameters_t;

/* Used with the uxTaskGetSystemState() function to return the state of each task
in the system. */
typedef struct xTASK_STATUS
{
	TaskHandle_t xHandle;			/* The handle of the task to which the rest of the information in the structure relates. */
	const char *pcTaskName;			/* A pointer to the task's name.  This value will be invalid if the task was deleted since the structure was populated! */ /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
	UBaseType_t xTaskNumber;		/* A number unique to the task. */
	eTaskState eCurrentState;		/* The state in which the task existed when the structure was populated. */
	UBaseType_t uxCurrentPriority;	/* The priority at which the task was running (may be inherited) when the structure was populated. */
	UBaseType_t uxBasePriority;		/* The priority to which the task will return if the task's current priority has been inherited to avoid unbounded priority inversion when obtaining a mutex.  Only valid if configUSE_MUTEXES is defined as 1 in FreeRTOSConfig.h. */
	uint32_t ulRunTimeCounter;		/* The total run time allocated to the task so far, as defined by the run time stats clock.  See http://www.freertos.org/rtos-run-time-stats.html.  Only valid when configGENERATE_RUN_TIME_STATS is defined as 1 in FreeRTOSConfig.h. */
	StackType_t *pxStackBase;		/* Points to the lowest address of the task's stack area. */
	uint16_t usStackHighWaterMark;	/* The minimum amount of stack space that has remained for the task since the task was created.  The closer this value is to zero the closer the task has come to overflowing its stack. */
} TaskStatus_t;

/* Possible return values for eTaskConfirmSleepModeStatus(). */
typedef enum
{
	eAbortSleep = 0,		/* A task has been made ready or a context switch pended since portSUPPORESS_TICKS_AND_SLEEP() was called - abort entering a sleep mode. */
	eStandardSleep,			/* Enter a sleep mode that will not last any longer than the expected idle time. */
	eNoTasksWaitingTimeout	/* No tasks are waiting for a timeout so it is safe to enter a sleep mode that can only be exited by an external interrupt. */
} eSleepModeStatus;

/**
 * Defines the priority used by the idle task.  This must not be modified.
 *
 * \ingroup TaskUtils
 */
#define tskIDLE_PRIORITY			( ( UBaseType_t ) 0U )

/**
 * task. h
 *
 * Macro for forcing a context switch.
 *
 * \defgroup taskYIELD taskYIELD
 * \ingroup SchedulerControl
 */
#define taskYIELD()					portYIELD()

/**
 * task. h
 *
 * Macro to mark the start of a critical code region.  Preemptive context
 * switches cannot occur when in a critical region.
 *
 * NOTE: This may alter the stack (depending on the portable implementation)
 * so must be used with care!
 *
 * \defgroup taskENTER_CRITICAL taskENTER_CRITICAL
 * \ingroup SchedulerControl
 */
#define taskENTER_CRITICAL()		portENTER_CRITICAL()
#define taskENTER_CRITICAL_FROM_ISR() portSET_INTERRUPT_MASK_FROM_ISR()

/**
 * task. h
 *
 * Macro to mark the end of a critical code region.  Preemptive context
 * switches cannot occur when in a critical region.
 *
 * NOTE: This may alter the stack (depending on the portable implementation)
 * so must be used with care!
 *
 * \defgroup taskEXIT_CRITICAL taskEXIT_CRITICAL
 * \ingroup SchedulerControl
 */
#define taskEXIT_CRITICAL()			portEXIT_CRITICAL()
#define taskEXIT_CRITICAL_FROM_ISR( x ) portCLEAR_INTERRUPT_MASK_FROM_ISR( x )
/**
 * task. h
 *
 * Macro to disable all maskable interrupts.
 *
 * \defgroup taskDISABLE_INTERRUPTS taskDISABLE_INTERRUPTS
 * \ingroup SchedulerControl
 */
#define taskDISABLE_INTERRUPTS()	portDISABLE_INTERRUPTS()

/**
 * task. h
 *
 * Macro to enable microcontroller interrupts.
 *
 * \defgroup taskENABLE_INTERRUPTS taskENABLE_INTERRUPTS
 * \ingroup SchedulerControl
 */
#define taskENABLE_INTERRUPTS()		portENABLE_INTERRUPTS()

/* Definitions returned by xTaskGetSchedulerState().  taskSCHEDULER_SUSPENDED is
0 to generate more optimal code when configASSERT() is defined as the constant
is used in assert() statements. */
#define taskSCHEDULER_SUSPENDED		( ( BaseType_t ) 0 )//表示任务调度器处于挂起状态。当调度器被挂起时，任务切换被暂停，且不再响应新的任务请求。这个状态可以通过调用 vTaskSuspendAll() 来实现。
#define taskSCHEDULER_NOT_STARTED	( ( BaseType_t ) 1 )//表示调度器尚未启动。通常在系统初始化阶段，调度器还没有调用 vTaskStartScheduler() 启动时，系统处于这个状态。在此状态下，任务调度尚未生效。
#define taskSCHEDULER_RUNNING		( ( BaseType_t ) 2 )//表示调度器正在运行。此状态下，FreeRTOS 的任务调度器已经启动并正常运行，任务可以按照优先级和时间片轮转执行。


/*-----------------------------------------------------------
 * TASK CREATION API
 *----------------------------------------------------------*/

/**
 * 动态任务创建函数：创建一个新任务并将其加入任务就绪列表
 *
 * @param pvTaskCode 指向任务函数的指针
 *
 * @param pcName 任务名，任务名字符串的最大长度由FreeRTOSConfig.h文件中的configMAX_TASK_NAME_LEN定义
 *
 * @param ulStackDepth 任务栈的大小，单位为字
 *
 * @param pvParameters 指向任务函数的形参
 *
 * @param uxPriority 任务优先级
 *
 * @param pxCreatedTask 任务句柄
 *
 * @return 创建成功返回pdPASS失败返回errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY
 */
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
	BaseType_t xTaskCreate(	TaskFunction_t pxTaskCode,
							const char * const pcName,
							const uint16_t usStackDepth,
							void * const pvParameters,
							UBaseType_t uxPriority,
							TaskHandle_t * const pxCreatedTask ) PRIVILEGED_FUNCTION; /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
#endif

/**
 * 静态任务创建函数：创建一个新任务并将其加入任务就绪列表
 *
 * @param pvTaskCode 指向任务函数的指针
 *
 * @param pcName 任务名，任务名字符串的最大长度由FreeRTOSConfig.h文件中的configMAX_TASK_NAME_LEN定义
 *
 * @param ulStackDepth 任务栈的大小，单位为字
 *
 * @param pvParameters 指向任务函数的形参
 *
 * @param uxPriority 任务优先级
 *
 * @param pxStackBuffer 任务栈起始地址
 *
 * @param pxTaskBuffer 任务控制块指针
 *
 * @return 创建成功返回指向新创建任务的任务控制块TCB的指针
 */
#if( configSUPPORT_STATIC_ALLOCATION == 1 )
	TaskHandle_t xTaskCreateStatic(	TaskFunction_t pxTaskCode,
									const char * const pcName,
									const uint32_t ulStackDepth,
									void * const pvParameters,
									UBaseType_t uxPriority,
									StackType_t * const puxStackBuffer,
									StaticTask_t * const pxTaskBuffer ) PRIVILEGED_FUNCTION; /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
#endif /* configSUPPORT_STATIC_ALLOCATION */

/**
 * task. h
 *<pre>
 BaseType_t xTaskCreateRestricted( TaskParameters_t *pxTaskDefinition, TaskHandle_t *pxCreatedTask );</pre>
 *
 * xTaskCreateRestricted() should only be used in systems that include an MPU
 * implementation.
 *
 * Create a new task and add it to the list of tasks that are ready to run.
 * The function parameters define the memory regions and associated access
 * permissions allocated to the task.
 *
 * @param pxTaskDefinition Pointer to a structure that contains a member
 * for each of the normal xTaskCreate() parameters (see the xTaskCreate() API
 * documentation) plus an optional stack buffer and the memory region
 * definitions.
 *
 * @param pxCreatedTask Used to pass back a handle by which the created task
 * can be referenced.
 *
 * @return pdPASS if the task was successfully created and added to a ready
 * list, otherwise an error code defined in the file projdefs.h
 *
 * Example usage:
   <pre>
// Create an TaskParameters_t structure that defines the task to be created.
static const TaskParameters_t xCheckTaskParameters =
{
	vATask,		// pvTaskCode - the function that implements the task.
	"ATask",	// pcName - just a text name for the task to assist debugging.
	100,		// usStackDepth	- the stack size DEFINED IN WORDS.
	NULL,		// pvParameters - passed into the task function as the function parameters.
	( 1UL | portPRIVILEGE_BIT ),// uxPriority - task priority, set the portPRIVILEGE_BIT if the task should run in a privileged state.
	cStackBuffer,// puxStackBuffer - the buffer to be used as the task stack.

	// xRegions - Allocate up to three separate memory regions for access by
	// the task, with appropriate access permissions.  Different processors have
	// different memory alignment requirements - refer to the FreeRTOS documentation
	// for full information.
	{
		// Base address					Length	Parameters
        { cReadWriteArray,				32,		portMPU_REGION_READ_WRITE },
        { cReadOnlyArray,				32,		portMPU_REGION_READ_ONLY },
        { cPrivilegedOnlyAccessArray,	128,	portMPU_REGION_PRIVILEGED_READ_WRITE }
	}
};

int main( void )
{
TaskHandle_t xHandle;

	// Create a task from the const structure defined above.  The task handle
	// is requested (the second parameter is not NULL) but in this case just for
	// demonstration purposes as its not actually used.
	xTaskCreateRestricted( &xRegTest1Parameters, &xHandle );

	// Start the scheduler.
	vTaskStartScheduler();

	// Will only get here if there was insufficient memory to create the idle
	// and/or timer task.
	for( ;; );
}
   </pre>
 * \defgroup xTaskCreateRestricted xTaskCreateRestricted
 * \ingroup Tasks
 */
#if( portUSING_MPU_WRAPPERS == 1 )
	BaseType_t xTaskCreateRestricted( const TaskParameters_t * const pxTaskDefinition, TaskHandle_t *pxCreatedTask ) PRIVILEGED_FUNCTION;
#endif

/**
 * task. h
 *<pre>
 void vTaskAllocateMPURegions( TaskHandle_t xTask, const MemoryRegion_t * const pxRegions );</pre>
 *
 * Memory regions are assigned to a restricted task when the task is created by
 * a call to xTaskCreateRestricted().  These regions can be redefined using
 * vTaskAllocateMPURegions().
 *
 * @param xTask The handle of the task being updated.
 *
 * @param xRegions A pointer to an MemoryRegion_t structure that contains the
 * new memory region definitions.
 *
 * Example usage:
   <pre>
// Define an array of MemoryRegion_t structures that configures an MPU region
// allowing read/write access for 1024 bytes starting at the beginning of the
// ucOneKByte array.  The other two of the maximum 3 definable regions are
// unused so set to zero.
static const MemoryRegion_t xAltRegions[ portNUM_CONFIGURABLE_REGIONS ] =
{
	// Base address		Length		Parameters
	{ ucOneKByte,		1024,		portMPU_REGION_READ_WRITE },
	{ 0,				0,			0 },
	{ 0,				0,			0 }
};

void vATask( void *pvParameters )
{
	// This task was created such that it has access to certain regions of
	// memory as defined by the MPU configuration.  At some point it is
	// desired that these MPU regions are replaced with that defined in the
	// xAltRegions const struct above.  Use a call to vTaskAllocateMPURegions()
	// for this purpose.  NULL is used as the task handle to indicate that this
	// function should modify the MPU regions of the calling task.
	vTaskAllocateMPURegions( NULL, xAltRegions );

	// Now the task can continue its function, but from this point on can only
	// access its stack and the ucOneKByte array (unless any other statically
	// defined or shared regions have been declared elsewhere).
}
   </pre>
 * \defgroup xTaskCreateRestricted xTaskCreateRestricted
 * \ingroup Tasks
 */
void vTaskAllocateMPURegions( TaskHandle_t xTask, const MemoryRegion_t * const pxRegions ) PRIVILEGED_FUNCTION;

/**
 * 任务删除函数，xTaskToDelete是待删除任务的句柄，若传入NULL则会删除当前正在运行的任务pxCurrentTCB
 */
void vTaskDelete( TaskHandle_t xTaskToDelete ) PRIVILEGED_FUNCTION;

/*-----------------------------------------------------------
 * TASK CONTROL API
 *----------------------------------------------------------*/

/**
 * 相对时间任务阻塞函数，将当前任务相对于当前系统时间xTickCount阻塞xTicksToDelay
 */
void vTaskDelay( const TickType_t xTicksToDelay ) PRIVILEGED_FUNCTION;

/**
 * 绝对时间任务阻塞函数，将当前任务相对于其上一次阻塞时间pxPreviousWakeTime阻塞xTimeIncrement
 */
void vTaskDelayUntil( TickType_t * const pxPreviousWakeTime, const TickType_t xTimeIncrement ) PRIVILEGED_FUNCTION;

/**
 * task. h
 * <pre>BaseType_t xTaskAbortDelay( TaskHandle_t xTask );</pre>
 *
 * INCLUDE_xTaskAbortDelay must be defined as 1 in FreeRTOSConfig.h for this
 * function to be available.
 *
 * A task will enter the Blocked state when it is waiting for an event.  The
 * event it is waiting for can be a temporal event (waiting for a time), such
 * as when vTaskDelay() is called, or an event on an object, such as when
 * xQueueReceive() or ulTaskNotifyTake() is called.  If the handle of a task
 * that is in the Blocked state is used in a call to xTaskAbortDelay() then the
 * task will leave the Blocked state, and return from whichever function call
 * placed the task into the Blocked state.
 *
 * @param xTask The handle of the task to remove from the Blocked state.
 *
 * @return If the task referenced by xTask was not in the Blocked state then
 * pdFAIL is returned.  Otherwise pdPASS is returned.
 *
 * \defgroup xTaskAbortDelay xTaskAbortDelay
 * \ingroup TaskCtrl
 */
BaseType_t xTaskAbortDelay( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;

/**
 * 该函数用于获取目标任务的优先级，传入NULL获取当前任务的优先级
 */
UBaseType_t uxTaskPriorityGet( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;

/**
 * 该函数用于获取目标任务的优先级（用于中断中的），传入NULL获取当前任务的优先级
 */
UBaseType_t uxTaskPriorityGetFromISR( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;

/**
 * 该函数用于获取目标任务的当前状态
 */
eTaskState eTaskGetState( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;

/**
 * task. h
 * <pre>void vTaskGetInfo( TaskHandle_t xTask, TaskStatus_t *pxTaskStatus, BaseType_t xGetFreeStackSpace, eTaskState eState );</pre>
 *
 * configUSE_TRACE_FACILITY must be defined as 1 for this function to be
 * available.  See the configuration section for more information.
 *
 * Populates a TaskStatus_t structure with information about a task.
 *
 * @param xTask Handle of the task being queried.  If xTask is NULL then
 * information will be returned about the calling task.
 *
 * @param pxTaskStatus A pointer to the TaskStatus_t structure that will be
 * filled with information about the task referenced by the handle passed using
 * the xTask parameter.
 *
 * @xGetFreeStackSpace The TaskStatus_t structure contains a member to report
 * the stack high water mark of the task being queried.  Calculating the stack
 * high water mark takes a relatively long time, and can make the system
 * temporarily unresponsive - so the xGetFreeStackSpace parameter is provided to
 * allow the high water mark checking to be skipped.  The high watermark value
 * will only be written to the TaskStatus_t structure if xGetFreeStackSpace is
 * not set to pdFALSE;
 *
 * @param eState The TaskStatus_t structure contains a member to report the
 * state of the task being queried.  Obtaining the task state is not as fast as
 * a simple assignment - so the eState parameter is provided to allow the state
 * information to be omitted from the TaskStatus_t structure.  To obtain state
 * information then set eState to eInvalid - otherwise the value passed in
 * eState will be reported as the task state in the TaskStatus_t structure.
 *
 * Example usage:
   <pre>
 void vAFunction( void )
 {
 TaskHandle_t xHandle;
 TaskStatus_t xTaskDetails;

    // Obtain the handle of a task from its name.
    xHandle = xTaskGetHandle( "Task_Name" );

    // Check the handle is not NULL.
    configASSERT( xHandle );

    // Use the handle to obtain further information about the task.
    vTaskGetInfo( xHandle,
                  &xTaskDetails,
                  pdTRUE, // Include the high water mark in xTaskDetails.
                  eInvalid ); // Include the task state in xTaskDetails.
 }
   </pre>
 * \defgroup vTaskGetInfo vTaskGetInfo
 * \ingroup TaskCtrl
 */
void vTaskGetInfo( TaskHandle_t xTask, TaskStatus_t *pxTaskStatus, BaseType_t xGetFreeStackSpace, eTaskState eState ) PRIVILEGED_FUNCTION;

/**
 * 该函数用于获取目标任务的优先级（用于中断中的），传入NULL获取当前任务的优先级
 */
void vTaskPrioritySet( TaskHandle_t xTask, UBaseType_t uxNewPriority ) PRIVILEGED_FUNCTION;

/**
 * 该函数用于将传入的任务挂起，被挂起的任务不会再参与调度，若传入NULL则会挂起当前正在运行的任务pxCurrentTCB
 */
void vTaskSuspend( TaskHandle_t xTaskToSuspend ) PRIVILEGED_FUNCTION;

/**
 * 该函数用于将挂起的任务恢复就绪状态，无论任务被挂起多少次，只需要调用该恢复函数一次即可恢复到就绪态
 */
void vTaskResume( TaskHandle_t xTaskToResume ) PRIVILEGED_FUNCTION;

/**
 * 该函数是可在中断中使用的，将被挂起的任务唤醒并转换为就绪状态的函数
 */
BaseType_t xTaskResumeFromISR( TaskHandle_t xTaskToResume ) PRIVILEGED_FUNCTION;

/*-----------------------------------------------------------
 * SCHEDULER CONTROL
 *----------------------------------------------------------*/

/**
 * 调度器函数，创建一个IDLE任务（在创建任务的函数中会发现这是系统第一个任务所以会将pxCurrentTCB指向IDLE），
 * 该函数会调用xPortStartScheduler函数启动第一个任务，也就是IDLE（如果在调用该函数之前没创建其他任务的话），并开始调度，该函数还会根据配置选择是否创建软件定时器任务。
 */
void vTaskStartScheduler( void ) PRIVILEGED_FUNCTION;

/**
 * task. h
 * <pre>void vTaskEndScheduler( void );</pre>
 *
 * NOTE:  At the time of writing only the x86 real mode port, which runs on a PC
 * in place of DOS, implements this function.
 *
 * Stops the real time kernel tick.  All created tasks will be automatically
 * deleted and multitasking (either preemptive or cooperative) will
 * stop.  Execution then resumes from the point where vTaskStartScheduler ()
 * was called, as if vTaskStartScheduler () had just returned.
 *
 * See the demo application file main. c in the demo/PC directory for an
 * example that uses vTaskEndScheduler ().
 *
 * vTaskEndScheduler () requires an exit function to be defined within the
 * portable layer (see vPortEndScheduler () in port. c for the PC port).  This
 * performs hardware specific operations such as stopping the kernel tick.
 *
 * vTaskEndScheduler () will cause all of the resources allocated by the
 * kernel to be freed - but will not free resources allocated by application
 * tasks.
 *
 * Example usage:
   <pre>
 void vTaskCode( void * pvParameters )
 {
	 for( ;; )
	 {
		 // Task code goes here.

		 // At some point we want to end the real time kernel processing
		 // so call ...
		 vTaskEndScheduler ();
	 }
 }

 void vAFunction( void )
 {
	 // Create at least one task before starting the kernel.
	 xTaskCreate( vTaskCode, "NAME", STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL );

	 // Start the real time kernel with preemption.
	 vTaskStartScheduler ();

	 // Will only get here when the vTaskCode () task has called
	 // vTaskEndScheduler ().  When we get here we are back to single task
	 // execution.
 }
   </pre>
 *
 * \defgroup vTaskEndScheduler vTaskEndScheduler
 * \ingroup SchedulerControl
 */
void vTaskEndScheduler( void ) PRIVILEGED_FUNCTION;

/**
 * 该函数用于暂停调度器
 */
void vTaskSuspendAll( void ) PRIVILEGED_FUNCTION;

/**
 * 该函数用于将调度器重新唤醒,调用了多少次vTaskSuspendAll就要调用多少次xTaskResumeAll才能唤醒调度器
 */
BaseType_t xTaskResumeAll( void ) PRIVILEGED_FUNCTION;

/*-----------------------------------------------------------
 * TASK UTILITIES
 *----------------------------------------------------------*/

/**
 * 该函数返回当前系统的tick计数时间值
 */
TickType_t xTaskGetTickCount( void ) PRIVILEGED_FUNCTION;

/**
 * 该函数返回当前系统的tick计数时间值（用于中断中的）
 */
TickType_t xTaskGetTickCountFromISR( void ) PRIVILEGED_FUNCTION;

/**
 * 该函数用于获取当前系统总任务数
 */
UBaseType_t uxTaskGetNumberOfTasks( void ) PRIVILEGED_FUNCTION;

/**
 * 该函数用于获取任务控制块的名字
 */
char *pcTaskGetName( TaskHandle_t xTaskToQuery ) PRIVILEGED_FUNCTION; /*lint !e971 Unqualified char types are allowed for strings and single characters only. */

/**
 * task. h
 * <PRE>TaskHandle_t xTaskGetHandle( const char *pcNameToQuery );</PRE>
 *
 * NOTE:  This function takes a relatively long time to complete and should be
 * used sparingly.
 *
 * @return The handle of the task that has the human readable name pcNameToQuery.
 * NULL is returned if no matching name is found.  INCLUDE_xTaskGetHandle
 * must be set to 1 in FreeRTOSConfig.h for pcTaskGetHandle() to be available.
 *
 * \defgroup pcTaskGetHandle pcTaskGetHandle
 * \ingroup TaskUtils
 */
TaskHandle_t xTaskGetHandle( const char *pcNameToQuery ) PRIVILEGED_FUNCTION; /*lint !e971 Unqualified char types are allowed for strings and single characters only. */

/**
 * task.h
 * <PRE>UBaseType_t uxTaskGetStackHighWaterMark( TaskHandle_t xTask );</PRE>
 *
 * INCLUDE_uxTaskGetStackHighWaterMark must be set to 1 in FreeRTOSConfig.h for
 * this function to be available.
 *
 * Returns the high water mark of the stack associated with xTask.  That is,
 * the minimum free stack space there has been (in words, so on a 32 bit machine
 * a value of 1 means 4 bytes) since the task started.  The smaller the returned
 * number the closer the task has come to overflowing its stack.
 *
 * @param xTask Handle of the task associated with the stack to be checked.
 * Set xTask to NULL to check the stack of the calling task.
 *
 * @return The smallest amount of free stack space there has been (in words, so
 * actual spaces on the stack rather than bytes) since the task referenced by
 * xTask was created.
 */
UBaseType_t uxTaskGetStackHighWaterMark( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;

/* When using trace macros it is sometimes necessary to include task.h before
FreeRTOS.h.  When this is done TaskHookFunction_t will not yet have been defined,
so the following two prototypes will cause a compilation error.  This can be
fixed by simply guarding against the inclusion of these two prototypes unless
they are explicitly required by the configUSE_APPLICATION_TASK_TAG configuration
constant. */
#ifdef configUSE_APPLICATION_TASK_TAG
	#if configUSE_APPLICATION_TASK_TAG == 1
		/**
		 * task.h
		 * <pre>void vTaskSetApplicationTaskTag( TaskHandle_t xTask, TaskHookFunction_t pxHookFunction );</pre>
		 *
		 * Sets pxHookFunction to be the task hook function used by the task xTask.
		 * Passing xTask as NULL has the effect of setting the calling tasks hook
		 * function.
		 */
		void vTaskSetApplicationTaskTag( TaskHandle_t xTask, TaskHookFunction_t pxHookFunction ) PRIVILEGED_FUNCTION;

		/**
		 * task.h
		 * <pre>void xTaskGetApplicationTaskTag( TaskHandle_t xTask );</pre>
		 *
		 * Returns the pxHookFunction value assigned to the task xTask.
		 */
		TaskHookFunction_t xTaskGetApplicationTaskTag( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;
	#endif /* configUSE_APPLICATION_TASK_TAG ==1 */
#endif /* ifdef configUSE_APPLICATION_TASK_TAG */

#if( configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0 )

	/* Each task contains an array of pointers that is dimensioned by the
	configNUM_THREAD_LOCAL_STORAGE_POINTERS setting in FreeRTOSConfig.h.  The
	kernel does not use the pointers itself, so the application writer can use
	the pointers for any purpose they wish.  The following two functions are
	used to set and query a pointer respectively. */
	void vTaskSetThreadLocalStoragePointer( TaskHandle_t xTaskToSet, BaseType_t xIndex, void *pvValue ) PRIVILEGED_FUNCTION;
	void *pvTaskGetThreadLocalStoragePointer( TaskHandle_t xTaskToQuery, BaseType_t xIndex ) PRIVILEGED_FUNCTION;

#endif

/**
 * task.h
 * <pre>BaseType_t xTaskCallApplicationTaskHook( TaskHandle_t xTask, void *pvParameter );</pre>
 *
 * Calls the hook function associated with xTask.  Passing xTask as NULL has
 * the effect of calling the Running tasks (the calling task) hook function.
 *
 * pvParameter is passed to the hook function for the task to interpret as it
 * wants.  The return value is the value returned by the task hook function
 * registered by the user.
 */
BaseType_t xTaskCallApplicationTaskHook( TaskHandle_t xTask, void *pvParameter ) PRIVILEGED_FUNCTION;

/**
 * xTaskGetIdleTaskHandle() is only available if
 * INCLUDE_xTaskGetIdleTaskHandle is set to 1 in FreeRTOSConfig.h.
 *
 * Simply returns the handle of the idle task.  It is not valid to call
 * xTaskGetIdleTaskHandle() before the scheduler has been started.
 */
TaskHandle_t xTaskGetIdleTaskHandle( void ) PRIVILEGED_FUNCTION;

/**
 * configUSE_TRACE_FACILITY must be defined as 1 in FreeRTOSConfig.h for
 * uxTaskGetSystemState() to be available.
 *
 * uxTaskGetSystemState() populates an TaskStatus_t structure for each task in
 * the system.  TaskStatus_t structures contain, among other things, members
 * for the task handle, task name, task priority, task state, and total amount
 * of run time consumed by the task.  See the TaskStatus_t structure
 * definition in this file for the full member list.
 *
 * NOTE:  This function is intended for debugging use only as its use results in
 * the scheduler remaining suspended for an extended period.
 *
 * @param pxTaskStatusArray A pointer to an array of TaskStatus_t structures.
 * The array must contain at least one TaskStatus_t structure for each task
 * that is under the control of the RTOS.  The number of tasks under the control
 * of the RTOS can be determined using the uxTaskGetNumberOfTasks() API function.
 *
 * @param uxArraySize The size of the array pointed to by the pxTaskStatusArray
 * parameter.  The size is specified as the number of indexes in the array, or
 * the number of TaskStatus_t structures contained in the array, not by the
 * number of bytes in the array.
 *
 * @param pulTotalRunTime If configGENERATE_RUN_TIME_STATS is set to 1 in
 * FreeRTOSConfig.h then *pulTotalRunTime is set by uxTaskGetSystemState() to the
 * total run time (as defined by the run time stats clock, see
 * http://www.freertos.org/rtos-run-time-stats.html) since the target booted.
 * pulTotalRunTime can be set to NULL to omit the total run time information.
 *
 * @return The number of TaskStatus_t structures that were populated by
 * uxTaskGetSystemState().  This should equal the number returned by the
 * uxTaskGetNumberOfTasks() API function, but will be zero if the value passed
 * in the uxArraySize parameter was too small.
 *
 * Example usage:
   <pre>
    // This example demonstrates how a human readable table of run time stats
	// information is generated from raw data provided by uxTaskGetSystemState().
	// The human readable table is written to pcWriteBuffer
	void vTaskGetRunTimeStats( char *pcWriteBuffer )
	{
	TaskStatus_t *pxTaskStatusArray;
	volatile UBaseType_t uxArraySize, x;
	uint32_t ulTotalRunTime, ulStatsAsPercentage;

		// Make sure the write buffer does not contain a string.
		*pcWriteBuffer = 0x00;

		// Take a snapshot of the number of tasks in case it changes while this
		// function is executing.
		uxArraySize = uxTaskGetNumberOfTasks();

		// Allocate a TaskStatus_t structure for each task.  An array could be
		// allocated statically at compile time.
		pxTaskStatusArray = pvPortMalloc( uxArraySize * sizeof( TaskStatus_t ) );

		if( pxTaskStatusArray != NULL )
		{
			// Generate raw status information about each task.
			uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, &ulTotalRunTime );

			// For percentage calculations.
			ulTotalRunTime /= 100UL;

			// Avoid divide by zero errors.
			if( ulTotalRunTime > 0 )
			{
				// For each populated position in the pxTaskStatusArray array,
				// format the raw data as human readable ASCII data
				for( x = 0; x < uxArraySize; x++ )
				{
					// What percentage of the total run time has the task used?
					// This will always be rounded down to the nearest integer.
					// ulTotalRunTimeDiv100 has already been divided by 100.
					ulStatsAsPercentage = pxTaskStatusArray[ x ].ulRunTimeCounter / ulTotalRunTime;

					if( ulStatsAsPercentage > 0UL )
					{
						sprintf( pcWriteBuffer, "%s\t\t%lu\t\t%lu%%\r\n", pxTaskStatusArray[ x ].pcTaskName, pxTaskStatusArray[ x ].ulRunTimeCounter, ulStatsAsPercentage );
					}
					else
					{
						// If the percentage is zero here then the task has
						// consumed less than 1% of the total run time.
						sprintf( pcWriteBuffer, "%s\t\t%lu\t\t<1%%\r\n", pxTaskStatusArray[ x ].pcTaskName, pxTaskStatusArray[ x ].ulRunTimeCounter );
					}

					pcWriteBuffer += strlen( ( char * ) pcWriteBuffer );
				}
			}

			// The array is no longer needed, free the memory it consumes.
			vPortFree( pxTaskStatusArray );
		}
	}
	</pre>
 */
UBaseType_t uxTaskGetSystemState( TaskStatus_t * const pxTaskStatusArray, const UBaseType_t uxArraySize, uint32_t * const pulTotalRunTime ) PRIVILEGED_FUNCTION;

/**
 * task. h
 * <PRE>void vTaskList( char *pcWriteBuffer );</PRE>
 *
 * configUSE_TRACE_FACILITY and configUSE_STATS_FORMATTING_FUNCTIONS must
 * both be defined as 1 for this function to be available.  See the
 * configuration section of the FreeRTOS.org website for more information.
 *
 * NOTE 1: This function will disable interrupts for its duration.  It is
 * not intended for normal application runtime use but as a debug aid.
 *
 * Lists all the current tasks, along with their current state and stack
 * usage high water mark.
 *
 * Tasks are reported as blocked ('B'), ready ('R'), deleted ('D') or
 * suspended ('S').
 *
 * PLEASE NOTE:
 *
 * This function is provided for convenience only, and is used by many of the
 * demo applications.  Do not consider it to be part of the scheduler.
 *
 * vTaskList() calls uxTaskGetSystemState(), then formats part of the
 * uxTaskGetSystemState() output into a human readable table that displays task
 * names, states and stack usage.
 *
 * vTaskList() has a dependency on the sprintf() C library function that might
 * bloat the code size, use a lot of stack, and provide different results on
 * different platforms.  An alternative, tiny, third party, and limited
 * functionality implementation of sprintf() is provided in many of the
 * FreeRTOS/Demo sub-directories in a file called printf-stdarg.c (note
 * printf-stdarg.c does not provide a full snprintf() implementation!).
 *
 * It is recommended that production systems call uxTaskGetSystemState()
 * directly to get access to raw stats data, rather than indirectly through a
 * call to vTaskList().
 *
 * @param pcWriteBuffer A buffer into which the above mentioned details
 * will be written, in ASCII form.  This buffer is assumed to be large
 * enough to contain the generated report.  Approximately 40 bytes per
 * task should be sufficient.
 *
 * \defgroup vTaskList vTaskList
 * \ingroup TaskUtils
 */
void vTaskList( char * pcWriteBuffer ) PRIVILEGED_FUNCTION; /*lint !e971 Unqualified char types are allowed for strings and single characters only. */

/**
 * task. h
 * <PRE>void vTaskGetRunTimeStats( char *pcWriteBuffer );</PRE>
 *
 * configGENERATE_RUN_TIME_STATS and configUSE_STATS_FORMATTING_FUNCTIONS
 * must both be defined as 1 for this function to be available.  The application
 * must also then provide definitions for
 * portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() and portGET_RUN_TIME_COUNTER_VALUE()
 * to configure a peripheral timer/counter and return the timers current count
 * value respectively.  The counter should be at least 10 times the frequency of
 * the tick count.
 *
 * NOTE 1: This function will disable interrupts for its duration.  It is
 * not intended for normal application runtime use but as a debug aid.
 *
 * Setting configGENERATE_RUN_TIME_STATS to 1 will result in a total
 * accumulated execution time being stored for each task.  The resolution
 * of the accumulated time value depends on the frequency of the timer
 * configured by the portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() macro.
 * Calling vTaskGetRunTimeStats() writes the total execution time of each
 * task into a buffer, both as an absolute count value and as a percentage
 * of the total system execution time.
 *
 * NOTE 2:
 *
 * This function is provided for convenience only, and is used by many of the
 * demo applications.  Do not consider it to be part of the scheduler.
 *
 * vTaskGetRunTimeStats() calls uxTaskGetSystemState(), then formats part of the
 * uxTaskGetSystemState() output into a human readable table that displays the
 * amount of time each task has spent in the Running state in both absolute and
 * percentage terms.
 *
 * vTaskGetRunTimeStats() has a dependency on the sprintf() C library function
 * that might bloat the code size, use a lot of stack, and provide different
 * results on different platforms.  An alternative, tiny, third party, and
 * limited functionality implementation of sprintf() is provided in many of the
 * FreeRTOS/Demo sub-directories in a file called printf-stdarg.c (note
 * printf-stdarg.c does not provide a full snprintf() implementation!).
 *
 * It is recommended that production systems call uxTaskGetSystemState() directly
 * to get access to raw stats data, rather than indirectly through a call to
 * vTaskGetRunTimeStats().
 *
 * @param pcWriteBuffer A buffer into which the execution times will be
 * written, in ASCII form.  This buffer is assumed to be large enough to
 * contain the generated report.  Approximately 40 bytes per task should
 * be sufficient.
 *
 * \defgroup vTaskGetRunTimeStats vTaskGetRunTimeStats
 * \ingroup TaskUtils
 */
void vTaskGetRunTimeStats( char *pcWriteBuffer ) PRIVILEGED_FUNCTION; /*lint !e971 Unqualified char types are allowed for strings and single characters only. */

/**
 * 该函数用于给任务发送通知，通知可以用于代替消息（只有四字节）、事件、信号量等机制实现任务间的通信，宏定义xTaskNotify用于给任务发送通知，宏定义xTaskNotifyAndQuery用于给任务发送通知并获取之前的通知值
 */
BaseType_t xTaskGenericNotify( TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction, uint32_t *pulPreviousNotificationValue ) PRIVILEGED_FUNCTION;
#define xTaskNotify( xTaskToNotify, ulValue, eAction ) xTaskGenericNotify( ( xTaskToNotify ), ( ulValue ), ( eAction ), NULL )
#define xTaskNotifyAndQuery( xTaskToNotify, ulValue, eAction, pulPreviousNotifyValue ) xTaskGenericNotify( ( xTaskToNotify ), ( ulValue ), ( eAction ), ( pulPreviousNotifyValue ) )

/**
 * 该函数用于给任务发送通知（用于中断中），通知可以用于代替消息（只有四字节）、事件、信号量等机制实现任务间的通信，宏定义xTaskNotifyFromISR用于给任务发送通知（用于中断中），宏定义xTaskNotifyAndQueryFromISR用于给任务发送通知并获取之前的通知值（用于中断中）
 */
BaseType_t xTaskGenericNotifyFromISR( TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction, uint32_t *pulPreviousNotificationValue, BaseType_t *pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;
#define xTaskNotifyFromISR( xTaskToNotify, ulValue, eAction, pxHigherPriorityTaskWoken ) xTaskGenericNotifyFromISR( ( xTaskToNotify ), ( ulValue ), ( eAction ), NULL, ( pxHigherPriorityTaskWoken ) )
#define xTaskNotifyAndQueryFromISR( xTaskToNotify, ulValue, eAction, pulPreviousNotificationValue, pxHigherPriorityTaskWoken ) xTaskGenericNotifyFromISR( ( xTaskToNotify ), ( ulValue ), ( eAction ), ( pulPreviousNotificationValue ), ( pxHigherPriorityTaskWoken ) )

/**
 * 任务通知值获取函数（该函数一般用于将任务通知用作事件或消息时），因为任务通知值只属于任务本身，所以它是私有的，ulBitsToClearOnEntry用来设置进入时需要清除的位，ulBitsToClearOnExit用来设置退出时需要清除的位，
 * pulNotificationValue获取原本的任务通知值，xTicksToWait设置阻塞时间
 */
BaseType_t xTaskNotifyWait( uint32_t ulBitsToClearOnEntry, uint32_t ulBitsToClearOnExit, uint32_t *pulNotificationValue, TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

/**
 * 该宏定义用于向任务发送通知（将通知用作信号量时）
 */
#define xTaskNotifyGive( xTaskToNotify ) xTaskGenericNotify( ( xTaskToNotify ), ( 0 ), eIncrement, NULL )

/**
 * 任务通知值发送函数（用于中断中且通知用作信号量）
 */
void vTaskNotifyGiveFromISR( TaskHandle_t xTaskToNotify, BaseType_t *pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;

/**
 * 任务通知值获取函数（用于信号量），因为任务通知值只属于任务本身，所以它是私有的，xClearCountOnExit设置读取后是否清0，xTicksToWait设置阻塞时间
 */
uint32_t ulTaskNotifyTake( BaseType_t xClearCountOnExit, TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

/**
 * 该函数用于将任务的通知状态设置为taskNOT_WAITING_NOTIFICATION
 */
BaseType_t xTaskNotifyStateClear( TaskHandle_t xTask );

/*-----------------------------------------------------------
 * SCHEDULER INTERNALS AVAILABLE FOR PORTING PURPOSES
 *----------------------------------------------------------*/

/*
 * 该函数用于增加tick计数，时钟中断来临时，systick中断处理函数会调用该函数，视情况选择是否设置portNVIC_PENDSVSET_BIT进行任务调度
 */
BaseType_t xTaskIncrementTick( void ) PRIVILEGED_FUNCTION;

/*
 * 该函数用于将当前任务插入等待目标事件（可以是消息、信号量、事件组等）的链表，并将当前任务加入延时链表，任务可以被设置成永久阻塞地等待事件
 */
void vTaskPlaceOnEventList( List_t * const pxEventList, const TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

/*
 * 该函数用于将当前任务加入到事件组（这里等待的事件是事件组）的阻塞等待链表中，并将当前任务加入到延时链表中
 */
void vTaskPlaceOnUnorderedEventList( List_t * pxEventList, const TickType_t xItemValue, const TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

/*
 * 该函数用于将当前任务加入指定的事件链表，并将当前任务加入延时链表，xWaitIndefinitely为pdTRUE则会无限阻塞
 */
void vTaskPlaceOnEventListRestricted( List_t * const pxEventList, TickType_t xTicksToWait, const BaseType_t xWaitIndefinitely ) PRIVILEGED_FUNCTION;

/*
 * 该函数用于将pxEventList第一个链表项移出并加入就绪队列（也即优先级最高的那个链表项）
 */
BaseType_t xTaskRemoveFromEventList( const List_t * const pxEventList ) PRIVILEGED_FUNCTION;

/*
 * 该函数用于将阻塞在事件组（这里等待的事件是事件组）上的任务移除，并将其从延迟链表移除并添加到就绪链表
 */
BaseType_t xTaskRemoveFromUnorderedEventList( ListItem_t * pxEventListItem, const TickType_t xItemValue ) PRIVILEGED_FUNCTION;

/*
 * 任务切换函数，调度器不在运行则不允许进行上下文切换
 */
void vTaskSwitchContext( void ) PRIVILEGED_FUNCTION;

/*
 * 该函数用于重新设置任务控制块中事件链表项的值，将其设置为( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) pxCurrentTCB->uxPriority，说明此时已经不再用于等待事件了，返回值为当前任务等待的目标事件
 */
TickType_t uxTaskResetEventItemValue( void ) PRIVILEGED_FUNCTION;

/*
 * 该函数用于获取当前正在运行的任务
 */
TaskHandle_t xTaskGetCurrentTaskHandle( void ) PRIVILEGED_FUNCTION;

/*
 * 该函数用于设置等待时间的结构体
 */
void vTaskSetTimeOutState( TimeOut_t * const pxTimeOut ) PRIVILEGED_FUNCTION;

/*
 * 该函数用于检查任务等待消息队列满足要求的等待时间是否到达，pxTimeOut是之前记录的时间结构体，pxTicksToWait是可以等待的时间，返回pdTRUE表示到达时间了
 */
BaseType_t xTaskCheckForTimeOut( TimeOut_t * const pxTimeOut, TickType_t * const pxTicksToWait ) PRIVILEGED_FUNCTION;

/*
 * Shortcut used by the queue implementation to prevent unnecessary call to
 * taskYIELD();
 */
void vTaskMissedYield( void ) PRIVILEGED_FUNCTION;

/*
 * 该函数用于获取系统调度器的状态
 */
BaseType_t xTaskGetSchedulerState( void ) PRIVILEGED_FUNCTION;

/*
 * 该函数用于实现互斥量的优先级翻转机制
 */
void vTaskPriorityInherit( TaskHandle_t const pxMutexHolder ) PRIVILEGED_FUNCTION;

/*
 * 该函数用于将之前的优先级继承恢复
 */
BaseType_t xTaskPriorityDisinherit( TaskHandle_t const pxMutexHolder ) PRIVILEGED_FUNCTION;

/*
 * Get the uxTCBNumber assigned to the task referenced by the xTask parameter.
 */
UBaseType_t uxTaskGetTaskNumber( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;

/*
 * Set the uxTaskNumber of the task referenced by the xTask parameter to
 * uxHandle.
 */
void vTaskSetTaskNumber( TaskHandle_t xTask, const UBaseType_t uxHandle ) PRIVILEGED_FUNCTION;

/*
 * Only available when configUSE_TICKLESS_IDLE is set to 1.
 * If tickless mode is being used, or a low power mode is implemented, then
 * the tick interrupt will not execute during idle periods.  When this is the
 * case, the tick count value maintained by the scheduler needs to be kept up
 * to date with the actual execution time by being skipped forward by a time
 * equal to the idle period.
 */
void vTaskStepTick( const TickType_t xTicksToJump ) PRIVILEGED_FUNCTION;

/*
 * Only avilable when configUSE_TICKLESS_IDLE is set to 1.
 * Provided for use within portSUPPRESS_TICKS_AND_SLEEP() to allow the port
 * specific sleep function to determine if it is ok to proceed with the sleep,
 * and if it is ok to proceed, if it is ok to sleep indefinitely.
 *
 * This function is necessary because portSUPPRESS_TICKS_AND_SLEEP() is only
 * called with the scheduler suspended, not from within a critical section.  It
 * is therefore possible for an interrupt to request a context switch between
 * portSUPPRESS_TICKS_AND_SLEEP() and the low power mode actually being
 * entered.  eTaskConfirmSleepModeStatus() should be called from a short
 * critical section between the timer being stopped and the sleep mode being
 * entered to ensure it is ok to proceed into the sleep mode.
 */
eSleepModeStatus eTaskConfirmSleepModeStatus( void ) PRIVILEGED_FUNCTION;

/*
 * 该函数用将当前正在运行任务持有的互斥量加1
 */
void *pvTaskIncrementMutexHeldCount( void ) PRIVILEGED_FUNCTION;

#ifdef __cplusplus
}
#endif
#endif /* INC_TASK_H */



