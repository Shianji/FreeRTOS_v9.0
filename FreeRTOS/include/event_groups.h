/*
    FreeRTOS V9.0.0 - Copyright (C) 2016 Real Time Engineers Ltd.
    All rights reserved
*/

#ifndef EVENT_GROUPS_H
#define EVENT_GROUPS_H

#ifndef INC_FREERTOS_H
	#error "include FreeRTOS.h" must appear in source files before "include event_groups.h"
#endif

/* FreeRTOS includes. */
#include "timers.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * An event group is a collection of bits to which an application can assign a
 * meaning.  For example, an application may create an event group to convey
 * the status of various CAN bus related events in which bit 0 might mean "A CAN
 * message has been received and is ready for processing", bit 1 might mean "The
 * application has queued a message that is ready for sending onto the CAN
 * network", and bit 2 might mean "It is time to send a SYNC message onto the
 * CAN network" etc.  A task can then test the bit values to see which events
 * are active, and optionally enter the Blocked state to wait for a specified
 * bit or a group of specified bits to be active.  To continue the CAN bus
 * example, a CAN controlling task can enter the Blocked state (and therefore
 * not consume any processing time) until either bit 0, bit 1 or bit 2 are
 * active, at which time the bit that was actually active would inform the task
 * which action it had to take (process a received message, send a message, or
 * send a SYNC).
 *
 * The event groups implementation contains intelligence to avoid race
 * conditions that would otherwise occur were an application to use a simple
 * variable for the same purpose.  This is particularly important with respect
 * to when a bit within an event group is to be cleared, and when bits have to
 * be set and then tested atomically - as is the case where event groups are
 * used to create a synchronisation point between multiple tasks (a
 * 'rendezvous').
 *
 * \defgroup EventGroup
 */



/**
 * event_groups.h
 *
 * Type by which event groups are referenced.  For example, a call to
 * xEventGroupCreate() returns an EventGroupHandle_t variable that can then
 * be used as a parameter to other event group functions.
 *
 * \defgroup EventGroupHandle_t EventGroupHandle_t
 * \ingroup EventGroup
 */
typedef void * EventGroupHandle_t;

/*
 * The type that holds event bits always matches TickType_t - therefore the
 * number of bits it holds is set by configUSE_16_BIT_TICKS (16 bits if set to 1,
 * 32 bits if set to 0.
 *
 * \defgroup EventBits_t EventBits_t
 * \ingroup EventGroup
 */
typedef TickType_t EventBits_t;

/**
 *
 * 动态事件组创建函数，返回创建的事件组句柄
 */
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
	EventGroupHandle_t xEventGroupCreate( void ) PRIVILEGED_FUNCTION;
#endif

/**
 * 静态事件组创建函数，返回创建的事件组句柄 
 */
#if( configSUPPORT_STATIC_ALLOCATION == 1 )
	EventGroupHandle_t xEventGroupCreateStatic( StaticEventGroup_t *pxEventGroupBuffer ) PRIVILEGED_FUNCTION;
#endif

/**
 * 事件等待函数，参数xEventGroup为事件组句柄，uxBitsToWaitFor指明要等待的事件（对应位置1），xClearOnExit指示事件满足后是否清除相应位，xWaitForAllBits指示是等待所有事件还是任一事件（即是与还是或），xTicksToWait为阻塞时间，该函数返回一个事件组状态，还是需要根据返回值判断是当前任务等待的事件满足了返回的还是超时返回的
 */
EventBits_t xEventGroupWaitBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToWaitFor, const BaseType_t xClearOnExit, const BaseType_t xWaitForAllBits, TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

/**
 * 事件组的位清除函数，可以将指定位uxBitsToClear清除
 */
EventBits_t xEventGroupClearBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear ) PRIVILEGED_FUNCTION;

/**
 * 件组清除位函数（在中断中使用），清除事件组中的标志位是一个不确定的操作（可能耗时很长），FreeRTOS不允许不确定的操作在中断或临界区中发生，所以通过软件定时器实现
 */
#if( configUSE_TRACE_FACILITY == 1 )
	BaseType_t xEventGroupClearBitsFromISR( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet ) PRIVILEGED_FUNCTION;
#else
	#define xEventGroupClearBitsFromISR( xEventGroup, uxBitsToClear ) xTimerPendFunctionCallFromISR( vEventGroupClearBitsCallback, ( void * ) xEventGroup, ( uint32_t ) uxBitsToClear, NULL )
#endif

/**
 * 事件组置位函数，并唤醒在置位相关位之后应解除阻塞的任务 
 */
EventBits_t xEventGroupSetBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet ) PRIVILEGED_FUNCTION;

/**
 * 事件组置位函数（在中断中使用），置位事件组中的标志位是一个不确定的操作（可能耗时很长），FreeRTOS不允许不确定的操作在中断或临界区中发生，所以通过软件定时器实现
 */
#if( configUSE_TRACE_FACILITY == 1 )
	BaseType_t xEventGroupSetBitsFromISR( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet, BaseType_t *pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;
#else
	#define xEventGroupSetBitsFromISR( xEventGroup, uxBitsToSet, pxHigherPriorityTaskWoken ) xTimerPendFunctionCallFromISR( vEventGroupSetBitsCallback, ( void * ) xEventGroup, ( uint32_t ) uxBitsToSet, pxHigherPriorityTaskWoken )
#endif

/**
 * 该函数的作用是将事件组的某些位uxBitsToSet置位，然后判断当前任务等待的事件位uxBitsToWaitFor是否满足，不满足则阻塞当前任务xTicksToWait
 */
EventBits_t xEventGroupSync( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet, const EventBits_t uxBitsToWaitFor, TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;


/**
 * event_groups.h
 *<pre>
	EventBits_t xEventGroupGetBits( EventGroupHandle_t xEventGroup );
 </pre>
 *
 * Returns the current value of the bits in an event group.  This function
 * cannot be used from an interrupt.
 *
 * @param xEventGroup The event group being queried.
 *
 * @return The event group bits at the time xEventGroupGetBits() was called.
 *
 * \defgroup xEventGroupGetBits xEventGroupGetBits
 * \ingroup EventGroup
 */
#define xEventGroupGetBits( xEventGroup ) xEventGroupClearBits( xEventGroup, 0 )

/**
 * 该函数在中断在使用，用于获取目标事件组的事件位的状态
 */
EventBits_t xEventGroupGetBitsFromISR( EventGroupHandle_t xEventGroup ) PRIVILEGED_FUNCTION;

/**
 * 事件组删除函数，可将目标事件组删除
 */
void vEventGroupDelete( EventGroupHandle_t xEventGroup ) PRIVILEGED_FUNCTION;

/* For internal use only. */
void vEventGroupSetBitsCallback( void *pvEventGroup, const uint32_t ulBitsToSet ) PRIVILEGED_FUNCTION;
void vEventGroupClearBitsCallback( void *pvEventGroup, const uint32_t ulBitsToClear ) PRIVILEGED_FUNCTION;


#if (configUSE_TRACE_FACILITY == 1)
	UBaseType_t uxEventGroupGetNumber( void* xEventGroup ) PRIVILEGED_FUNCTION;
#endif

#ifdef __cplusplus
}
#endif

#endif /* EVENT_GROUPS_H */


