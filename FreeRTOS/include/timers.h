/*
    FreeRTOS V9.0.0 - Copyright (C) 2016 Real Time Engineers Ltd.
    All rights reserved
*/


#ifndef TIMERS_H
#define TIMERS_H

#ifndef INC_FREERTOS_H
	#error "include FreeRTOS.h must appear in source files before include timers.h"
#endif

/*lint -e537 This headers are only multiply included if the application code
happens to also be including task.h. */
#include "task.h"
/*lint +e537 */

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------
 * MACROS AND DEFINITIONS
 *----------------------------------------------------------*/

/* IDs for commands that can be sent/received on the timer queue.  These are to
be used solely through the macros that make up the public software timer API,
as defined below.  The commands that are sent from interrupts must use the
highest numbers as tmrFIRST_FROM_ISR_COMMAND is used to determine if the task
or interrupt version of the queue send function should be used. */
#define tmrCOMMAND_EXECUTE_CALLBACK_FROM_ISR 	( ( BaseType_t ) -2 )
#define tmrCOMMAND_EXECUTE_CALLBACK				( ( BaseType_t ) -1 )
#define tmrCOMMAND_START_DONT_TRACE				( ( BaseType_t ) 0 )
#define tmrCOMMAND_START					    ( ( BaseType_t ) 1 )
#define tmrCOMMAND_RESET						( ( BaseType_t ) 2 )
#define tmrCOMMAND_STOP							( ( BaseType_t ) 3 )
#define tmrCOMMAND_CHANGE_PERIOD				( ( BaseType_t ) 4 )
#define tmrCOMMAND_DELETE						( ( BaseType_t ) 5 )

#define tmrFIRST_FROM_ISR_COMMAND				( ( BaseType_t ) 6 )
#define tmrCOMMAND_START_FROM_ISR				( ( BaseType_t ) 6 )
#define tmrCOMMAND_RESET_FROM_ISR				( ( BaseType_t ) 7 )
#define tmrCOMMAND_STOP_FROM_ISR				( ( BaseType_t ) 8 )
#define tmrCOMMAND_CHANGE_PERIOD_FROM_ISR		( ( BaseType_t ) 9 )


/**
 * Type by which software timers are referenced.  For example, a call to
 * xTimerCreate() returns an TimerHandle_t variable that can then be used to
 * reference the subject timer in calls to other software timer API functions
 * (for example, xTimerStart(), xTimerReset(), etc.).
 */
typedef void * TimerHandle_t;

/*
 * Defines the prototype to which timer callback functions must conform.
 */
typedef void (*TimerCallbackFunction_t)( TimerHandle_t xTimer );

/*
 * Defines the prototype to which functions used with the
 * xTimerPendFunctionCallFromISR() function must conform.
 */
typedef void (*PendedFunction_t)( void *, uint32_t );

/**
 * 软件定时器动态创建函数，返回所创建的软件定时器的句柄
 */
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
	TimerHandle_t xTimerCreate(	const char * const pcTimerName,
								const TickType_t xTimerPeriodInTicks,
								const UBaseType_t uxAutoReload,
								void * const pvTimerID,
								TimerCallbackFunction_t pxCallbackFunction ) PRIVILEGED_FUNCTION; /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
#endif

/**
 * 软件定时器静态创建函数，返回所创建的软件定时器的句柄
 */
#if( configSUPPORT_STATIC_ALLOCATION == 1 )
	TimerHandle_t xTimerCreateStatic(	const char * const pcTimerName,
										const TickType_t xTimerPeriodInTicks,
										const UBaseType_t uxAutoReload,
										void * const pvTimerID,
										TimerCallbackFunction_t pxCallbackFunction,
										StaticTimer_t *pxTimerBuffer ) PRIVILEGED_FUNCTION; /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
#endif /* configSUPPORT_STATIC_ALLOCATION */

/**
 * 该函数用于获取目标定时器ID
 */
void *pvTimerGetTimerID( const TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;

/**
 * 该函数用于设置目标定时器ID
 */
void vTimerSetTimerID( TimerHandle_t xTimer, void *pvNewID ) PRIVILEGED_FUNCTION;

/**
 * 该函数用于判断目标定时器是否在使用
 */
BaseType_t xTimerIsTimerActive( TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;

/**
 * 该函数用于返回定时器任务句柄
 */
TaskHandle_t xTimerGetTimerDaemonTaskHandle( void ) PRIVILEGED_FUNCTION;

/**
* 该宏定义用于启动软件定时器
 */
#define xTimerStart( xTimer, xTicksToWait ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_START, ( xTaskGetTickCount() ), NULL, ( xTicksToWait ) )

/**
 * 该宏定义用于停止软件定时器
 */
#define xTimerStop( xTimer, xTicksToWait ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_STOP, 0U, NULL, ( xTicksToWait ) )

/**
 * 该宏定义用于修改软件定时器的周期
 */
 #define xTimerChangePeriod( xTimer, xNewPeriod, xTicksToWait ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_CHANGE_PERIOD, ( xNewPeriod ), NULL, ( xTicksToWait ) )

/**
 * 该宏定义用于删除软件定时器
 */
#define xTimerDelete( xTimer, xTicksToWait ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_DELETE, 0U, NULL, ( xTicksToWait ) )

/**
 * 该宏定义用于重置软件定时器
 */
#define xTimerReset( xTimer, xTicksToWait ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_RESET, ( xTaskGetTickCount() ), NULL, ( xTicksToWait ) )

/**
 * 该宏定义用于启动软件定时器（用于中断中的）
 */
#define xTimerStartFromISR( xTimer, pxHigherPriorityTaskWoken ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_START_FROM_ISR, ( xTaskGetTickCountFromISR() ), ( pxHigherPriorityTaskWoken ), 0U )

/**
 * 该宏定义用于启动软件定时器（用于中断中的）
 */
#define xTimerStopFromISR( xTimer, pxHigherPriorityTaskWoken ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_STOP_FROM_ISR, 0, ( pxHigherPriorityTaskWoken ), 0U )

/**
 * 该宏定义用于修改软件定时器的周期（用于中断中的）
 */
#define xTimerChangePeriodFromISR( xTimer, xNewPeriod, pxHigherPriorityTaskWoken ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_CHANGE_PERIOD_FROM_ISR, ( xNewPeriod ), ( pxHigherPriorityTaskWoken ), 0U )

/**
 * 该宏定义用于重置软件定时器（用于中断中的）
 */
#define xTimerResetFromISR( xTimer, pxHigherPriorityTaskWoken ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_RESET_FROM_ISR, ( xTaskGetTickCountFromISR() ), ( pxHigherPriorityTaskWoken ), 0U )


/**
 * 该函数用于在中断中设置执行回调函数的软件定时器，可供事件组的xEventGroupSetBitsFromISR和xEventGroupClearBitsFromISR函数使用
 */
BaseType_t xTimerPendFunctionCallFromISR( PendedFunction_t xFunctionToPend, void *pvParameter1, uint32_t ulParameter2, BaseType_t *pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;

 /**
  * BaseType_t xTimerPendFunctionCall( PendedFunction_t xFunctionToPend,
  *                                    void *pvParameter1,
  *                                    uint32_t ulParameter2,
  *                                    TickType_t xTicksToWait );
  *
  *
  * Used to defer the execution of a function to the RTOS daemon task (the timer
  * service task, hence this function is implemented in timers.c and is prefixed
  * with 'Timer').
  *
  * @param xFunctionToPend The function to execute from the timer service/
  * daemon task.  The function must conform to the PendedFunction_t
  * prototype.
  *
  * @param pvParameter1 The value of the callback function's first parameter.
  * The parameter has a void * type to allow it to be used to pass any type.
  * For example, unsigned longs can be cast to a void *, or the void * can be
  * used to point to a structure.
  *
  * @param ulParameter2 The value of the callback function's second parameter.
  *
  * @param xTicksToWait Calling this function will result in a message being
  * sent to the timer daemon task on a queue.  xTicksToWait is the amount of
  * time the calling task should remain in the Blocked state (so not using any
  * processing time) for space to become available on the timer queue if the
  * queue is found to be full.
  *
  * @return pdPASS is returned if the message was successfully sent to the
  * timer daemon task, otherwise pdFALSE is returned.
  *
  */
BaseType_t xTimerPendFunctionCall( PendedFunction_t xFunctionToPend, void *pvParameter1, uint32_t ulParameter2, TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

/**
 * 该函数用于获取目标定时器的名字
 */
const char * pcTimerGetName( TimerHandle_t xTimer ) PRIVILEGED_FUNCTION; /*lint !e971 Unqualified char types are allowed for strings and single characters only. */

/**
 * 该函数用于获取目标定时器的定时周期
 */
TickType_t xTimerGetPeriod( TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;

/**
* 该函数用于获取定时器结构体中的xTimerListItem链表项上的值
*/
TickType_t xTimerGetExpiryTime( TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;

/*
 * Functions beyond this part are not part of the public API and are intended
 * for use by the kernel only.
 */
BaseType_t xTimerCreateTimerTask( void ) PRIVILEGED_FUNCTION;
BaseType_t xTimerGenericCommand( TimerHandle_t xTimer, const BaseType_t xCommandID, const TickType_t xOptionalValue, BaseType_t * const pxHigherPriorityTaskWoken, const TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

#ifdef __cplusplus
}
#endif
#endif /* TIMERS_H */



