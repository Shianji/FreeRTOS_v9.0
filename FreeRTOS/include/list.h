/*
    FreeRTOS V9.0.0 - Copyright (C) 2016 Real Time Engineers Ltd.
*/

/*
 * This is the list implementation used by the scheduler.  While it is tailored
 * heavily for the schedulers needs, it is also available for use by
 * application code.
 *
 * list_ts can only store pointers to list_item_ts.  Each ListItem_t contains a
 * numeric value (xItemValue).  Most of the time the lists are sorted in
 * descending item value order.
 *
 * Lists are created already containing one list item.  The value of this
 * item is the maximum possible that can be stored, it is therefore always at
 * the end of the list and acts as a marker.  The list member pxHead always
 * points to this marker - even though it is at the tail of the list.  This
 * is because the tail contains a wrap back pointer to the true head of
 * the list.
 *
 * In addition to it's value, each list item contains a pointer to the next
 * item in the list (pxNext), a pointer to the list it is in (pxContainer)
 * and a pointer to back to the object that contains it.  These later two
 * pointers are included for efficiency of list manipulation.  There is
 * effectively a two way link between the object containing the list item and
 * the list item itself.
 *
 */

#ifndef INC_FREERTOS_H
	#error FreeRTOS.h must be included before list.h
#endif

#ifndef LIST_H
#define LIST_H

/*
链表结构成员是从中断内部修改的，因此理应声明为 volatile。但是，它们仅以功能原子方式进行修改（在调度程序暂停的关键部分内），
并且通过引用传递到函数中或通过 volatile 变量进行索引。因此，在迄今为止测试的所有用例中，可以省略 volatile 限定符，以提供适
度的性能改进，而不会对功能行为产生不利影响。当各自的编译器选项设置为最大优化时，IAR、ARM 和 GCC 编译器生成的汇编指令已经过检
查，并被认为符合预期。话虽如此，随着编译器技术的进步，尤其是如果使用积极的跨模块优化（尚未得到广泛应用的用例），那么可能需要 
volatile 限定符才能进行正确的优化。预计编译器会删除必要的代码，因为如果链表结构成员上没有 volatile 限定符，并且进行了积极的
跨模块优化，编译器会认为代码是不必要的，这将导致调度程序完全且明显失败。如果遇到这种情况，只需在 FreeRTOSConfig.h 中将 
configLIST_VOLATILE 定义为 volatile（按照此注释块底部的示例），即可将 volatile 限定符插入链表结构中的相关位置。如果未定义 
configLIST_VOLATILE，则下面的预处理器指令将完全 #define configLIST_VOLATILE。
要使用 volatile 链表结构成员，请将以下行添加到
FreeRTOSConfig.h（不带引号）：
“#define configLIST_VOLATILE volatile”
 */
#ifndef configLIST_VOLATILE
	#define configLIST_VOLATILE
#endif /* configSUPPORT_CROSS_MODULE_OPTIMISATION */

#ifdef __cplusplus
extern "C" {
#endif

/* 下面这些宏定义是用来设置校验值的，默认configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES为0表示不使用校验值*/
#if( configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES == 0 )
	/* Define the macros to do nothing. */
	#define listFIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE
	#define listSECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE
	#define listFIRST_LIST_INTEGRITY_CHECK_VALUE
	#define listSECOND_LIST_INTEGRITY_CHECK_VALUE
	#define listSET_FIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem )
	#define listSET_SECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem )
	#define listSET_LIST_INTEGRITY_CHECK_1_VALUE( pxList )
	#define listSET_LIST_INTEGRITY_CHECK_2_VALUE( pxList )
	#define listTEST_LIST_ITEM_INTEGRITY( pxItem )
	#define listTEST_LIST_INTEGRITY( pxList )
#else
	/* Define macros that add new members into the list structures. */
	#define listFIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE				TickType_t xListItemIntegrityValue1;
	#define listSECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE				TickType_t xListItemIntegrityValue2;
	#define listFIRST_LIST_INTEGRITY_CHECK_VALUE					TickType_t xListIntegrityValue1;
	#define listSECOND_LIST_INTEGRITY_CHECK_VALUE					TickType_t xListIntegrityValue2;

	/* Define macros that set the new structure members to known values. */
	#define listSET_FIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem )		( pxItem )->xListItemIntegrityValue1 = pdINTEGRITY_CHECK_VALUE
	#define listSET_SECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem )	( pxItem )->xListItemIntegrityValue2 = pdINTEGRITY_CHECK_VALUE
	#define listSET_LIST_INTEGRITY_CHECK_1_VALUE( pxList )		( pxList )->xListIntegrityValue1 = pdINTEGRITY_CHECK_VALUE
	#define listSET_LIST_INTEGRITY_CHECK_2_VALUE( pxList )		( pxList )->xListIntegrityValue2 = pdINTEGRITY_CHECK_VALUE

	/* Define macros that will assert if one of the structure members does not
	contain its expected value. */
	#define listTEST_LIST_ITEM_INTEGRITY( pxItem )		configASSERT( ( ( pxItem )->xListItemIntegrityValue1 == pdINTEGRITY_CHECK_VALUE ) && ( ( pxItem )->xListItemIntegrityValue2 == pdINTEGRITY_CHECK_VALUE ) )
	#define listTEST_LIST_INTEGRITY( pxList )			configASSERT( ( ( pxList )->xListIntegrityValue1 == pdINTEGRITY_CHECK_VALUE ) && ( ( pxList )->xListIntegrityValue2 == pdINTEGRITY_CHECK_VALUE ) )
#endif /* configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES */


/*
 * Definition of the only type of object that a list can contain.
 */
struct xLIST_ITEM
{
	listFIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE			/*< Set to a known value if configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES is set to 1. */
	configLIST_VOLATILE TickType_t xItemValue;			/*< 链表项中存储的值，一般按照降序进行排列*/
	struct xLIST_ITEM * configLIST_VOLATILE pxNext;		/*< Pointer to the next ListItem_t in the list. */
	struct xLIST_ITEM * configLIST_VOLATILE pxPrevious;	/*< Pointer to the previous ListItem_t in the list. */
	void * pvOwner;										/*< 指向拥有该链表项的内核对象（通常是 TCB）的指针。因此包含链表项的对象与链表项本身之间存在双向链接 */
	void * configLIST_VOLATILE pvContainer;				/*< 指向该链表项所属链表 */
	listSECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE			/*< Set to a known value if configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES is set to 1. */
};
typedef struct xLIST_ITEM ListItem_t;					/* For some reason lint wants this as two separate definitions. */

struct xMINI_LIST_ITEM									/*< xMINI_LIST_ITEM应该是专门用于尾链表项的，尾链表项应该是不挂载TCB的，由于这里是双向链表，其实也可以将尾链表项理解为头链表项 */
{
	listFIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE			/*< Set to a known value if configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES is set to 1. */
	configLIST_VOLATILE TickType_t xItemValue;
	struct xLIST_ITEM * configLIST_VOLATILE pxNext;
	struct xLIST_ITEM * configLIST_VOLATILE pxPrevious;
};
typedef struct xMINI_LIST_ITEM MiniListItem_t;

/*
 * Definition of the type of queue used by the scheduler.
 */
typedef struct xLIST
{
	listFIRST_LIST_INTEGRITY_CHECK_VALUE				/*< Set to a known value if configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES is set to 1. */
	configLIST_VOLATILE UBaseType_t uxNumberOfItems;	/*< 记录链表中链表项的数量 */
	ListItem_t * configLIST_VOLATILE pxIndex;			/*< 指向链表项的指针，用于遍历列表 */
	MiniListItem_t xListEnd;							/*< 指向链表尾对应的链表项，包含最大可能项目值的列表项意味着它始终位于列表的末尾 */
	listSECOND_LIST_INTEGRITY_CHECK_VALUE				/*< Set to a known value if configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES is set to 1. */
} List_t;

/*
 * 设置链表项的拥有者（TCB）
 */
#define listSET_LIST_ITEM_OWNER( pxListItem, pxOwner )		( ( pxListItem )->pvOwner = ( void * ) ( pxOwner ) )

/*
 * 获取链表项的拥有者（TCB）
 */
#define listGET_LIST_ITEM_OWNER( pxListItem )	( ( pxListItem )->pvOwner )

/*
 * 设置链表项的值
 */
#define listSET_LIST_ITEM_VALUE( pxListItem, xValue )	( ( pxListItem )->xItemValue = ( xValue ) )

/*
 * 获取链表项的值
 */
#define listGET_LIST_ITEM_VALUE( pxListItem )	( ( pxListItem )->xItemValue )

/*
 * 获取第一个链表项中的值
 */
#define listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxList )	( ( ( pxList )->xListEnd ).pxNext->xItemValue )

/*
 * 获取第一个链表项的指针
 */
#define listGET_HEAD_ENTRY( pxList )	( ( ( pxList )->xListEnd ).pxNext )

/*
 * 获取当前链表项的下一个链表项指针
 */
#define listGET_NEXT( pxListItem )	( ( pxListItem )->pxNext )

/*
 * 获取链表尾项的指针
 */
#define listGET_END_MARKER( pxList )	( ( ListItem_t const * ) ( &( ( pxList )->xListEnd ) ) )

/*
 * 判断当前链表是否为空，若是则返回1，否则返回0
 */
#define listLIST_IS_EMPTY( pxList )	( ( BaseType_t ) ( ( pxList )->uxNumberOfItems == ( UBaseType_t ) 0 ) )

/*
 * 获取当前链表的链表项总数量
 */
#define listCURRENT_LIST_LENGTH( pxList )	( ( pxList )->uxNumberOfItems )

/*
 * 获取当前pxIndex所指链表项的下一个链表项的Owner，如果下个链表项是尾链表项，则需要将指针继续后移一位，跳过尾链表项
 */
#define listGET_OWNER_OF_NEXT_ENTRY( pxTCB, pxList )										\
{																							\
List_t * const pxConstList = ( pxList );													\
	( pxConstList )->pxIndex = ( pxConstList )->pxIndex->pxNext;							\
	if( ( void * ) ( pxConstList )->pxIndex == ( void * ) &( ( pxConstList )->xListEnd ) )	\
	{																						\
		( pxConstList )->pxIndex = ( pxConstList )->pxIndex->pxNext;						\
	}																						\
	( pxTCB ) = ( pxConstList )->pxIndex->pvOwner;											\
}


/*
 * 获得链表中第一个链表项对应的Owner
 */
#define listGET_OWNER_OF_HEAD_ENTRY( pxList )  ( (&( ( pxList )->xListEnd ))->pxNext->pvOwner )

/*
 * 判断传入的pxListItem是否属于传入的pxList
 */
#define listIS_CONTAINED_WITHIN( pxList, pxListItem ) ( ( BaseType_t ) ( ( pxListItem )->pvContainer == ( void * ) ( pxList ) ) )

/*
 * 获取链表项pxListItem所属的链表
 */
#define listLIST_ITEM_CONTAINER( pxListItem ) ( ( pxListItem )->pvContainer )

/*
 * 判断当前pxList是否已经初始化
 */
#define listLIST_IS_INITIALISED( pxList ) ( ( pxList )->xListEnd.xItemValue == portMAX_DELAY )

/*
 * 链表初始化函数
 */
void vListInitialise( List_t * const pxList ) PRIVILEGED_FUNCTION;

/*
 * 链表项初始化函数
 */
void vListInitialiseItem( ListItem_t * const pxItem ) PRIVILEGED_FUNCTION;

/*
 * 链表项插入函数（按照链表项的值升序排序插入）
 */
void vListInsert( List_t * const pxList, ListItem_t * const pxNewListItem ) PRIVILEGED_FUNCTION;

/*
 * 链表项插入函数（不排序）
 */
void vListInsertEnd( List_t * const pxList, ListItem_t * const pxNewListItem ) PRIVILEGED_FUNCTION;

/*
 * 链表项删除函数
 */
UBaseType_t uxListRemove( ListItem_t * const pxItemToRemove ) PRIVILEGED_FUNCTION;

#ifdef __cplusplus
}
#endif

#endif

