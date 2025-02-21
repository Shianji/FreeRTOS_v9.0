/*
    FreeRTOS V9.0.0 - Copyright (C) 2016 Real Time Engineers Ltd.
    All rights reserved
*/

#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#ifndef INC_FREERTOS_H
	#error "include FreeRTOS.h" must appear in source files before "include semphr.h"
#endif

#include "queue.h"

typedef QueueHandle_t SemaphoreHandle_t;

#define semBINARY_SEMAPHORE_QUEUE_LENGTH	( ( uint8_t ) 1U )
#define semSEMAPHORE_QUEUE_ITEM_LENGTH		( ( uint8_t ) 0U )
#define semGIVE_BLOCK_TIME					( ( TickType_t ) 0U )


/**
 * 该宏定义用于创建二值信号量，并将信号量的值初始化为1，实际是借助于消息队列的数据结构和创建函数实现的，所以这里的队列长度传入1，消息大小传入semSEMAPHORE_QUEUE_ITEM_LENGTH即0，类型为queueQUEUE_TYPE_BINARY_SEMAPHORE表示二值信号量
 */
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
	#define vSemaphoreCreateBinary( xSemaphore )																							\
		{																																	\
			( xSemaphore ) = xQueueGenericCreate( ( UBaseType_t ) 1, semSEMAPHORE_QUEUE_ITEM_LENGTH, queueQUEUE_TYPE_BINARY_SEMAPHORE );	\
			if( ( xSemaphore ) != NULL )																									\
			{																																\
				( void ) xSemaphoreGive( ( xSemaphore ) );																					\
			}																																\
		}
#endif

/**
 * 该宏定义用于创建二值信号量，但是并没有将信号量初始化为1（信号量初始值为0），目前一般用上面的vSemaphoreCreateBinary代替xSemaphoreCreateBinary
 */
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
	#define xSemaphoreCreateBinary() xQueueGenericCreate( ( UBaseType_t ) 1, semSEMAPHORE_QUEUE_ITEM_LENGTH, queueQUEUE_TYPE_BINARY_SEMAPHORE )
#endif

/**
 * 该宏定义用于静态创建计数信号量，信号量初始值为0
 */
#if( configSUPPORT_STATIC_ALLOCATION == 1 )
	#define xSemaphoreCreateBinaryStatic( pxStaticSemaphore ) xQueueGenericCreateStatic( ( UBaseType_t ) 1, semSEMAPHORE_QUEUE_ITEM_LENGTH, NULL, pxStaticSemaphore, queueQUEUE_TYPE_BINARY_SEMAPHORE )
#endif /* configSUPPORT_STATIC_ALLOCATION */

/**
 * 该宏定义用于获取信号量，若信号量值为0时会被阻塞，xBlockTime为可阻塞的时间，该宏定义也可以用于获取普通互斥量
 */
#define xSemaphoreTake( xSemaphore, xBlockTime )		xQueueGenericReceive( ( QueueHandle_t ) ( xSemaphore ), NULL, ( xBlockTime ), pdFALSE )

/**
 * 该宏定义用于获取递归互斥量
 */
#if( configUSE_RECURSIVE_MUTEXES == 1 )
	#define xSemaphoreTakeRecursive( xMutex, xBlockTime )	xQueueTakeMutexRecursive( ( xMutex ), ( xBlockTime ) )
#endif

/**
 * 该宏定义用于释放信号量，即给信号量当前值加1，该宏定义也可以用于释放普通互斥量
 */
#define xSemaphoreGive( xSemaphore )		xQueueGenericSend( ( QueueHandle_t ) ( xSemaphore ), NULL, semGIVE_BLOCK_TIME, queueSEND_TO_BACK )

/**
 * 该宏定义用于释放递归互斥量
 */
#if( configUSE_RECURSIVE_MUTEXES == 1 )
	#define xSemaphoreGiveRecursive( xMutex )	xQueueGiveMutexRecursive( ( xMutex ) )
#endif

/**
 * 该宏定义用于（用于中断中的）释放信号量，即给信号量当前值加1
 */
#define xSemaphoreGiveFromISR( xSemaphore, pxHigherPriorityTaskWoken )	xQueueGiveFromISR( ( QueueHandle_t ) ( xSemaphore ), ( pxHigherPriorityTaskWoken ) )

/**
 *  该宏定义用于（用于中断中的）获取信号量，中断中是不允许阻塞的
 */
#define xSemaphoreTakeFromISR( xSemaphore, pxHigherPriorityTaskWoken )	xQueueReceiveFromISR( ( QueueHandle_t ) ( xSemaphore ), NULL, ( pxHigherPriorityTaskWoken ) )

/**
 * 该宏定义用于动态创建创建普通互斥量
 */
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
	#define xSemaphoreCreateMutex() xQueueCreateMutex( queueQUEUE_TYPE_MUTEX )
#endif

/**
 * 该宏定义用于静态创建创建普通互斥量
 */
 #if( configSUPPORT_STATIC_ALLOCATION == 1 )
	#define xSemaphoreCreateMutexStatic( pxMutexBuffer ) xQueueCreateMutexStatic( queueQUEUE_TYPE_MUTEX, ( pxMutexBuffer ) )
#endif /* configSUPPORT_STATIC_ALLOCATION */


/**
 * 该宏定义用于动态创建创建递归互斥量
 */
#if( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configUSE_RECURSIVE_MUTEXES == 1 ) )
	#define xSemaphoreCreateRecursiveMutex() xQueueCreateMutex( queueQUEUE_TYPE_RECURSIVE_MUTEX )
#endif

/**
 * 该宏定义用于静态创建创建递归互斥量
 */
#if( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configUSE_RECURSIVE_MUTEXES == 1 ) )
	#define xSemaphoreCreateRecursiveMutexStatic( pxStaticSemaphore ) xQueueCreateMutexStatic( queueQUEUE_TYPE_RECURSIVE_MUTEX, pxStaticSemaphore )
#endif /* configSUPPORT_STATIC_ALLOCATION */

/**
 * 该宏定义用于动态创建计数信号量，并信号量初始值设置为uxInitialCount，uxMaxCount为信号量最大值
 */
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
	#define xSemaphoreCreateCounting( uxMaxCount, uxInitialCount ) xQueueCreateCountingSemaphore( ( uxMaxCount ), ( uxInitialCount ) )
#endif

/**
 * 该宏定义用于静态创建计数信号量
 */
#if( configSUPPORT_STATIC_ALLOCATION == 1 )
	#define xSemaphoreCreateCountingStatic( uxMaxCount, uxInitialCount, pxSemaphoreBuffer ) xQueueCreateCountingSemaphoreStatic( ( uxMaxCount ), ( uxInitialCount ), ( pxSemaphoreBuffer ) )
#endif /* configSUPPORT_STATIC_ALLOCATION */

/**
 * 该宏定义用于删除信号量或互斥量，实际调用的是消息队列的删除函数
 */
#define vSemaphoreDelete( xSemaphore ) vQueueDelete( ( QueueHandle_t ) ( xSemaphore ) )

/**
 * semphr.h
 * <pre>TaskHandle_t xSemaphoreGetMutexHolder( SemaphoreHandle_t xMutex );</pre>
 *
 * If xMutex is indeed a mutex type semaphore, return the current mutex holder.
 * If xMutex is not a mutex type semaphore, or the mutex is available (not held
 * by a task), return NULL.
 *
 * Note: This is a good way of determining if the calling task is the mutex
 * holder, but not a good way of determining the identity of the mutex holder as
 * the holder may change between the function exiting and the returned value
 * being tested.
 */
#define xSemaphoreGetMutexHolder( xSemaphore ) xQueueGetMutexHolder( ( xSemaphore ) )

/**
 * semphr.h
 * <pre>UBaseType_t uxSemaphoreGetCount( SemaphoreHandle_t xSemaphore );</pre>
 *
 * If the semaphore is a counting semaphore then uxSemaphoreGetCount() returns
 * its current count value.  If the semaphore is a binary semaphore then
 * uxSemaphoreGetCount() returns 1 if the semaphore is available, and 0 if the
 * semaphore is not available.
 *
 */
#define uxSemaphoreGetCount( xSemaphore ) uxQueueMessagesWaiting( ( QueueHandle_t ) ( xSemaphore ) )

#endif /* SEMAPHORE_H */


