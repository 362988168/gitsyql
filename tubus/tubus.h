#ifndef __TUBUS_H__
#define __TUBUS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <libubus.h>
#include <json-c/json.h>
#include <libubox/uloop.h>
#include <libubox/blobmsg_json.h>
#include <libubox/ustream.h>
#include <libubox/utils.h>
#include "typedef.h"

#define NAME_LONG_MAX 64

/*******************************************
1、对象、方法、事件名最大长度不超过64字节
2、订阅通知发送notify的最快周期50ms，且通知消息体最大长度512字节
3、针对开机订阅对象等待时间60S，如果超时服务线程会重启
*******************************************/
typedef enum
{
    ENUM_TUBUS_STATUS_OK = 0,
    ENUM_TUBUS_NO_MEMORY,           // 申请内存失败
    ENUM_TUBUS_INVALID_PARA,        // 参数无效
    ENUM_TUBUS_ARRAY_OVER,          // 数组越界
    ENUM_TUBUS_OBJECT_NOT_FOUND,    // 没有对象
    ENUM_TUBUS_CONNECTION_FAILED,   // 连接失败
    ENUM_TUBUS_CREAT_OBJECT_FAILED, // 创建对象失败
    ENUM_TUBUS_EVENT_REG_FAILED,    // 事件注册失败
    ENUM_TUBUS_EVENT_SEND_FAILED,   // 事件发送失败
    ENUM_TUBUS_INVOKE_SEND_FAILED,  // 远程调用发送失败
    ENUM_TUBUS_SUBSCRIBE_FAILED,    // 订阅失败

} ENUM_TUBUS_ERROR_TYPE; // 故障类型

// 远程调用call数据返回 回调函数
typedef void (*TUbusResponseCallback)(struct ubus_request *req, int type, struct blob_attr *msg);

// 注册方法回调函数
typedef int (*UserMethodCallback)(const char *object, const char *method, struct blob_attr *recv_msg, struct blob_buf *send_buf);

// 注册广播事件回调函数
typedef void (*UserEventCallback)(const char *event, struct blob_attr *msg);

// 订阅对象回调函数
typedef void (*UserSubscriberCallback)(const char *subs_obj, const char *event, struct blob_attr *msg);

// 用户注册event接口
typedef struct
{
    char event_name[NAME_LONG_MAX];
    UserEventCallback cb;

} STRU_TUBUS_USER_EVENT;

// 用户方法注册接口
typedef struct
{
    char method_name[NAME_LONG_MAX];
    UserMethodCallback cb;

} STRU_TUBUS_USER_METHODS;

// 用户对象创建接口
typedef struct
{
    char object_name[NAME_LONG_MAX];

    STRU_TUBUS_USER_METHODS *user_method;

    int method_num;

} STRU_TUBUS_USER_OBJECT;

// 用户订阅接口
typedef struct
{
    char subs_obj_name[NAME_LONG_MAX];
    UserSubscriberCallback cb;

} STRU_TUBUS_USER_SUBSCRIBER;

/**************************************************************************
 * 名    称: TUBUS_ServerCreat
 * 说    明: 服务端对象、方法、订阅、事件注册接口
 * 参    数:
 *           pObjectGroup:对象注册列表
 *           objectNum:对象数目
 *           pSubscriberGroup:订阅列表
 *           subscriberNum:订阅数目
 *           pEventGroup:注册事件列表
 *           eventNum:事件个数
 *
 * 返    回: 成功返回OK，失败返回ERR
 **************************************************************************/
ENUM_TUBUS_ERROR_TYPE TUBUS_ServerCreat(STRU_TUBUS_USER_OBJECT *pObjectGroup, int objectNum,
                                        STRU_TUBUS_USER_SUBSCRIBER *pSubscriberGroup, int subscriberNum,
                                        STRU_TUBUS_USER_EVENT *pEventGroup, int eventNum);

/**************************************************************************
 * 名    称: TUBUS_ServerAdd
 * 说    明: 添加服务端对象、方法、订阅、事件注册接口
 * 参    数:
 *           pObjectGroup:对象注册列表
 *           objectNum:对象数目
 *           pSubscriberGroup:订阅列表
 *           subscriberNum:订阅数目
 *           pEventGroup:注册事件列表
 *           eventNum:事件个数
 *
 * 返    回: 成功返回OK，失败返回ERR
 **************************************************************************/
ENUM_TUBUS_ERROR_TYPE TUBUS_ServerAdd(STRU_TUBUS_USER_OBJECT *pObjectGroup, int objectNum,
                                      STRU_TUBUS_USER_SUBSCRIBER *pSubscriberGroup, int subscriberNum,
                                      STRU_TUBUS_USER_EVENT *pEventGroup, int eventNum);

/**************************************************************************
 * 名    称: TUBUS_RemoteInvoke
 * 说    明: 远程调用对象方法接口
 * 参    数:
 *           pObjectName:调用对象名称
 *           pMethodName:调用方法名称
 *           pMsg:调用时发送数据
 *           pPriv:用户私有数据，可自定义后在回调中以此区分不同方法的回复
 *           cb:调用后应答回复回调接口
 *           timeout:超时时间（ms） 0:无限等待 -1：立即返回
 *
 * 返    回: 成功返回OK，失败返回ERR
 **************************************************************************/
ENUM_TUBUS_ERROR_TYPE TUBUS_RemoteInvoke(char *pObjectName, char *pMethodName, struct blob_attr *pMsg, void *pPriv, TUbusResponseCallback cb, int timeout);
/**************************************************************************
 * 名    称: TUBUS_EventSend
 * 说    明: 广播事件发送接口
 * 参    数:
 *           event:发送事件名称
 *           pMsg:发送数据
 *
 * 返    回: 成功返回OK，失败返回ERR
 **************************************************************************/
ENUM_TUBUS_ERROR_TYPE TUBUS_EventSend(const char *event, struct blob_attr *pMsg);

/**************************************************************************
 * 名    称: TUBUS_NotifySend
 * 说    明: 订阅通知发送接口
 * 参    数:
 *           pObjectName:发送对象名称
 *           method:发送事件名称
 *           pMsg:发送数据
 *
 * 返    回: 成功返回OK，失败返回ERR
 **************************************************************************/
ENUM_TUBUS_ERROR_TYPE TUBUS_NotifySend(char *pObjectName, const char *method, struct blob_attr *pMsg);

#ifdef __cplusplus
}
#endif

#endif /* __TUBUS_H__ */
