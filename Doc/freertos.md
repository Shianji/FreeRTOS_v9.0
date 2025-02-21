###### list.c和list.h

list.h定义了与链表有关的数据结构，主要有typedef struct xLIST_ITEM ListItem_t定义了链表节点，typedef struct xMINI_LIST_ITEM MiniListItem_t定义了一个“小节点”，用来在list结构中存储尾节点，typedef struct xLIST list定义了链表。宏configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES用来控制是否在上述结构体中加入校验值（某些特定值，在使用上述结构体时会检查这些值是否被修改来说明是否发生错误），其值默认是0表示不加入校验值。

list.c定义了双向链表操作。主要函数有：

- 链表初始化函数vListInitialise：将pxIndex指向链表中的尾节点MiniListItem_t类型的xListEnd，并将尾节点的xItemValue值设置为最大值portMAX_DELAY，将尾节点的前驱节点指针后后继节点指针都指向尾节点自己，将链表成员数uxNumberOfItems设置为0；
- 链表尾插函数vListInsertEnd：将新节点插入到pxIndex所指向节点的前一个位置；
- 链表插入函数vListInsert：按照待插入节点的xItemValue值，将其按照升序插入链表，如果有节点有相同的值，则插入在具有同样值的节点的最后面；
- 节点删除函数uxListRemove：删除链表中对应的节点，如果pxIndex指向待删除的节点，则将pxIndex重新指向前一个节点，修改uxNumberOfItems的值并返回uxNumberOfItems的值。

###### portmacro.h

主要定一些宏定义及开关中断的函数，这些宏定义和函数是与具体架构相关的，文件内容主要包括：

- 宏定义portYIELD()将portNVIC_INT_CTRL_REG中断控制及状态寄存器的portNVIC_PENDSVSET_BIT位置1，可触发PendSV，产生上下文切换；
- 宏定义portRECORD_READY_PRIORITY、portRESET_READY_PRIORITY、portGET_HIGHEST_PRIORITY利用clz指令来设置、清除相关任务的优先级和获取系统当前就绪任务最高优先级；
- static portFORCE_INLINE void vPortSetBASEPRI( uint32_t ulBASEPRI )函数可以设置basepri寄存器的值，以此来打开或关闭中断；
- static portFORCE_INLINE void vPortRaiseBASEPRI( void )函数用来关中断，不带返回值所以不能嵌套；
- static portFORCE_INLINE void vPortClearBASEPRIFromISR( void )将寄存器basepri的值设置为0，以此来开启中断；
- static portFORCE_INLINE uint32_t ulPortRaiseBASEPRI( void )是关中断函数，带返回值所以可以嵌套，返回的是之前的basepri寄存器的值；
- static portFORCE_INLINE BaseType_t xPortIsInsideInterrupt( void )函数用来判断当前是否是在中断上下文中，若在中断上下文种则返回pdTRUE否则返回pdFALSE，ipsr寄存器中存储了当前执行的中断号。

###### port.c

主要是一些宏定义和中断处理函数（与具体架构相关的汇编函数），文件内容主要包括：

- StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters )函数用来初始化任务栈，这个函数相当于模拟了任务正常运行时中断发生任务备切换后，任务栈中硬件自动操作后的结果，这个函数只有在任务刚被创建时才会被调用；
- static void prvTaskExitError( void )是任务函数的返回地址所指向的函数(会在初始化任务栈时入写任务栈中，但Freertos的任务函数一般不会返回，所以一般不会运行到该函数处)；
- asm void vPortSVCHandler( void )是svc中断处理函数，在系统启动后调用第一个任务时是通过svc 0指令触发svc中断然后调用该函数开始执行第一个任务的；
- asm void prvStartFirstTask( void )函数用来启动系统中的第一个任务，他最后会调用svc 0；
- BaseType_t xPortStartScheduler( void )函数在调度器中被调用，用来在一开始设置PendSV和Systick的中断优先级，获取当前系统中的优先级分组相关信息，另外调用了vPortSetupTimerInterrupt()初始化了定时器，并调用prvStartFirstTask()开始第一个任务的执行；
- void vPortEnterCritical( void )函数用来进入临界区，他不能在中断处理函数中被调用；
- void vPortExitCritical( void )函数用来退出临界区，他不能在中断处理函数中被调用；
- asm void xPortPendSVHandler( void )是PendSV的中断处理函数，他会调用vTaskSwitchContext改变pxCurrentTCB指针的指向，完成任务的切换；
- void xPortSysTickHandler( void )是SysTick的中断处理函数，他会进入临界区然后调用xTaskIncrementTick()函数并根据返回值判断是否引起PendSV中断切换任务；
- weak void vPortSuppressTicksAndSleep( TickType_t xExpectedIdleTime )该函数在设置了宏configUSE_TICKLESS_IDLE时，即提供低功耗运行功能时，可以根据需要使系统低功耗运行相应的tick；
- void vPortSetupTimerInterrupt( void )函数用来设置systick相关的控制、重装载等寄存器的值；
- __asm uint32_t vPortGetIPSR( void )函数用来获取ipsr寄存器的值(主要用于指示当前的中断处理状态)；
- void vPortValidateInterruptPriority( void )函数用于验证当前(用户)中断的优先级设置是否符合 FreeRTOS 的要求。

###### FreeRTOS.h和FreeRTOSConfig.h

FreeRTOS.h中主要是提供了一些可以配置的宏定义，用户可以根据需要在FreeRTOSConfig.h（FreeRTOSConfig.h是用户自定义的配置文件）根据实际应用需要进行配置。FreeRTOS.h中的StaticTask_t、StaticQueue_t、StaticEventGroup_t等结构体定义主要用于静态分配内存时使用，这些结构体和对应的实际定义的任务控制块、消息队列、事件组等结构体的大小是相同的，在静态分配内存时可以用来提前占据空间。

###### portable.h

该头文件主要包含一些与内存分配相关的宏定义和函数声明，如：宏定义portBYTE_ALIGNMENT表示内存对齐要；pvPortMalloc、vPortFree、vPortInitialiseBlocks分别是内存分配、回收和内存块初始化函数。

###### task.c和task.h

任务的结构体中的xStateListItem成员是一个链表项，可以被挂载到系统的就绪链表或者阻塞链表中，系统中的就绪链表共有pxReadyTasksLists[ configMAX_PRIORITIES ]个按照任务优先级排列，阻塞链表（也可叫做延时链表）有两个xDelayedTaskList1和xDelayedTaskList2，设置两个是为了在tick计数溢出时切换。任务的结构体中的xEventListItem成员也是一个链表项，可以被挂载到任务等待的事件链表上，这些事件链表可以是消息队列中的xTasksWaitingToSend和xTasksWaitingToReceive（表示任务在等待消息、信号量或互斥量），也可以是事件组中的xTasksWaitingForBits（表示任务在等待事件组相应位的事件发生）。xEventListItem还可以被挂载到xPendingReadyList上表示调度器暂停期间被恢复为就绪状态的任务。这两个文件中主要包括与任务task相关的一些结构体和函数定义，文件内容包括：

- 宏定义configUSE_PORT_OPTIMISED_TASK_SELECTION用于选择是使用通用方法还是使用优化方法设置当前就绪任务优先级，优化方法借助clz指令获取当前就绪任务的最高优先级，且采用优先级位图记录系统当前就绪任务的优先级集合。
- 宏定义taskSWITCH_DELAYED_LISTS()在系统tick计数发生溢出时进行pxDelayedTaskList和pxOverflowDelayedTaskList的切换。
- 宏定义prvAddTaskToReadyList( pxTCB )将就绪任务插入对应的优先级就绪链表（不排序插入最后面），并更新当前系统就绪任务的最高优先级或更新系统优先级位图，此处并没有改变pxCurrentTCB的指向。
- typedef struct tskTaskControlBlock定义了任务控制块结构体，结构体包含任务栈顶指针pxTopOfStack、任务优先级uxPriority、任务栈(指向栈底)指针pxStack、任务名称pcTaskName等，注意任务栈的sp是按照4个字节移动的。
- TaskHandle_t xTaskCreateStatic(   TaskFunction_t pxTaskCode,const char * const pcName,const uint32_t ulStackDepth,void * const pvParameters,UBaseType_t uxPriority, StackType_t * const puxStackBuffer, StaticTask_t * const pxTaskBuffer )是静态任务创建函数，创建一个新任务并将其加入任务就绪列表。
- BaseType_t xTaskCreate(   TaskFunction_t pxTaskCode,const char * const pcName,const uint16_t usStackDepth,void * const pvParameters,UBaseType_t uxPriority,TaskHandle_t * const pxCreatedTask )是动态任务创建函数，创建一个新任务并将其加入任务就绪列表，任务所要用到的栈空间和控制块TCB的空间都是pvPortMalloc动态分配的。
- static void prvInitialiseNewTask(  TaskFunction_t pxTaskCode, const char * const pcName, const uint32_t ulStackDepth, void * const pvParameters,UBaseType_t uxPriority, TaskHandle_t * const pxCreatedTask,TCB_t *pxNewTCB, const MemoryRegion_t * const xRegions )是任务TCB结构体初始化函数，设置成功会将pxCreatedTask指向pxNewTCB，而pxNewTCB指向新初始化的任务TCB，这个函数是无法被用户调用的。
- static void prvAddNewTaskToReadyList( TCB_t *pxNewTCB )函数将新任务加入对应的优先级就绪链表，并根据当前调度器的运行状态进行调度。
- void vTaskDelete( TaskHandle_t xTaskToDelete )是任务删除函数，会将目标任务从就绪链表或延时链表以及事件链表删除（若果任务在等待消息队列的消息的话），若传入NULL则会删除当前正在运行的任务pxCurrentTCB。
- void vTaskDelayUntil( TickType_t * const pxPreviousWakeTime, const TickType_t xTimeIncrement )是绝对时间任务阻塞函数，将当前任务相对于其上一次阻塞时间pxPreviousWakeTime阻塞xTimeIncrement 。
- void vTaskDelay( const TickType_t xTicksToDelay )是相对时间任务阻塞函数，将当前任务相对于当前系统时间xTickCount阻塞xTicksToDelay。
- eTaskState eTaskGetState( TaskHandle_t xTask )函数用于获取目标任务的当前状态。
- UBaseType_t uxTaskPriorityGet( TaskHandle_t xTask )函数用于获取目标任务的优先级，传入NULL获取当前任务的优先级。
- UBaseType_t uxTaskPriorityGetFromISR( TaskHandle_t xTask )函数用于获取目标任务的优先级（用于中断中的），传入NULL获取当前任务的优先级。
- void vTaskPrioritySet( TaskHandle_t xTask, UBaseType_t uxNewPriority )函数用于设置目标任务的优先级，传入NULL设置当前任务的优先级。
- void vTaskSuspend( TaskHandle_t xTaskToSuspend )函数用于将传入的任务挂起，被挂起的任务不会再参与调度，若传入NULL则会挂起当前正在运行的任务pxCurrentTCB。
- static BaseType_t prvTaskIsTaskSuspended( const TaskHandle_t xTask )函数用于判断一个任务是否被挂起了，若任务被挂起了则返回pdTRUE。
- void vTaskResume( TaskHandle_t xTaskToResume )函数用于将挂起的任务恢复就绪状态，无论任务被挂起多少次，只需要调用该恢复函数一次即可恢复到就绪态。
- BaseType_t xTaskResumeFromISR( TaskHandle_t xTaskToResume )函数是可在中断中使用的，将被挂起的任务唤醒并转换为就绪状态的函数。
- void vTaskStartScheduler( void )是调度器函数，创建一个IDLE任务（在创建任务的函数中会发现这是系统第一个任务所以会将pxCurrentTCB指向IDLE），该函数会调用xPortStartScheduler函数启动第一个任务，也就是IDLE（如果在调用该函数之前没创建其他任务的话），并开始调度。该函数还会根据配置选择是否创建软件定时器任务。
- void vTaskSuspendAll( void )函数用于暂停调度器。
- BaseType_t xTaskResumeAll( void )函数用于将调度器重新唤醒,调用了多少次vTaskSuspendAll就要调用多少次xTaskResumeAll才能唤醒调度器。
- TickType_t xTaskGetTickCount( void )函数返回当前系统的tick计数时间值。
- TickType_t xTaskGetTickCountFromISR( void )函数返回当前系统的tick计数时间值（用于中断中的）。
- UBaseType_t uxTaskGetNumberOfTasks( void )函数用于获取当前系统总任务数。
- char *pcTaskGetName( TaskHandle_t xTaskToQuery )函数用于获取任务控制块的名字。
- BaseType_t xTaskIncrementTick( void )函数用于增加tick计数，时钟中断来临时，systick中断处理函数会调用该函数，视情况选择是否设置portNVIC_PENDSVSET_BIT进行任务调度。
- void vTaskSwitchContext( void )是任务切换函数，调度器不在运行则不允许进行上下文切换。
- void vTaskPlaceOnEventList( List_t * const pxEventList, const TickType_t xTicksToWait )函数用于将当前任务插入等待目标事件（可以是消息、信号量、事件组等）的链表，并将当前任务加入延时链表，任务可以被设置成永久阻塞地等待事件。
- void vTaskPlaceOnUnorderedEventList( List_t * pxEventList, const TickType_t xItemValue, const TickType_t xTicksToWait )函数用于将当前任务加入到事件组（这里等待的事件是事件组）的阻塞等待链表中，并将当前任务加入到延时链表中。
- void vTaskPlaceOnEventListRestricted( List_t * const pxEventList, TickType_t xTicksToWait, const BaseType_t xWaitIndefinitely )函数用于将当前任务加入指定的事件链表，并将当前任务加入延时链表，xWaitIndefinitely为pdTRUE则会无限阻塞。
- BaseType_t xTaskRemoveFromEventList( const List_t * const pxEventList )函数用于将pxEventList第一个链表项移出并加入就绪队列（也即优先级最高的那个链表项）。
- BaseType_t xTaskRemoveFromUnorderedEventList( ListItem_t * pxEventListItem, const TickType_t xItemValue )函数用于将阻塞在事件组（这里等待的事件是事件组）上的任务移除，并将其从延迟链表移除并添加到就绪链表。
- void vTaskSetTimeOutState( TimeOut_t * const pxTimeOut )函数用于设置等待时间的结构体。
- BaseType_t xTaskCheckForTimeOut( TimeOut_t * const pxTimeOut, TickType_t * const pxTicksToWait )函数用于检查任务等待消息队列满足要求的等待时间是否到达，pxTimeOut是之前记录的时间结构体，pxTicksToWait是可以等待的时间，返回pdTRUE表示到达时间了。
- static portTASK_FUNCTION( prvIdleTask, pvParameters )是IDLE任务对应的函数，portTASK_FUNCTION是宏定义，转换后为void prvIdleTask( void *pvParameters )。
- static void prvInitialiseTaskLists( void )函数用于初始化任务相关的所有链表的函数，用于初始化调度程序使用的所有链表的函数，在创建第一个任务时会自动调用此函数。
- static void prvCheckTasksWaitingTermination( void )函数用于检查当前是否有任务待删除，这个函数只会被IDLE任务的函数调用。
- static void prvDeleteTCB( TCB_t *pxTCB )是任务栈和TCB内存空间回收函数，可根据配置释放之前分配的内存空间。
- static void prvResetNextTaskUnblockTime( void )函数用于获取下一个任务唤醒的时间。
- TaskHandle_t xTaskGetCurrentTaskHandle( void )函数用于获取当前正在运行的任务。
- BaseType_t xTaskGetSchedulerState( void )函数用于获取系统调度器的状态。
- void vTaskPriorityInherit( TaskHandle_t const pxMutexHolder )函数用于实现互斥量的优先级翻转机制。
- BaseType_t xTaskPriorityDisinherit( TaskHandle_t const pxMutexHolder )函数用于将之前的优先级继承恢复。
- TickType_t uxTaskResetEventItemValue( void )函数用于重新设置任务控制块中事件链表项的值，将其设置为( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) pxCurrentTCB->uxPriority，说明此时已经不再用于等待事件了，返回值为当前任务等待的目标事件。
- void *pvTaskIncrementMutexHeldCount( void )函数用将当前正在运行任务持有的互斥量加1。
- uint32_t ulTaskNotifyTake( BaseType_t xClearCountOnExit, TickType_t xTicksToWait )是任务通知值获取函数（用于信号量），因为任务通知值只属于任务本身，所以它是私有的，xClearCountOnExit设置读取后是否清0，xTicksToWait设置阻塞时间。
- BaseType_t xTaskNotifyWait( uint32_t ulBitsToClearOnEntry, uint32_t ulBitsToClearOnExit, uint32_t *pulNotificationValue, TickType_t xTicksToWait )是任务通知值获取函数（该函数一般用于将任务通知用作事件或消息时），因为任务通知值只属于任务本身，所以它是私有的，ulBitsToClearOnEntry用来设置进入时需要清除的位，ulBitsToClearOnExit用来设置退出时需要清除的位，pulNotificationValue获取原本的任务通知值，xTicksToWait设置阻塞时间。
- BaseType_t xTaskGenericNotify( TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction, uint32_t *pulPreviousNotificationValue )是任务通知发送函数，xTaskToNotify为任务句柄，ulValue为发送的值，eAction为更新通知的方式，pulPreviousNotificationValue用于获取原本的任务通知的值，若传入NULL则不获取 。
- BaseType_t xTaskGenericNotifyFromISR( TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction, uint32_t *pulPreviousNotificationValue, BaseType_t *pxHigherPriorityTaskWoken )函数用于在中断中向一个任务发送任务通知值。
- void vTaskNotifyGiveFromISR( TaskHandle_t xTaskToNotify, BaseType_t *pxHigherPriorityTaskWoken )是任务通知值发送函数（用于中断中且通知用作信号量）。
- BaseType_t xTaskNotifyStateClear( TaskHandle_t xTask )函数用于将任务的通知状态设置为taskNOT_WAITING_NOTIFICATION。

###### queue.c和queue.h

主要包括与消息队列queue相关的宏定义和函数，消息队列可用来传递消息（任意大小）、信号量（信号量值为0-n）、互斥量，以实现任务间的通信，文件内容主要包括：

- xQueueRegistry[ configQUEUE_REGISTRY_SIZE ]是一个全局的数组，数组成员为指向各个消息队列的句柄。
- 宏定义prvLockQueue( pxQueue )用于将消息队列锁定，而不允许在中断中操作阻塞的事件链表。
- BaseType_t xQueueGenericReset( QueueHandle_t xQueue, BaseType_t xNewQueue )函数用于重置（清空）消息队列，xQueue为待重置的消息队列，xNewQueue若为pdTRUE则表示是初始化消息队列时调用的该函数。
- QueueHandle_t xQueueGenericCreateStatic( const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, uint8_t *pucQueueStorage, StaticQueue_t *pxStaticQueue, const uint8_t ucQueueType )是消息队列的静态创建函数，uxQueueLength为消息队列长度（即数据队列可包含消息的个数），uxItemSize为单个消息大小，pucQueueStorage指向消息存储区域，pxStaticQueue指向待初始化的消息队列控制块，ucQueueType为消息类型。
- QueueHandle_t xQueueGenericCreate( const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, const uint8_t ucQueueType )是消息队列动态创建函数，uxQueueLength为消息队列长度（即数据队列可包含消息的个数），uxItemSize为单个消息大小，ucQueueType为消息类型。
- static void prvInitialiseNewQueue( const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, uint8_t *pucQueueStorage, const uint8_t ucQueueType, Queue_t *pxNewQueue )是消息队列初始化函数，会在消息队列的静态或动态创建函数中被调用，uxQueueLength为消息队列长度（即数据队列可包含消息的个数），uxItemSize为单个消息大小，pucQueueStorage指向消息存储区域，ucQueueType为消息类型，pxNewQueue指向待初始化的消息队列控制块。
- static void prvInitialiseMutex( Queue_t *pxNewQueue )函数用于初始化互斥量，需要将pxNewQueue->uxQueueType即pxNewQueue->head设置为NULL表示当前消息队列为互斥量。
- QueueHandle_t xQueueCreateMutex( const uint8_t ucQueueType )函数用于动态创建互斥量（包括普通互斥量和递归互斥量），ucQueueType指明互斥量类型。
- QueueHandle_t xQueueCreateMutexStatic( const uint8_t ucQueueType, StaticQueue_t *pxStaticQueue )函数用于静态创建互斥量（包括普通互斥量和递归互斥量），ucQueueType指明互斥量类型。
- BaseType_t xQueueGiveMutexRecursive( QueueHandle_t xMutex )函数用于释放递归互斥量。
- BaseType_t xQueueTakeMutexRecursive( QueueHandle_t xMutex, TickType_t xTicksToWait )函数用于获取递归互斥量。
- QueueHandle_t xQueueCreateCountingSemaphoreStatic( const UBaseType_t uxMaxCount, const UBaseType_t uxInitialCount, StaticQueue_t *pxStaticQueue )函数用于静态创建计数信号量并初始化信号量的值为uxInitialCount。
- QueueHandle_t xQueueCreateCountingSemaphore( const UBaseType_t uxMaxCount, const UBaseType_t uxInitialCount )函数用于动态创建计数信号量，并将信号量初始值设置为uxInitialCount，uxMaxCount为信号量最大值。
- BaseType_t xQueueGenericSend( QueueHandle_t xQueue, const void * const pvItemToQueue, TickType_t xTicksToWait, const BaseType_t xCopyPosition )是通用的消息发送函数（发送到消息队列，消息队列可以存储消息、信号量或互斥量），xQueue为目标消息队列，pvItemToQueue指向待发送的消息，xTicksToWait为等待时间，xCopyPosition为写入消息的位置。
- BaseType_t xQueueGenericSendFromISR( QueueHandle_t xQueue, const void * const pvItemToQueue, BaseType_t * const pxHigherPriorityTaskWoken, const BaseType_t xCopyPosition )是通用的（中断中的，消息队列可以存储消息）消息发送函数（发送到消息队列），中断期间不能延时等待，xQueue为目标消息队列，pvItemToQueue指向待发送的消息，pxHigherPriorityTaskWoken指示退出函数后是否要进行上下文切换，xCopyPosition为写入消息的位置。
- BaseType_t xQueueGiveFromISR( QueueHandle_t xQueue, BaseType_t * const pxHigherPriorityTaskWoken )函数用于在中断中使用，用于释放信号量，使得信号量的值加1。
- BaseType_t xQueueGenericReceive( QueueHandle_t xQueue, void * const pvBuffer, TickType_t xTicksToWait, const BaseType_t xJustPeeking )是通用的消息接收函数（从消息队列接收，可用于获取消息、信号量或互斥量），xQueue为目标消息队列，pvBuffer指向接收的消息存储目标地址，xTicksToWait为等待时间，xJustPeeking指明是只读取还是读取后删除消息（传入pdFALSE表示读取后删除）。
- BaseType_t xQueueReceiveFromISR( QueueHandle_t xQueue, void * const pvBuffer, BaseType_t * const pxHigherPriorityTaskWoken )是通用的（中断中的）消息接收函数（从消息队列接收，可用于获取消息、信号量），中断期间不能延时等待,xQueue为目标消息队列，pvBuffer指向接收的消息存储目标地址，pxHigherPriorityTaskWoken指示退出函数后是否要进行上下文切换。
- BaseType_t xQueuePeekFromISR( QueueHandle_t xQueue,  void * const pvBuffer )函数用于在中断中从消息队列读取一个消息，读取后不删除该消息。
- UBaseType_t uxQueueMessagesWaiting( const QueueHandle_t xQueue )函数用于获取当前消息队列中的消息数量。
- UBaseType_t uxQueueSpacesAvailable( const QueueHandle_t xQueue )函数用于获取目标消息队列中还能容纳的消息数，即空闲空间。
- void vQueueDelete( QueueHandle_t xQueue )是消息队列删除函数。
- static BaseType_t prvCopyDataToQueue( Queue_t * const pxQueue, const void *pvItemToQueue, const BaseType_t xPosition )函数用于将消息写入到目标消息队列的目标位置，pxQueue是目标消息队列，pvItemToQueue是待写入的消息，xPosition指定待写入的位置。
- static void prvCopyDataFromQueue( Queue_t * const pxQueue, void * const pvBuffer )函数将pxQueue队列中的消息复制到pvBuffer中，实现recieve操作。
- static void prvUnlockQueue( Queue_t * const pxQueue )函数用于给队列解锁，解锁状态下的消息队列中的两个阻塞链表才可以在中断中被修改，只有在调度器暂停的时候才能调用该函数。
- static BaseType_t prvIsQueueEmpty( const Queue_t *pxQueue )函数用于判断当前队列是否为空，若是返回pdTRUE。
- static BaseType_t prvIsQueueFull( const Queue_t *pxQueue )函数用于判断当前队列是否已满，若是返回pdTRUE。
- void vQueueAddToRegistry( QueueHandle_t xQueue, const char *pcQueueName ) 函数用于将消息队列注册到全局的消息队列数组中。
- const char *pcQueueGetName( QueueHandle_t xQueue )函数用于获取消息队列句柄对应的消息队列名字。
- void vQueueUnregisterQueue( QueueHandle_t xQueue )函数用于从全局的消息队列数组中将相应的消息队列注销。
- void vQueueWaitForMessageRestricted( QueueHandle_t xQueue, TickType_t xTicksToWait, const BaseType_t xWaitIndefinitely )若目标队列为空，则该函数会将当前任务加入等待事件链表并将当前任务阻塞。

###### semphr.h

该文件中主要是与信号量和互斥量相关的宏定义及函数声明，信号量和互斥量实际上还是通过消息队列实现的，此时的消息队列中的消息大小为0。

###### event_groups.c和event_groups.h

主要包括与事件组EventGroup相关的一些结构体和函数定义，文件内容包括：

- EventGroupHandle_t xEventGroupCreateStatic( StaticEventGroup_t *pxEventGroupBuffer )是静态事件组创建函数，返回创建的事件组句柄。
- EventGroupHandle_t xEventGroupCreate( void )是动态事件组创建函数，返回创建的事件组句柄。
- EventBits_t xEventGroupSync( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet, const EventBits_t uxBitsToWaitFor, TickType_t xTicksToWait )函数的作用是将事件组的某些位uxBitsToSet置位，然后判断当前任务等待的事件位uxBitsToWaitFor是否满足，不满足则阻塞当前任务xTicksToWait。
- EventBits_t xEventGroupWaitBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToWaitFor, const BaseType_t xClearOnExit, const BaseType_t xWaitForAllBits, TickType_t xTicksToWait )是事件等待函数，参数xEventGroup为事件组句柄，uxBitsToWaitFor指明要等待的事件（对应位置1），xClearOnExit指示事件满足后是否清除相应位，xWaitForAllBits指示是等待所有事件还是任一事件（即是与还是或），xTicksToWait为阻塞时间，该函数返回一个事件组状态，还是需要根据返回值判断是当前任务等待的事件满足了返回的还是超时返回的。
- EventBits_t xEventGroupClearBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear )是事件组的位清除函数，可以将指定位uxBitsToClear清除。
- BaseType_t xEventGroupClearBitsFromISR( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear )是事件组位清除函数（在中断中使用），清除事件组中的标志位是一个不确定的操作（可能耗时很长），FreeRTOS不允许不确定的操作在中断或临界区中发生，所以通过软件定时器实现。
- EventBits_t xEventGroupGetBitsFromISR( EventGroupHandle_t xEventGroup )函数在中断在使用，用于获取目标事件组的事件位的状态。
- EventBits_t xEventGroupSetBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet )是事件组置位函数，并唤醒在置位相关位之后应解除阻塞的任务。
- void vEventGroupDelete( EventGroupHandle_t xEventGroup )是事件组删除函数，可将目标事件组删除。
- static BaseType_t prvTestWaitCondition( const EventBits_t uxCurrentEventBits, const EventBits_t uxBitsToWaitFor, const BaseType_t xWaitForAllBits )函数用来判断任务等待的事件与已经发生的事件是否已经匹配。
- BaseType_t xEventGroupSetBitsFromISR( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet, BaseType_t *pxHigherPriorityTaskWoken )是事件组置位函数（在中断中使用），置位事件组中的标志位是一个不确定的操作（可能耗时很长），FreeRTOS不允许不确定的操作在中断或临界区中发生，所以通过软件定时器实现。

###### timers.c和timers.h

系统中共设置了两个软件定时器延时链表xActiveTimerList1和xActiveTimerList2，链表上挂载的是将要执行的定时器，另外系统中还有一个软件定时器命令队列，队列上是发送给软件定时器的任务（这个队列可以在tick计数溢出时协助完成软件定时器的设置）。软件定时器任务的优先级是最高的，所以软件定时器任务总是最先被执行的。这两个文件中主要包括与软件定时器timers相关的宏定义和函数，文件内容主要包括：

- BaseType_t xTimerCreateTimerTask( void )是软件定时器任务的创建函数，prvTimerTask为定时器任务对应的函数。
- TimerHandle_t xTimerCreate(   const char * const pcTimerName,const TickType_t xTimerPeriodInTicks,const UBaseType_t uxAutoReload,void * const pvTimerID,TimerCallbackFunction_t pxCallbackFunction ) 是软件定时器动态创建函数，返回所创建的软件定时器的句柄。
- TimerHandle_t xTimerCreateStatic(  const char * const pcTimerName,const TickType_t xTimerPeriodInTicks,const UBaseType_t uxAutoReload,void * const pvTimerID,TimerCallbackFunction_t pxCallbackFunction,StaticTimer_t *pxTimerBuffer )是软件定时器静态创建函数，返回所创建的软件定时器的句柄。
- static void prvInitialiseNewTimer(  const char * const pcTimerName,const TickType_t xTimerPeriodInTicks,const UBaseType_t uxAutoReload,void * const pvTimerID,TimerCallbackFunction_t pxCallbackFunction,Timer_t *pxNewTimer )是软件定时器初始化函数。
- BaseType_t xTimerGenericCommand( TimerHandle_t xTimer, const BaseType_t xCommandID, const TickType_t xOptionalValue, BaseType_t * const pxHigherPriorityTaskWoken, const TickType_t xTicksToWait )是定时器命令发送函数，xTimer为定时器句柄，xCommandID代表发送的具体命令，这些命令会被添加到全局的定时器命令队列xTimerQueue上。
- TaskHandle_t xTimerGetTimerDaemonTaskHandle( void )函数用于返回定时器任务句柄。
- TickType_t xTimerGetPeriod( TimerHandle_t xTimer )函数用于获取目标定时器的定时周期。
- TickType_t xTimerGetExpiryTime( TimerHandle_t xTimer )函数用于获取定时器结构体中的xTimerListItem链表项上的值。
- const char * pcTimerGetName( TimerHandle_t xTimer )函数用于获取目标定时器的名字。
- static void prvProcessExpiredTimer( const TickType_t xNextExpireTime, const TickType_t xTimeNow )函数用于在当前时间大于定时器应该被响应时间时调用，传入的xNextExpireTime应该是小于等于xTimeNow的，该函数会执行定时器注册的回调函数，并根据情况判断是否要发送命令到全局的定时器命令队列xTimerQueue上。
- static void prvTimerTask( void *pvParameters )是软件定时器任务对应的函数。
- static void prvProcessTimerOrBlockTask( const TickType_t xNextExpireTime, BaseType_t xListWasEmpty )函数用于根据实际情况将当前的定时器任务阻塞到下一个定时器时间来临或执行所有到期的定时器的回调函数。
- static TickType_t prvGetNextExpireTime( BaseType_t * const pxListWasEmpty )函数用于获取当前定时器列表中下一个定时器到期时间。
- static TickType_t prvSampleTimeNow( BaseType_t * const pxTimerListsWereSwitched )函数判断当前系统tick计数是否溢出，并按照实际情况选择是否切换定时器计数链表。
- static BaseType_t prvInsertTimerInActiveList( Timer_t * const pxTimer, const TickType_t xNextExpiryTime, const TickType_t xTimeNow, const TickType_t xCommandTime )函数用于判断下一次定时器响应时间是否到达并做出相应的处理，xCommandTime是上一次定时器应该响应时间，xNextExpiryTime是下一次定时器应该响应时间，xTimeNow是当前时间，如果不存在溢出的话xCommandTime应该是小于xTimeNow的，这个函数能处理掉计数值溢出的问题，在prvProcessReceivedCommands中被调用实现定时器正常响应，很关键。
- static void prvProcessReceivedCommands( void )函数用于接收定时器命令，并根据情况响应执行回调函数。
- static void prvSwitchTimerLists( void )函数用来切换当前定时器列表，并执行所有到期的定时器的回调函数。
- static void prvCheckForValidListAndQueue( void )函数用来检查定时器相关的链表和用来传递消息的命令队列是否已经初始化，若未初始化则进行初始化。
- BaseType_t xTimerIsTimerActive( TimerHandle_t xTimer )函数用于判断目标定时器是否在使用。
- void *pvTimerGetTimerID( const TimerHandle_t xTimer )函数用于获取目标定时器ID。
- void vTimerSetTimerID( TimerHandle_t xTimer, void *pvNewID )函数用于设置目标定时器ID。
- BaseType_t xTimerPendFunctionCallFromISR( PendedFunction_t xFunctionToPend, void *pvParameter1, uint32_t ulParameter2, BaseType_t *pxHigherPriorityTaskWoken )函数用于在中断中设置执行回调函数的软件定时器，可供事件组的xEventGroupSetBitsFromISR和xEventGroupClearBitsFromISR函数使用

###### heap_*.c

heap_*.c这几个文件主要实现内存管理，void *pvPortMalloc( size_t xWantedSize )函数用来分配内存，分配成功会返回分配的内存的首地址；void vPortFree( void *pv )函数用来释放已分配的内存；size_t xPortGetFreeHeapSize( void )函数返回当前空闲的内存大小；static void prvHeapInit( void );为内存初始化函数。其中：heap_1.c的内存分配算法很简单，就是从前往后按照顺序分配内存，并且已经分配的内存无法被释放并回收；heap_2.c内存分配算法采用最佳分配算法，采用链表管理空闲内存块，按照内存大小升序排列，heap_2.c不能把相邻的两个小的内存块合并成一个大的内存块；heqp_3.c内存分配算法是利用标准C库中的malloc和free实现的；heap_4.c采用的是首次适应算法，空闲内存块链表是按照内存地址大小的升序排列的，若空闲内存相邻则会被合并，heap_4.c使用了一个标记位（xBlockAllocatedBit，用最高位作标记位表示当前内存块已分配与否），所以按道理来说总内存大小在地址空间为32位时不能大于2GB否则可能会出错；heap_5.c内存分配算法在heap_4.c分配算法基础上添加了支持多块物理上不连续的物理内存块的功能。

###### projdefs.h

这个文件中定义了一些通用的返回值，例如：pdFALSE、pdTRUE、pdPASS、pdFAIL等等。

###### StackMacros.h

这个文件中定义了一些和栈相关的宏定义，用于栈空间检查。

###### mpu_prototypes.h和mpu_wrappers.h

这两个文件是和MPU（Memory Protection Unit，内存保护单元）相关的。

###### croutine.c和croutine.h

这是和协程相关的文件，开启configUSE_CO_ROUTINES宏定义之后才会用到，这个宏定义默认是0表示不使用协程。