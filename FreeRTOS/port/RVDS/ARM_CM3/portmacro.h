/*
    FreeRTOS V9.0.0 - Copyright (C) 2016 Real Time Engineers Ltd.
*/


#ifndef PORTMACRO_H
#define PORTMACRO_H

#ifdef __cplusplus
extern "C" {
#endif

/* 类型定义 */
#define portCHAR		char
#define portFLOAT		float
#define portDOUBLE		double
#define portLONG		long
#define portSHORT		short
#define portSTACK_TYPE	uint32_t
#define portBASE_TYPE	long

typedef portSTACK_TYPE StackType_t;
typedef long BaseType_t;
typedef unsigned long UBaseType_t;

#if( configUSE_16_BIT_TICKS == 1 )
	typedef uint16_t TickType_t;
	#define portMAX_DELAY ( TickType_t ) 0xffff
#else
	typedef uint32_t TickType_t;
	#define portMAX_DELAY ( TickType_t ) 0xffffffffUL

	/* 32-bit tick type on a 32-bit architecture, so reads of the tick count do
	not need to be guarded with a critical section. */
	#define portTICK_TYPE_IS_ATOMIC 1
#endif
/*-----------------------------------------------------------*/

/* Architecture specifics. */
#define portSTACK_GROWTH			( -1 )
#define portTICK_PERIOD_MS			( ( TickType_t ) 1000 / configTICK_RATE_HZ )
#define portBYTE_ALIGNMENT			8

/* 在 ARM 架构中，内存屏障指令（如 DSB 和 ISB）有不同的行为模式，具体的控制标志可以通过数字值来指定。数字值 15 通常代表一个位掩码，它在内存屏障指令中启用了所有同步选项。 */
#define portSY_FULL_READ_WRITE		( 15 )

/*-----------------------------------------------------------*/

/* 触发PendSV，引起系统调度 */
#define portYIELD()																\
{																				\
	portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;	/* 触发PendSV，产生上下文切换 */\
																				\
	/* 同步内存操作和指令执行顺序 */						\
	__dsb( portSY_FULL_READ_WRITE );											\
	__isb( portSY_FULL_READ_WRITE );											\
}
/*-----------------------------------------------------------*/

#define portNVIC_INT_CTRL_REG		( * ( ( volatile uint32_t * ) 0xe000ed04 ) )/*中断控制及状态寄存器ICSR寄存器的地址，该寄存器的第28位为PENDSVSET，写入1可以悬起PendSV中断*/
#define portNVIC_PENDSVSET_BIT		( 1UL << 28UL )
#define portEND_SWITCHING_ISR( xSwitchRequired ) if( xSwitchRequired != pdFALSE ) portYIELD()	/*在中断中判断是否触发系统调度*/
#define portYIELD_FROM_ISR( x ) portEND_SWITCHING_ISR( x )
/*-----------------------------------------------------------*/

/* Critical section management. */
extern void vPortEnterCritical( void );
extern void vPortExitCritical( void );

#define portDISABLE_INTERRUPTS()				vPortRaiseBASEPRI()		//关中断
#define portENABLE_INTERRUPTS()					vPortSetBASEPRI( 0 )	//向basepri寄存器写入0打开中断
#define portENTER_CRITICAL()					vPortEnterCritical()	//进入临界区
#define portEXIT_CRITICAL()						vPortExitCritical()		//退出临界区
#define portSET_INTERRUPT_MASK_FROM_ISR()		ulPortRaiseBASEPRI() 	//可嵌套（带中断保护的）关中断函数
#define portCLEAR_INTERRUPT_MASK_FROM_ISR(x)	vPortSetBASEPRI(x)		//不带中断保护的中断设置函数

/*-----------------------------------------------------------*/

/* 提供了一个接口来控制系统的空闲时如何处理低功耗模式 */
#ifndef portSUPPRESS_TICKS_AND_SLEEP
	extern void vPortSuppressTicksAndSleep( TickType_t xExpectedIdleTime );
	#define portSUPPRESS_TICKS_AND_SLEEP( xExpectedIdleTime ) vPortSuppressTicksAndSleep( xExpectedIdleTime )
#endif
/*-----------------------------------------------------------*/

/* Port specific optimisations. */
#ifndef configUSE_PORT_OPTIMISED_TASK_SELECTION
	#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#endif

#if configUSE_PORT_OPTIMISED_TASK_SELECTION == 1

	/* 确保最大优先级configMAX_PRIORITIES被设置不大于32，因为系统中的就绪优先级是通过32位优先级位图实现的，所以优先级不能超过32，只能是0-31 */
	#if( configMAX_PRIORITIES > 32 )
		#error configUSE_PORT_OPTIMISED_TASK_SELECTION can only be set to 1 when configMAX_PRIORITIES is less than or equal to 32.  It is very rare that a system requires more than 10 to 15 difference priorities as tasks that share a priority will time slice.
	#endif

	/* 设置/清除优先级位图 */
	#define portRECORD_READY_PRIORITY( uxPriority, uxReadyPriorities ) ( uxReadyPriorities ) |= ( 1UL << ( uxPriority ) )
	#define portRESET_READY_PRIORITY( uxPriority, uxReadyPriorities ) ( uxReadyPriorities ) &= ~( 1UL << ( uxPriority ) )

	/*获取当前最高优先级，优先级数字越大，优先级越高，clz指令获取前导0的个数*/
	#define portGET_HIGHEST_PRIORITY( uxTopPriority, uxReadyPriorities ) uxTopPriority = ( 31UL - ( uint32_t ) __clz( ( uxReadyPriorities ) ) )

#endif /* taskRECORD_READY_PRIORITY */
/*-----------------------------------------------------------*/

/* Task function macros as described on the FreeRTOS.org WEB site.  These are
not necessary for to use this port.  They are defined so the common demo files
(which build with all the ports) will build. */
#define portTASK_FUNCTION_PROTO( vFunction, pvParameters ) void vFunction( void *pvParameters )
#define portTASK_FUNCTION( vFunction, pvParameters ) void vFunction( void *pvParameters )
/*-----------------------------------------------------------*/
/*vPortValidateInterruptPriority()该函数用于验证当前中断优先级是否有效*/
#ifdef configASSERT
	void vPortValidateInterruptPriority( void );
	#define portASSERT_IF_INTERRUPT_PRIORITY_INVALID() 	vPortValidateInterruptPriority()
#endif

/* portNOP() is not required by this port. */
#define portNOP()

#define portINLINE __inline

#ifndef portFORCE_INLINE
	#define portFORCE_INLINE __forceinline
#endif

/*-----------------------------------------------------------*/
//该函数可以设置basepri寄存器的值，以此来打开或关闭中断
static portFORCE_INLINE void vPortSetBASEPRI( uint32_t ulBASEPRI )
{
	__asm
	{
		msr basepri, ulBASEPRI
	}
}
/*-----------------------------------------------------------*/

/* 关中断函数，不带返回值所以不能嵌套，优先级小于等于configMAX_SYSCALL_INTERRUPT_PRIORITY的中断将被屏蔽，目前设置的为15*/
static portFORCE_INLINE void vPortRaiseBASEPRI( void )
{
uint32_t ulNewBASEPRI = configMAX_SYSCALL_INTERRUPT_PRIORITY;
	__asm
	{
		msr basepri, ulNewBASEPRI
		dsb
		isb
	}
}
/*-----------------------------------------------------------*/

//开中断函数
static portFORCE_INLINE void vPortClearBASEPRIFromISR( void )
{
	__asm
	{
		msr basepri, #0
	}
}
/*-----------------------------------------------------------*/

/* 关中断函数，带返回值所以可以嵌套，返回的是之前的basepri值*/
static portFORCE_INLINE uint32_t ulPortRaiseBASEPRI( void )
{
uint32_t ulReturn, ulNewBASEPRI = configMAX_SYSCALL_INTERRUPT_PRIORITY;

	__asm
	{
		mrs ulReturn, basepri
		msr basepri, ulNewBASEPRI
		dsb
		isb
	}

	return ulReturn;
}
/*-----------------------------------------------------------*/
/*用于检查当前是否处于中断上下文,即判断当前是否正在执行中断服务程序，若是返回pdTRUE否则返回pdFALSE*/
static portFORCE_INLINE BaseType_t xPortIsInsideInterrupt( void )
{
uint32_t ulCurrentInterrupt;
BaseType_t xReturn;

	/* Obtain the number of the currently executing interrupt. */
	__asm
	{
		mrs ulCurrentInterrupt, ipsr
	}

	if( ulCurrentInterrupt == 0 )
	{
		xReturn = pdFALSE;
	}
	else
	{
		xReturn = pdTRUE;
	}

	return xReturn;
}


#ifdef __cplusplus
}
#endif

#endif /* PORTMACRO_H */

