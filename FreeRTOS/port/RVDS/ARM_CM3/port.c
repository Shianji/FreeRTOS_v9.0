/*
    FreeRTOS V9.0.0 - Copyright (C) 2016 Real Time Engineers Ltd.
    All rights reserved
*/

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"

#ifndef configKERNEL_INTERRUPT_PRIORITY
	#define configKERNEL_INTERRUPT_PRIORITY 255
#endif

#if configMAX_SYSCALL_INTERRUPT_PRIORITY == 0
	#error configMAX_SYSCALL_INTERRUPT_PRIORITY must not be set to 0.  See http://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html
#endif

/*portNVIC_SYSTICK_CLK_BIT 是一个位掩码，它表示SysTick使用CPU时钟源,这里表示允许自定义SysTick定时器的时钟源。如果不做特殊配置，系统会默认使用与CPU时钟源相同的时钟作为SysTick的时钟源。*/
#ifndef configSYSTICK_CLOCK_HZ
	#define configSYSTICK_CLOCK_HZ configCPU_CLOCK_HZ
	#define portNVIC_SYSTICK_CLK_BIT	( 1UL << 2UL )
#else
	#define portNVIC_SYSTICK_CLK_BIT	( 0 )
#endif

/* The __weak attribute does not work as you might expect with the Keil tools
so the configOVERRIDE_DEFAULT_TICK_CONFIGURATION constant must be set to 1 if
the application writer wants to provide their own implementation of
vPortSetupTimerInterrupt().  Ensure configOVERRIDE_DEFAULT_TICK_CONFIGURATION
is defined. */
#ifndef configOVERRIDE_DEFAULT_TICK_CONFIGURATION
	#define configOVERRIDE_DEFAULT_TICK_CONFIGURATION 0
#endif

/* Cortex-M3的SysTick寄存器地址宏定义 */
#define portNVIC_SYSTICK_CTRL_REG			( * ( ( volatile uint32_t * ) 0xe000e010 ) )
#define portNVIC_SYSTICK_LOAD_REG			( * ( ( volatile uint32_t * ) 0xe000e014 ) )
#define portNVIC_SYSTICK_CURRENT_VALUE_REG	( * ( ( volatile uint32_t * ) 0xe000e018 ) )
#define portNVIC_SYSPRI2_REG				( * ( ( volatile uint32_t * ) 0xe000ed20 ) )
/* ...then bits in the registers. */
#define portNVIC_SYSTICK_INT_BIT			( 1UL << 1UL )
#define portNVIC_SYSTICK_ENABLE_BIT			( 1UL << 0UL )
#define portNVIC_SYSTICK_COUNT_FLAG_BIT		( 1UL << 16UL )
#define portNVIC_PENDSVCLEAR_BIT 			( 1UL << 27UL )
#define portNVIC_PEND_SYSTICK_CLEAR_BIT		( 1UL << 25UL )

/* Masks off all bits but the VECTACTIVE bits in the ICSR register. */
#define portVECTACTIVE_MASK					( 0xFFUL )
/* 
 * 参考资料《STM32F10xxx Cortex-M3 programming manual》4.4.3，百度搜索“PM0056”即可找到这个文档
 * 在Cortex-M中，内核外设SCB中SHPR3寄存器用于设置SysTick和PendSV的异常优先级
 * System handler priority register 3 (SCB_SHPR3) SCB_SHPR3：0xE000 ED20
 * Bits 31:24 PRI_15[7:0]: Priority of system handler 15, SysTick exception 
 * Bits 23:16 PRI_14[7:0]: Priority of system handler 14, PendSV 
 * 按下面这样设定，关中断时PENDSV和SYSTICK中断都会被屏蔽
 */
#define portNVIC_PENDSV_PRI					( ( ( uint32_t ) configKERNEL_INTERRUPT_PRIORITY ) << 16UL )
#define portNVIC_SYSTICK_PRI				( ( ( uint32_t ) configKERNEL_INTERRUPT_PRIORITY ) << 24UL )

/* 检查中断优先级有效性所需的常量，在xPortStartScheduler函数中使用用来获取系统的优先级分组情况 */
#define portFIRST_USER_INTERRUPT_NUMBER		( 16 )	/*定义了第一个用户中断号的值为16，在ARM Cortex-M微控制器中，系统中断（如系统定时器、故障中断等）通常使用中断号0到15*/
#define portNVIC_IP_REGISTERS_OFFSET_16 	( 0xE000E3F0 ) /*中断优先级寄存器的基地址，16号中断也即用户第一个中断的地址为 0xE000E400，即portNVIC_IP_REGISTERS_OFFSET_16+portFIRST_USER_INTERRUPT_NUMBER */
#define portAIRCR_REG						( * ( ( volatile uint32_t * ) 0xE000ED0C ) )//该寄存器的主要作用是控制中断和复位的行为，包括设置优先级分组
#define portMAX_8_BIT_VALUE					( ( uint8_t ) 0xff )
#define portTOP_BIT_OF_BYTE					( ( uint8_t ) 0x80 )
#define portMAX_PRIGROUP_BITS				( ( uint8_t ) 7 )//此宏定义表示 Cortex-M 处理器中断优先级分组中可用的最大位数
#define portPRIORITY_GROUP_MASK				( 0x07UL << 8UL )//此宏定义表示用于设置 中断优先级分组的掩码
#define portPRIGROUP_SHIFT					( 8UL )//此宏定义表示优先级分组掩码的位移量

/* Constants required to set up the initial stack. */
#define portINITIAL_XPSR			( 0x01000000 )

/* systick 是一个24位计数器 */
#define portMAX_24_BIT_NUMBER				( 0xffffffUL )

/* 一个用于估计在无滴答空闲计算期间 SysTick 计数器停止时可能发生的 SysTick 计数数量的调整因子 */
#define portMISSED_COUNTS_FACTOR			( 45UL )

/* 为了严格遵守 Cortex-M 规范，任务起始地址应该清除位 0，因为它在退出 ISR 时加载到 PC 中。 */
#define portSTART_ADDRESS_MASK				( ( StackType_t ) 0xfffffffeUL )

/* Each task maintains its own interrupt status in the critical nesting
variable. */
//uxCriticalNesting 是FreeRTOS用来追踪临界区嵌套的变量，该值通常会在创建调度程序之前或在系统初始化时会被重新设置为0，见函数xPortStartScheduler，在运行过程中，如果发现 uxCriticalNesting 的值不正确（例如仍然是 0xaaaaaaaa），则可以追踪问题的来源，确保系统的健壮性。
static UBaseType_t uxCriticalNesting = 0xaaaaaaaa;

/*
 * 设置Systick相关的寄存器的值的函数的声明，该函数用于自定义时钟中断
 */
void vPortSetupTimerInterrupt( void );

/*
 * 异常处理函数声明
 */
void xPortPendSVHandler( void );
void xPortSysTickHandler( void );
void vPortSVCHandler( void );

/*
 * 系统启动时执行的函数的声明
 */
static void prvStartFirstTask( void );

/*
 * 当一个任务企图退出时会调用该函数，理论上任务是while无限循环的，只会被调度，不会退出，退出说明出错了
 */
static void prvTaskExitError( void );

/*-----------------------------------------------------------*/

/*
 * The number of SysTick increments that make up one tick period.ulTimerCountsForOneTick表示每个tick晶振的振动次数
 */
#if configUSE_TICKLESS_IDLE == 1
	static uint32_t ulTimerCountsForOneTick = 0;
#endif /* configUSE_TICKLESS_IDLE */

/*
 * 可抑制的最大滴答周期数,受 SysTick 定时器的 24 位分辨率限制（低功耗模式）
 */
#if configUSE_TICKLESS_IDLE == 1
	static uint32_t xMaximumPossibleSuppressedTicks = 0;
#endif /* configUSE_TICKLESS_IDLE */

/*
 * 补偿 SysTick 停止时经过的 CPU 周期（仅限低功耗功能）。
 */
#if configUSE_TICKLESS_IDLE == 1
	static uint32_t ulStoppedTimerCompensation = 0;
#endif /* configUSE_TICKLESS_IDLE */

/*
 * 根据是否需要在xPortStartScheduler函数中获取当前系统的优先级分组情况来定义全局变量ucMaxSysCallPriority和ulMaxPRIGROUPValue分别表示系统可管理的最高优先级和应该根据分组情况写入AIRCR 寄存器的值
 */
#if ( configASSERT_DEFINED == 1 )
	 static uint8_t ucMaxSysCallPriority = 0;
	 static uint32_t ulMaxPRIGROUPValue = 0;
	 static const volatile uint8_t * const pcInterruptPriorityRegisters = ( uint8_t * ) portNVIC_IP_REGISTERS_OFFSET_16;
#endif /* configASSERT_DEFINED */

/*-----------------------------------------------------------*/

/* 任务栈初始化函数，下面的操作中每个--自减运算是按字为单位进行的，这个函数相当于模拟了任务正常运行时中断发生任务备切换后，任务栈中硬件自动操作后的结果，这个函数只有在任务刚被创建时才会被调用 */
StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters )
{
	//异常发生时，自动加载到CPU寄存器的内容，里栈的写入顺序可以参考CM3权威指南第九章-中断的具体行为，R13即SP
	pxTopOfStack--; /* Offset added to account for the way the MCU uses the stack on entry/exit of interrupts. */
	*pxTopOfStack = portINITIAL_XPSR;	/* xPSR的bit24必须置1 */
	pxTopOfStack--;
	*pxTopOfStack = ( ( StackType_t ) pxCode ) & portSTART_ADDRESS_MASK;	/* 压入PC，即任务入口函数，R15即PC */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) prvTaskExitError;/* LR，函数返回地址，R14即LR */

	pxTopOfStack -= 5;	/* R12, R3, R2 and R1. */
	*pxTopOfStack = ( StackType_t ) pvParameters;	/* R0，任务形参 */
	/* 异常发生时，以下是手动加载到CPU寄存器的内容 */
	pxTopOfStack -= 8;	/* R11, R10, R9, R8, R7, R6, R5 and R4默认初始化为0 */
	/* 返回栈顶指针，此时pxTopOfStack指向空闲栈 */
	return pxTopOfStack;
}
/*-----------------------------------------------------------*/
/* 这个函数在任务企图退出时被调用，任务实际上应该永远不会退出只会被调度，企图退出说明出错了 */
static void prvTaskExitError( void )
{
	configASSERT( uxCriticalNesting == ~0UL );
	portDISABLE_INTERRUPTS();
	for( ;; );
}
/*-----------------------------------------------------------*/
/*SVC中断处理函数，只有在系统刚启动，唤醒第一个任务时会被调用*/
__asm void vPortSVCHandler( void )
{
	PRESERVE8

	ldr	r3, =pxCurrentTCB	/* 加载pxCurrentTCB的地址到r3 */
	ldr r1, [r3]			/* 加载pxCurrentTCB到r1 */
	ldr r0, [r1]			/* 加载pxCurrentTCB指向的值到r0，目前r0的值等于第一个任务堆栈的栈顶 */
	ldmia r0!, {r4-r11}		/* 以r0为基地址，将栈里面的内容加载到r4~r11寄存器，同时r0会递增 */
	msr psp, r0				/* 将r0的值，即任务的栈指针更新到psp */
	isb			
	mov r0, #0				/* 设置r0的值为0 */
	msr	basepri, r0		/* 设置basepri寄存器的值为0，即所有的中断都没有被屏蔽在进入异常服务程序后，将自动更新LR的值为特殊的EXC_RETURN，可以参考CM3权威指南第九章-中断的具体行为9.6异常返回值 */
	orr r14, #0xd			/* 当从SVC中断服务退出前,通过向r14寄存器最后4位按位或上0x0D，使得硬件在退出时使用进程堆栈指针PSP（bit2）完成出栈操作并返回后进入线程模式（bit3）、返回Thumb状态（bit0） */
	bx r14
}
/*-----------------------------------------------------------*/

__asm void prvStartFirstTask( void )
{
	PRESERVE8
	/* 在Cortex-M中，0xE000ED08是SCB_VTOR这个寄存器的地址，
       里面存放的是向量表的起始地址，即MSP的地址，可以参考启动文件，启动文件的汇编代码部分定义了主栈的大小和位置，并将其地址赋值给了中断向量表的第一个向量 */
	ldr r0, =0xE000ED08
	ldr r0, [r0]
	ldr r0, [r0]//此时r0里面的值就是主栈的栈顶地址

	/* 设置主堆栈指针msp的值 */
	msr msp, r0
	/* 使能全局中断 */
	cpsie i
	cpsie f
	dsb
	isb
	/* 调用SVC去启动第一个任务，会进入到svc中断处理函数vPortSVCHandler */
	svc 0
	nop
	nop
}
/*-----------------------------------------------------------*/
/* 该函数用与初始化定时器，启动第一个任务，设置PENDSV和SYSTICK中断的优先级，并根据设置获取系统优先级分组情况*/
BaseType_t xPortStartScheduler( void )
{
	#if( configASSERT_DEFINED == 1 )//下面这部分用来获取系统的优先级分组情况
	{
		volatile uint32_t ulOriginalPriority;
		volatile uint8_t * const pucFirstUserPriorityRegister = ( uint8_t * ) ( portNVIC_IP_REGISTERS_OFFSET_16 + portFIRST_USER_INTERRUPT_NUMBER );
		volatile uint8_t ucMaxPriorityValue;

		/* 确定可以调用 ISR 安全 FreeRTOS API 函数的最大优先级。ISR 安全函数是以“FromISR”结尾的函数。FreeRTOS 维护单独的线程和 ISR API 函数，以确保中断入口尽可能快速和简单。保存即将被破坏的中断优先级值。 */
		ulOriginalPriority = *pucFirstUserPriorityRegister;

		/* 确定可用的优先级位数，首先写入所有可能的位。 */
		*pucFirstUserPriorityRegister = portMAX_8_BIT_VALUE;
		//写入8位1再读出，可以得知当前系统中优先级是几位有效的，无效的位读出来是0，所以在这里下面的ucMaxPriorityValue读出来应该是0xf0
		ucMaxPriorityValue = *pucFirstUserPriorityRegister;

		/* 根据宏定义configMAX_SYSCALL_INTERRUPT_PRIORITY得到系统可管理的最高优先级 */
		ucMaxSysCallPriority = configMAX_SYSCALL_INTERRUPT_PRIORITY & ucMaxPriorityValue;

		//这个while循环在这里得到的ulMaxPRIGROUPValue值应该是0b011,表示最大优先组为3，也就是表示用IPR寄存器的高4位用来配置优先级
		ulMaxPRIGROUPValue = portMAX_PRIGROUP_BITS;
		while( ( ucMaxPriorityValue & portTOP_BIT_OF_BYTE ) == portTOP_BIT_OF_BYTE )
		{
			ulMaxPRIGROUPValue--;
			ucMaxPriorityValue <<= ( uint8_t ) 0x01;
		}
		
		/* 根据所得结果得出应该写入AIRCR 寄存器的值 */
		ulMaxPRIGROUPValue <<= portPRIGROUP_SHIFT;
		ulMaxPRIGROUPValue &= portPRIORITY_GROUP_MASK;//最后得到的ulMaxPRIGROUPValue值是0b0000 0000 0000 0000 0000 0011 0000 0000,实际的优先级分组情况会在main函数中的BSP_Init()函数中的NVIC_PriorityGroupConfig( NVIC_PriorityGroup_4 )中配置

		/* 恢复借用的优先级寄存器中的原来的值 */
		*pucFirstUserPriorityRegister = ulOriginalPriority;
	}
	#endif /* conifgASSERT_DEFINED */

	//设置PendSV和Systick的中断优先级
	portNVIC_SYSPRI2_REG |= portNVIC_PENDSV_PRI;
	portNVIC_SYSPRI2_REG |= portNVIC_SYSTICK_PRI;

	//初始化定时器,时间到了会产生SysTick中断
	vPortSetupTimerInterrupt();

	/* Initialise the critical nesting count ready for the first task. */
	uxCriticalNesting = 0;

	/* 唤醒第一个任务 */
	prvStartFirstTask();

	/* Should not get here! */
	return 0;
}
/*-----------------------------------------------------------*/

void vPortEndScheduler( void )
{
	/* 用于验证在调度器结束时是否还有不正常的临界区嵌套，如果 uxCriticalNesting 不等于预期的值（1000），会触发断言，这通常意味着在调度器结束时，仍然存在不合适的临界区嵌套，可能导致系统不稳定，或者临界区解锁和恢复顺序出错 */
	configASSERT( uxCriticalNesting == 1000UL );
}
/*-----------------------------------------------------------*/
/*临界区进入函数，这个函数会关中断*/
void vPortEnterCritical( void )
{
	portDISABLE_INTERRUPTS();
	uxCriticalNesting++;

	/* 这不是进入临界区函数的中断安全版本，因此如果从中断上下文调用它，则使用 assert()。只有以“FromISR”结尾的 API 函数才能在中断中使用。如果断言函数也使用临界区部分，则仅当临界区嵌套计数为 1 时才进行断言，以防止递归调用。 */
	if( uxCriticalNesting == 1 )//检查当前是否正在中断中，若在则会报错，表明在中断上下文中不应调用 vPortEnterCritical，可以理解为不能在中断中进入临界区
	{
		configASSERT( ( portNVIC_INT_CTRL_REG & portVECTACTIVE_MASK ) == 0 );
	}
}
/*-----------------------------------------------------------*/
/*临界区退出函数*/
void vPortExitCritical( void )
{
	configASSERT( uxCriticalNesting );
	uxCriticalNesting--;
	if( uxCriticalNesting == 0 )
	{
		portENABLE_INTERRUPTS();
	}
}
/*-----------------------------------------------------------*/
/*PendSV中断处理函数，用来实现任务的切换*/
__asm void xPortPendSVHandler( void )
{
	extern uxCriticalNesting;
	extern pxCurrentTCB;
	extern vTaskSwitchContext;

	PRESERVE8
	/* 当进入PendSVC Handler时，上一个任务运行的环境即：xPSR（程序状态寄存器），PC（任务入口地址），R14，R12，R3，R2，R1，R0（任务的形参）这些CPU寄存器的值会自动保存到任务的栈中，剩下的r4~r11需要手动保存 */
  	/* 获取任务栈指针到r0 */
	mrs r0, psp
	isb

	ldr	r3, =pxCurrentTCB		/* 加载pxCurrentTCB的地址到r3 */
	ldr	r2, [r3]						/* 加载pxCurrentTCB到r2 */

	stmdb r0!, {r4-r11}			/* 将CPU寄存器r4~r11的值存储到r0指向的地址 */
	str r0, [r2]						 /* 将任务栈的新的栈顶指针存储到当前任务TCB的第一个成员，即栈顶指针 */		

	stmdb sp!, {r3, r14}		 /* 将R3和R14临时压入堆栈，因为即将调用函数vTaskSwitchContext,
									调用函数时,返回地址自动保存到R14中,所以一旦调用发生,R14的值会被覆盖,因此需要入栈保护;
									R3保存的当前激活的任务TCB指针(pxCurrentTCB)地址,函数调用后会用到,因此也要入栈保护，这里的sp是msp */
	mov r0, #configMAX_SYSCALL_INTERRUPT_PRIORITY	 /* 关中断，进入临界段 */
	msr basepri, r0						/*basepri这个寄存器最多有 9 位（对于大多数 Cortex-M 处理器来说，BASEPRI 只有 8 位有效，具体多少位由表达优先级的位数决定,本程序使用的是stmF103只有高四位有效， Cortex-M 系列处理器中的中断优先级使用的是高位对齐方式）。
											它定义了被屏蔽优先级的阈值。当它被设成某个值后，所有优先级号大于等于此值的中断都被关（优先级号越大，优先级越低）。但若被设成 0，则不关闭任何中断， 0 也是缺省值。*/
	dsb
	isb
	bl vTaskSwitchContext			 /* 调用函数vTaskSwitchContext，寻找新的任务运行,通过使变量pxCurrentTCB指向新的任务来实现任务切换 */ 
	mov r0, #0                  		/* 开中断，退出临界段 */
	msr basepri, r0						
	ldmia sp!, {r3, r14}

	ldr r1, [r3]
	ldr r0, [r1]				/* 当前激活的任务TCB第一项保存了任务堆栈的栈顶,现在栈顶值存入R0，这时候pxCurrentTCB指针里面存储的地址值已经变了，任务已经切换了*/
	ldmia r0!, {r4-r11}			/* 将新任务对应的寄存器值出栈 */
	msr psp, r0
	isb
	bx r14								/* 异常发生时,R14中保存异常返回标志,包括返回后进入线程模式还是处理器模式、使用PSP堆栈指针还是MSP堆栈指针，当调用 bx r14指令后，硬件会知道要从异常返回，
                                   		然后出栈，这个时候堆栈指针PSP已经指向了新任务堆栈的正确位置，当新任务的运行地址被出栈到PC寄存器后，新的任务也会被执行。*/
	nop
}
/*-----------------------------------------------------------*/
/*SysTick中断处理函数*/
void xPortSysTickHandler( void )
{
	/* SysTick 以最低中断优先级运行，因此当执行此中断时，所有中断都必须取消屏蔽。因此无需保存然后恢复中断屏蔽值，因为其值已经知道 - 因此使用稍快的 vPortRaiseBASEPRI() 函数代替 portSET_INTERRUPT_MASK_FROM_ISR()。 */
	vPortRaiseBASEPRI();
	{
		/* 增加 RTOS tick值. */
		if( xTaskIncrementTick() != pdFALSE )
		{
			/* 设置 PendSV 的中断挂起位，可能产生上下文切换*/
			portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
		}
	}
	vPortClearBASEPRIFromISR();
}
/*-----------------------------------------------------------*/

#if configUSE_TICKLESS_IDLE == 1
/*低功耗模式的处理函数*/
	__weak void vPortSuppressTicksAndSleep( TickType_t xExpectedIdleTime )
	{
	uint32_t ulReloadValue, ulCompleteTickPeriods, ulCompletedSysTickDecrements, ulSysTickCTRL;
	TickType_t xModifiableIdleTime;

		/* Make sure the SysTick reload value does not overflow the counter. */
		if( xExpectedIdleTime > xMaximumPossibleSuppressedTicks )
		{
			xExpectedIdleTime = xMaximumPossibleSuppressedTicks;
		}

		/* Stop the SysTick momentarily.  The time the SysTick is stopped for
		is accounted for as best it can be, but using the tickless mode will
		inevitably result in some tiny drift of the time maintained by the
		kernel with respect to calendar time. */
		portNVIC_SYSTICK_CTRL_REG &= ~portNVIC_SYSTICK_ENABLE_BIT;

		/* Calculate the reload value required to wait xExpectedIdleTime
		tick periods.  -1 is used because this code will execute part way
		through one of the tick periods. */
		ulReloadValue = portNVIC_SYSTICK_CURRENT_VALUE_REG + ( ulTimerCountsForOneTick * ( xExpectedIdleTime - 1UL ) );
		if( ulReloadValue > ulStoppedTimerCompensation )
		{//从滴答定时器停止运行，到把统计得到的低功耗模式运行时间补偿给 FreeRTOS系统时钟，也是需要时间的，这期间也是有程序在运行的。这段程序运行的时间我们要留出来，具体的时间没法去统计，因为平台不同、编译器的代码优化水平不同导致了程序的执行时间也不同。
			ulReloadValue -= ulStoppedTimerCompensation;
		}

		/* Enter a critical section but don't use the taskENTER_CRITICAL()
		method as that will mask interrupts that should exit sleep mode.不使用 taskENTER_CRITICAL() 是为了不屏蔽那些能唤醒系统的中断 */
		__disable_irq();//调用 __disable_irq() 后，通过设置 PRIMASK（Primary Mask Register）来禁用中断除了不可屏蔽中断（如硬故障 HardFault），处理器将不再响应任何中断或异常请求，直到重新启用中断。
		__dsb( portSY_FULL_READ_WRITE );
		__isb( portSY_FULL_READ_WRITE );

		/* If a context switch is pending or a task is waiting for the scheduler
		to be unsuspended then abandon the low power entry.判断是否可以进入低功耗状态，返回eAbortSleep就不能进入低功耗模式了*/
		if( eTaskConfirmSleepModeStatus() == eAbortSleep )
		{
			/* Restart from whatever is left in the count register to complete
			this tick period. */
			portNVIC_SYSTICK_LOAD_REG = portNVIC_SYSTICK_CURRENT_VALUE_REG;

			/* Restart SysTick. */
			portNVIC_SYSTICK_CTRL_REG |= portNVIC_SYSTICK_ENABLE_BIT;

			/* Reset the reload register to the value required for normal tick
			periods. */
			portNVIC_SYSTICK_LOAD_REG = ulTimerCountsForOneTick - 1UL;

			/* Re-enable interrupts - see comments above __disable_irq() call
			above. */
			__enable_irq();
		}
		else
		{
			/* Set the new reload value. */
			portNVIC_SYSTICK_LOAD_REG = ulReloadValue;

			/* Clear the SysTick count flag and set the count value back to
			zero. */
			portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;

			/* Restart SysTick. */
			portNVIC_SYSTICK_CTRL_REG |= portNVIC_SYSTICK_ENABLE_BIT;

			/* Sleep until something happens.  configPRE_SLEEP_PROCESSING() can
			set its parameter to 0 to indicate that its implementation contains
			its own wait for interrupt or wait for event instruction, and so wfi
			should not be executed again.  However, the original expected idle
			time variable must remain unmodified, so a copy is taken. */
			xModifiableIdleTime = xExpectedIdleTime;
			configPRE_SLEEP_PROCESSING( xModifiableIdleTime );//在真正的低功耗设计中不仅仅是将处理器设置到低功耗模式就行了，还需要做一些其他的处理，比如： 将处理器降低到合适的频率,修改时钟源等，FreeRTOS 为我们提供了一个宏来完成这些操作，它就是 configPRE_SLEEP_PROCESSING()，这个宏的具体实现内容需要用户去编写。
			if( xModifiableIdleTime > 0 )
			{
				__dsb( portSY_FULL_READ_WRITE );
				__wfi();					//__wfi() 指令使处理器进入低功耗模式，等待中断的发生。当没有中断发生时，处理器将处于停止状态，不进行任何指令执行，从而减少功耗。一旦发生外部或内部中断，处理器会立即退出低功耗状态，继续执行后续指令。但由于此处在前面执行了__disable_irq()屏蔽了中断，所以中断在调用__enable_irq()之前不会被执行。
				__isb( portSY_FULL_READ_WRITE );
			}
			configPOST_SLEEP_PROCESSING( xExpectedIdleTime );
			// 当代码执行到这里，说明已经退出了低功耗模式
			/* Stop SysTick.  Again, the time the SysTick is stopped for is
			accounted for as best it can be, but using the tickless mode will
			inevitably result in some tiny drift of the time maintained by the
			kernel with respect to calendar time. */
			ulSysTickCTRL = portNVIC_SYSTICK_CTRL_REG;
			portNVIC_SYSTICK_CTRL_REG = ( ulSysTickCTRL & ~portNVIC_SYSTICK_ENABLE_BIT );//重新禁用systick中断，那么__enable_irq()之后如果是systick中断激活了__wfi()，SysTick中断处理函数将不会被执行

			/* Re-enable interrupts - see comments above __disable_irq() call
			above. */
			__enable_irq();//此处开中断之后，如果是除systick中断之外的其他中断唤醒了__wfi，那么相应的中断处理函数会被立即执行
			//判断导致退出低功耗的是外部中断，还是滴答定时器计时时间到了
			if( ( ulSysTickCTRL & portNVIC_SYSTICK_COUNT_FLAG_BIT ) != 0 )//通过控制寄存器的第16位COUNT判断是否是SysTick中断到来
			{
				uint32_t ulCalculatedLoadValue;

				/* The tick interrupt has already executed, and the SysTick
				count reloaded with ulReloadValue.  Reset the
				portNVIC_SYSTICK_LOAD_REG with whatever remains of this tick
				period. */
				ulCalculatedLoadValue = ( ulTimerCountsForOneTick - 1UL ) - ( ulReloadValue - portNVIC_SYSTICK_CURRENT_VALUE_REG );

				/* Don't allow a tiny value, or values that have somehow
				underflowed because the post sleep hook did something
				that took too long. */
				if( ( ulCalculatedLoadValue < ulStoppedTimerCompensation ) || ( ulCalculatedLoadValue > ulTimerCountsForOneTick ) )
				{
					ulCalculatedLoadValue = ( ulTimerCountsForOneTick - 1UL );
				}

				portNVIC_SYSTICK_LOAD_REG = ulCalculatedLoadValue;

				/* The tick interrupt handler will already have pended the tick
				processing in the kernel.  As the pending tick will be
				processed as soon as this function exits, the tick value
				maintained by the tick is stepped forward by one less than the
				time spent waiting. */
				ulCompleteTickPeriods = xExpectedIdleTime - 1UL;
			}
			else//外部中断唤醒的，需要进行时间补偿
			{
				/* Something other than the tick interrupt ended the sleep.
				Work out how long the sleep lasted rounded to complete tick
				periods (not the ulReload value which accounted for part
				ticks). */
				ulCompletedSysTickDecrements = ( xExpectedIdleTime * ulTimerCountsForOneTick ) - portNVIC_SYSTICK_CURRENT_VALUE_REG;

				/* How many complete tick periods passed while the processor
				was waiting? */
				ulCompleteTickPeriods = ulCompletedSysTickDecrements / ulTimerCountsForOneTick;

				/* The reload value is set to whatever fraction of a single tick
				period remains. */
				portNVIC_SYSTICK_LOAD_REG = ( ( ulCompleteTickPeriods + 1UL ) * ulTimerCountsForOneTick ) - ulCompletedSysTickDecrements;
			}

			/* Restart SysTick so it runs from portNVIC_SYSTICK_LOAD_REG
			again, then set portNVIC_SYSTICK_LOAD_REG back to its standard
			value.  The critical section is used to ensure the tick interrupt
			can only execute once in the case that the reload register is near
			zero. */
			portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;
			portENTER_CRITICAL();
			{
				portNVIC_SYSTICK_CTRL_REG |= portNVIC_SYSTICK_ENABLE_BIT;
				vTaskStepTick( ulCompleteTickPeriods );
				portNVIC_SYSTICK_LOAD_REG = ulTimerCountsForOneTick - 1UL;
			}
			portEXIT_CRITICAL();
		}
	}

#endif /* #if configUSE_TICKLESS_IDLE */

/*-----------------------------------------------------------*/

/*Systick寄存器值设置函数，会在xPortStartScheduler函数中被调用*/
#if configOVERRIDE_DEFAULT_TICK_CONFIGURATION == 0

	void vPortSetupTimerInterrupt( void )
	{
		/* Calculate the constants required to configure the tick interrupt. */
		#if configUSE_TICKLESS_IDLE == 1
		{
			ulTimerCountsForOneTick = ( configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ );
			xMaximumPossibleSuppressedTicks = portMAX_24_BIT_NUMBER / ulTimerCountsForOneTick;
			ulStoppedTimerCompensation = portMISSED_COUNTS_FACTOR / ( configCPU_CLOCK_HZ / configSYSTICK_CLOCK_HZ );
		}
		#endif /* configUSE_TICKLESS_IDLE */

		/* Configure SysTick to interrupt at the requested rate. */
		portNVIC_SYSTICK_LOAD_REG = ( configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ ) - 1UL;
		portNVIC_SYSTICK_CTRL_REG = ( portNVIC_SYSTICK_CLK_BIT | portNVIC_SYSTICK_INT_BIT | portNVIC_SYSTICK_ENABLE_BIT );
	}

#endif /* configOVERRIDE_DEFAULT_TICK_CONFIGURATION */
/*-----------------------------------------------------------*/
/*返回当前中断的中断号，通过r0寄存器返回*/
__asm uint32_t vPortGetIPSR( void )
{
	PRESERVE8

	mrs r0, ipsr
	bx r14
}
/*-----------------------------------------------------------*/


/*主要目的是验证当前(用户)中断的优先级设置是否符合 FreeRTOS 的要求*/
#if( configASSERT_DEFINED == 1 )

	void vPortValidateInterruptPriority( void )
	{
	uint32_t ulCurrentInterrupt;
	uint8_t ucCurrentPriority;

		/* 获取当前中断号 */
		ulCurrentInterrupt = vPortGetIPSR();

		/* Is the interrupt number a user defined interrupt? */
		if( ulCurrentInterrupt >= portFIRST_USER_INTERRUPT_NUMBER )
		{
			/* Look up the interrupt's priority. */
			ucCurrentPriority = pcInterruptPriorityRegisters[ ulCurrentInterrupt ];

			/* 用户配置的中断优先级数值上必须>= ucMaxSysCallPriority，也即用户配置的中断优先级必须<=系统可管理的优先级*/
			configASSERT( ucCurrentPriority >= ucMaxSysCallPriority );
		}

		/* 断言检查优先级分组设置是否符合要求*/
		configASSERT( ( portAIRCR_REG & portPRIORITY_GROUP_MASK ) <= ulMaxPRIGROUPValue );
	}

#endif /* configASSERT_DEFINED */


