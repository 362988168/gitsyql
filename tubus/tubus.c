
#include "tubus.h"
#include "log.h"
#include "mymq.h"
#include "mypth.h"

// v1.4
#define TUBUS_PTHREAD_STACK_SIZE (32 * 1024)

#define TUBUS_CYCLE_TIMEOUT 50     // 周期定时器超时时间-发送notify 50ms
#define TUBUS_SUBSCRIBE_TIMEOUT 10 // 开机订阅超时等待时间10s
#define METHODS_NUM_MAX 32
#define NOTIFY_DATA_MAX 512
#define TUBUS_MIN(x, y) x > y ? y : x

ENUM_TUBUS_ERROR_TYPE TUbusObjectCreat(void);
ENUM_TUBUS_ERROR_TYPE TUbusSubscriberRegister(void);
ENUM_TUBUS_ERROR_TYPE TUbusEventRegister(void);

typedef struct
{
    char sub_obj_name[NAME_LONG_MAX];
    char sub_meth_name[NAME_LONG_MAX];
    char data[NOTIFY_DATA_MAX];

} STRU_NOTIFY_DATA;

typedef struct
{
    struct ubus_object dubus_object;
    struct ubus_method *dubus_methods;
    struct ubus_object_type dubus_type;
    struct blobmsg_policy dubus_policy;

    STRU_TUBUS_USER_OBJECT duser_object;
    bool object_state; // 对象创建状态
    struct list_head object_list;

} STRU_TUBUS_OBJECT;

typedef struct
{
    unsigned int subs_obj_id;
    struct ubus_subscriber dubus_subscriber;

    STRU_TUBUS_USER_SUBSCRIBER duser_subscriber;
    bool subs_state; // 订阅状态
    struct list_head subscriber_list;

} STRU_TUBUS_SUBSCRIBER;

typedef struct
{
    struct ubus_event_handler event_handler;

    STRU_TUBUS_USER_EVENT duser_event;
    bool event_state; // 注册状态
    struct list_head event_list;

} STRU_TUBUS_EVENT;

typedef struct
{
    char *cli_path;
    struct ubus_context *ubus_ctx;
    struct uloop_timeout cycle_retry;

    STRU_TUBUS_OBJECT *my_object;
    int object_num;

    STRU_TUBUS_SUBSCRIBER *my_subscriber;
    int subscriber_num;

    STRU_TUBUS_EVENT *my_event;
    int event_num;

    bool conn_state;        // 与ubusd连接状态
    int msgQRecv;           // 订阅事件发送队列
    pthread_t idThreadRecv; // 接收线程句柄

} STRU_TUBUS_CONTEXT;

STRU_TUBUS_CONTEXT g_tubus_ctx;

// 对象链表的初始化
STRU_TUBUS_OBJECT *ObjectListInit(void)
{
    STRU_TUBUS_OBJECT *head = malloc(sizeof(STRU_TUBUS_OBJECT));
    INIT_LIST_HEAD(&(head->object_list));
    return head;
}

// 创建新的节点
STRU_TUBUS_OBJECT *ObjectListAddNode(STRU_TUBUS_OBJECT *head)
{
    STRU_TUBUS_OBJECT *new = malloc(sizeof(STRU_TUBUS_OBJECT));

    INIT_LIST_HEAD(&(new->object_list));
    list_add(&(new->object_list), &(head->object_list));
    return new;
}

// 查找链表数据
STRU_TUBUS_OBJECT *ObjectListEach(STRU_TUBUS_OBJECT *head, const char *name)
{
    STRU_TUBUS_OBJECT *pos = NULL;
    list_for_each_entry(pos, &(head->object_list), object_list)
    {
        if (!strcmp(pos->duser_object.object_name, name))
        {
            return pos;
        }
    }
    return NULL;
}

int ObjectListDel(STRU_TUBUS_OBJECT *head)
{
    STRU_TUBUS_OBJECT *pos = NULL;
    list_for_each_entry(pos, &(head->object_list), object_list)
    {
        list_del(&(pos->object_list));
        free(pos);
    }
    return 0;
}

// 订阅链表的初始化
STRU_TUBUS_SUBSCRIBER *SubscribeListInit(void)
{
    STRU_TUBUS_SUBSCRIBER *head = malloc(sizeof(STRU_TUBUS_SUBSCRIBER));
    INIT_LIST_HEAD(&(head->subscriber_list));
    return head;
}

// 创建新的节点
STRU_TUBUS_SUBSCRIBER *SubscribeListAddNode(STRU_TUBUS_SUBSCRIBER *head)
{
    STRU_TUBUS_SUBSCRIBER *new = malloc(sizeof(STRU_TUBUS_SUBSCRIBER));

    INIT_LIST_HEAD(&(new->subscriber_list));
    list_add(&(new->subscriber_list), &(head->subscriber_list));
    return new;
}

// 查找数据
STRU_TUBUS_SUBSCRIBER *SubscribeListEach(STRU_TUBUS_SUBSCRIBER *head, char *name)
{
    STRU_TUBUS_SUBSCRIBER *pos = NULL;
    list_for_each_entry(pos, &(head->subscriber_list), subscriber_list)
    {
        if (!strcmp(pos->duser_subscriber.subs_obj_name, name))
        {
            return pos;
        }
    }
    return NULL;
}

STRU_TUBUS_SUBSCRIBER *SubscribeListEachId(STRU_TUBUS_SUBSCRIBER *head, unsigned int id)
{
    STRU_TUBUS_SUBSCRIBER *pos = NULL;
    list_for_each_entry(pos, &(head->subscriber_list), subscriber_list)
    {
        if (pos->subs_obj_id == id)
        {
            return pos;
        }
    }
    return NULL;
}

int SubscribeListDel(STRU_TUBUS_SUBSCRIBER *head)
{
    STRU_TUBUS_SUBSCRIBER *pos = NULL;
    list_for_each_entry(pos, &(head->subscriber_list), subscriber_list)
    {
        list_del(&(pos->subscriber_list));
        free(pos);
    }
    return 0;
}

// 事件链表的初始化
STRU_TUBUS_EVENT *EventListInit(void)
{
    STRU_TUBUS_EVENT *head = malloc(sizeof(STRU_TUBUS_EVENT));
    INIT_LIST_HEAD(&(head->event_list));
    return head;
}

// 创建新的节点
STRU_TUBUS_EVENT *EventListAddNode(STRU_TUBUS_EVENT *head)
{
    STRU_TUBUS_EVENT *new = malloc(sizeof(STRU_TUBUS_EVENT));

    INIT_LIST_HEAD(&(new->event_list));
    list_add(&(new->event_list), &(head->event_list));
    return new;
}

// 查找链表数据
STRU_TUBUS_EVENT *EventListEach(STRU_TUBUS_EVENT *head, const char *name)
{
    STRU_TUBUS_EVENT *pos = NULL;
    list_for_each_entry(pos, &(head->event_list), event_list)
    {
        if (!strcmp(pos->duser_event.event_name, name))
        {
            return pos;
        }
    }
    return NULL;
}

int EventListDel(STRU_TUBUS_EVENT *head)
{
    STRU_TUBUS_EVENT *pos = NULL;
    list_for_each_entry(pos, &(head->event_list), event_list)
    {
        list_del(&(pos->event_list));
        free(pos);
    }
    return 0;
}

void ListStateReset(void)
{
    STRU_TUBUS_CONTEXT *p = &g_tubus_ctx;
    STRU_TUBUS_OBJECT *objPos = NULL;
    STRU_TUBUS_SUBSCRIBER *subPos = NULL;
    STRU_TUBUS_EVENT *eventPos = NULL;

    list_for_each_entry(objPos, &(p->my_object->object_list), object_list)
    {
        objPos->object_state = false;
    }

    list_for_each_entry(subPos, &(p->my_subscriber->subscriber_list), subscriber_list)
    {
        subPos->subs_state = false;
    }

    list_for_each_entry(eventPos, &(p->my_event->event_list), event_list)
    {
        eventPos->event_state = false;
    }
}
/***************************ubus 初始化***************************/

static void ubus_cycle_timer(struct uloop_timeout *timeout)
{
    static int check_count = 0;
    static int check_state = 0;
    STRU_NOTIFY_DATA notify_data;
    STRU_TUBUS_OBJECT *object_node = NULL;
    struct blob_attr *msg = (struct blob_attr *)notify_data.data;
    // 周期性定时器
    if (g_tubus_ctx.conn_state)
    {
        if (++check_count >= (2000 / TUBUS_CYCLE_TIMEOUT))
        {
            check_count = 0;
            switch (check_state)
            {
            case 0:
                TUbusObjectCreat();
                check_state = 1;
                break;
            case 1:
                TUbusSubscriberRegister();
                check_state = 2;
                break;
            case 2:
                TUbusEventRegister();
                check_state = 0;
                break;
            default:
                break;
            }
        }

        while (ERR != MyMq_Recv(g_tubus_ctx.msgQRecv, (char *)&notify_data, sizeof(STRU_NOTIFY_DATA), (unsigned *)NULL, 0))
        {
            if (notify_data.sub_obj_name != NULL)
            {
                object_node = ObjectListEach(g_tubus_ctx.my_object, notify_data.sub_obj_name);
                if (object_node != NULL)
                {
                    if (object_node->dubus_object.has_subscribers == false) // 对象没有被订阅不发送
                    {
                        // LOG_DebugRecord(NONE, "has_subscribers Null\n");
                    }
                    else
                    {
                        // 广播notification消息
                        ubus_notify(g_tubus_ctx.ubus_ctx, &object_node->dubus_object, notify_data.sub_meth_name, msg, -1);
                    }
                }
            }
        }
    }

    uloop_timeout_set(timeout, TUBUS_CYCLE_TIMEOUT);
}

static void ubus_connection_lost(struct ubus_context *ctx)
{
    g_tubus_ctx.conn_state = false;
    uloop_end();
}

ENUM_TUBUS_ERROR_TYPE TubusServerInit(void)
{
    char *path = NULL;

    uloop_init();
    signal(SIGPIPE, SIG_IGN);

    g_tubus_ctx.cli_path = path;
    /**
     * 初始化client端context结构，并连接ubusd
     */
    g_tubus_ctx.ubus_ctx = ubus_connect(path);
    if (!g_tubus_ctx.ubus_ctx)
    {
        LOG_DebugRecord(NONE, "ubus connect failed\n");
        return ENUM_TUBUS_CONNECTION_FAILED;
    }

    LOG_DebugRecord(NONE, "connected as %d--\n", g_tubus_ctx.ubus_ctx->local_id);
    g_tubus_ctx.ubus_ctx->connection_lost = ubus_connection_lost;

    ubus_add_uloop(g_tubus_ctx.ubus_ctx);

    g_tubus_ctx.cycle_retry.cb = ubus_cycle_timer;

    return ENUM_TUBUS_STATUS_OK;
}

static int TUbusMethodCallback(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req,
                               const char *method, struct blob_attr *msg)
{
    int i = 0;
    static struct blob_buf b;
    STRU_TUBUS_OBJECT *object_node = NULL;

    if ((g_tubus_ctx.ubus_ctx == ctx) && (obj->name != NULL))
    {
        object_node = ObjectListEach(g_tubus_ctx.my_object, obj->name);
        if (object_node != NULL)
        {
            for (i = 0; i < object_node->duser_object.method_num; i++)
            {
                if ((method != NULL) && !strcmp(object_node->duser_object.user_method[i].method_name, method))
                {
                    if (object_node->duser_object.user_method[i].cb != NULL)
                    {
                        blob_buf_init(&b, 0);
                        object_node->duser_object.user_method[i].cb(obj->name, method, msg, &b);
                        ubus_send_reply(ctx, req, b.head);

                        blob_buf_free(&b);
                    }
                    break;
                }
            }
        }
    }

    return OK;
}

ENUM_TUBUS_ERROR_TYPE TUbusObjectCreat(void)
{
    int i = 0;
    int ret = ENUM_TUBUS_STATUS_OK;

    STRU_TUBUS_CONTEXT *p = &g_tubus_ctx;
    STRU_TUBUS_OBJECT *object_node = NULL;

    if ((p == NULL) || (p->ubus_ctx == NULL))
    {
        LOG_DebugRecord(NONE, "ubus_ctx pointer Null\n");
        ret = ENUM_TUBUS_INVALID_PARA;
    }

    list_for_each_entry(object_node, &(p->my_object->object_list), object_list)
    {
        if ((object_node != NULL) && (object_node->object_state == false))
        {
            for (i = 0; i < object_node->duser_object.method_num; i++)
            {
                if (object_node->duser_object.user_method[i].method_name != NULL)
                {
                    if (strlen(object_node->duser_object.user_method[i].method_name) < NAME_LONG_MAX)
                    {
                        object_node->dubus_methods[i].name = object_node->duser_object.user_method[i].method_name;
                        object_node->dubus_methods[i].handler = TUbusMethodCallback;
                        object_node->dubus_methods[i].policy = &object_node->dubus_policy;
                        object_node->dubus_methods[i].n_policy = ARRAY_SIZE(&object_node->dubus_policy);
                    }
                }
            }

            // 定义 object 类型
            memset((char *)&object_node->dubus_type, 0x00, sizeof(struct ubus_object_type));
            object_node->dubus_type.name = object_node->duser_object.object_name;
            object_node->dubus_type.methods = object_node->dubus_methods;
            object_node->dubus_type.n_methods = object_node->duser_object.method_num;
            // 定义 object
            memset((char *)&object_node->dubus_object, 0x00, sizeof(struct ubus_object));
            object_node->dubus_object.name = object_node->duser_object.object_name;
            object_node->dubus_object.type = &object_node->dubus_type;
            object_node->dubus_object.methods = object_node->dubus_methods;
            object_node->dubus_object.n_methods = object_node->duser_object.method_num;

            if (ubus_add_object(p->ubus_ctx, &object_node->dubus_object) != 0)
            {
                object_node->object_state = false;
                ret = ENUM_TUBUS_CREAT_OBJECT_FAILED;
                LOG_DebugRecord(NONE, "failed to add object to ubus\n");
            }
            else
            {
                object_node->object_state = true;
            }
        }
    }

    return ret;
}

static int TUbusNotifyCallback(struct ubus_context *ctx, struct ubus_object *obj,
                               struct ubus_request_data *req,
                               const char *method, struct blob_attr *msg)
{
    struct ubus_subscriber *s = NULL;
    STRU_TUBUS_SUBSCRIBER *subs_node = NULL;

    if (g_tubus_ctx.ubus_ctx == ctx)
    {
        s = container_of(obj, struct ubus_subscriber, obj);
        subs_node = SubscribeListEachId(g_tubus_ctx.my_subscriber, s->subscribe_obj_id);
        if ((subs_node != NULL) && (subs_node->duser_subscriber.cb != NULL))
        {
            subs_node->duser_subscriber.cb(subs_node->duser_subscriber.subs_obj_name, method, msg);
        }
    }

    return 0;
}

static void TUbusSubscriberRemoveCallback(struct ubus_context *ctx, struct ubus_subscriber *obj, int id)
{
    STRU_TUBUS_SUBSCRIBER *subs_node = NULL;

    if (g_tubus_ctx.ubus_ctx == ctx)
    {
        subs_node = SubscribeListEachId(g_tubus_ctx.my_subscriber, id);
        if (subs_node != NULL)
        {
            subs_node->subs_state = false;
            ubus_unregister_subscriber(g_tubus_ctx.ubus_ctx, &subs_node->dubus_subscriber);
        }
    }
}

ENUM_TUBUS_ERROR_TYPE TUbusSubscriberRegister(void)
{
    int ret = ENUM_TUBUS_STATUS_OK;
    unsigned int obj_id = 0;
    STRU_TUBUS_CONTEXT *p = &g_tubus_ctx;
    STRU_TUBUS_SUBSCRIBER *subs_node = NULL;

    if ((p == NULL) || (p->ubus_ctx == NULL))
    {
        LOG_DebugRecord(NONE, "ubus_ctx pointer Null\n");
        ret = ENUM_TUBUS_INVALID_PARA;
    }

    list_for_each_entry(subs_node, &(p->my_subscriber->subscriber_list), subscriber_list)
    {
        if ((subs_node != NULL) && (subs_node->subs_state == false))
        {
            memset(&subs_node->dubus_subscriber, 0x00, sizeof(struct ubus_subscriber));
            subs_node->dubus_subscriber.cb = TUbusNotifyCallback;
            subs_node->dubus_subscriber.remove_cb = (ubus_remove_handler_t)TUbusSubscriberRemoveCallback;
            /* 得到要订阅的object的id */
            if (ubus_lookup_id(p->ubus_ctx, subs_node->duser_subscriber.subs_obj_name, &obj_id) != 0)
            {
                LOG_DebugRecord(NONE, "Failed to lookup object: %s\n", ubus_strerror(ret));
                subs_node->subs_state = false;
                ret = ENUM_TUBUS_SUBSCRIBE_FAILED;
                continue;
            }

            // 注册订阅者
            if (ubus_register_subscriber(p->ubus_ctx, &subs_node->dubus_subscriber) != 0)
            {
                LOG_DebugRecord(NONE, "Failed to register subscriber handler: %s\n", ubus_strerror(ret));
                subs_node->subs_state = false;
                ret = ENUM_TUBUS_SUBSCRIBE_FAILED;
                continue;
            }

            /* 订阅object */

            if (ubus_subscribe(p->ubus_ctx, &subs_node->dubus_subscriber, obj_id) != 0)
            {
                LOG_DebugRecord(NONE, "Failed to subscribe: %s\n", ubus_strerror(ret));
                subs_node->subs_state = false;
                ret = ENUM_TUBUS_SUBSCRIBE_FAILED;
                continue;
            }
            subs_node->subs_obj_id = obj_id;
            subs_node->subs_state = true;
        }
    }

    return ret;
}

static void TUbusEventCallback(struct ubus_context *ctx, struct ubus_event_handler *ev, const char *event, struct blob_attr *msg)
{
    STRU_TUBUS_EVENT *event_node = NULL;

    if (g_tubus_ctx.ubus_ctx == ctx)
    {
        event_node = EventListEach(g_tubus_ctx.my_event, event);
        if ((event_node != NULL) && (event_node->duser_event.cb != NULL))
        {
            event_node->duser_event.cb(event, msg);
        }
    }
}

ENUM_TUBUS_ERROR_TYPE TUbusEventRegister(void)
{
    int ret = ENUM_TUBUS_STATUS_OK;

    STRU_TUBUS_CONTEXT *p = &g_tubus_ctx;
    STRU_TUBUS_EVENT *event_node = NULL;

    if ((p == NULL) || (p->ubus_ctx == NULL))
    {
        LOG_DebugRecord(NONE, "ubus_ctx pointer Null\n");
        ret = ENUM_TUBUS_INVALID_PARA;
    }

    list_for_each_entry(event_node, &(p->my_event->event_list), event_list)
    {
        if ((event_node != NULL) && (event_node->event_state == false))
        {
            if (event_node->duser_event.event_name != NULL)
            {
                memset(&event_node->event_handler, 0, sizeof(struct ubus_event_handler));
                event_node->event_handler.cb = TUbusEventCallback;

                if (ubus_register_event_handler(p->ubus_ctx, &event_node->event_handler, event_node->duser_event.event_name) != 0)
                {
                    event_node->event_state = false;
                    ret = ENUM_TUBUS_EVENT_REG_FAILED;
                    LOG_DebugRecord(NONE, "Failed to register event to ubus server %s\n", ubus_strerror(ret));
                }
                else
                {
                    event_node->event_state = true;
                }
            }
        }
    }

    return ret;
}

ENUM_TUBUS_ERROR_TYPE TUBUS_RemoteInvoke(char *pObjectName, char *pMethodName, struct blob_attr *pMsg, void *pPriv, TUbusResponseCallback cb, int timeout)
{
    unsigned int id;
    int ret = ENUM_TUBUS_STATUS_OK;
    struct ubus_context *cli_ctx = NULL;
    char *cli_path = NULL;

    if ((pObjectName == NULL) || (pMethodName == NULL))
    {
        LOG_DebugRecord(NONE, "TUBUS_RemoteInvoke Parameter invalid\n");
        return ENUM_TUBUS_INVALID_PARA;
    }

    cli_ctx = ubus_connect(cli_path);
    if (!cli_ctx)
    {
        LOG_DebugRecord(NONE, "TUBUS_RemoteInvoke connect failed\n");
        return ENUM_TUBUS_CONNECTION_FAILED;
    }

    ret = ubus_lookup_id(cli_ctx, pObjectName, &id);
    if (ret != UBUS_STATUS_OK)
    {
        ret = ENUM_TUBUS_OBJECT_NOT_FOUND;
        LOG_DebugRecord(NONE, "lookup scan_prog failed\n");
    }
    else
    {
        // 调用"scan_prog"对象的"scan"方法
        ret = ubus_invoke(cli_ctx, id, pMethodName, pMsg, cb, pPriv, timeout);
        if (ret != 0)
        {
            ret = ENUM_TUBUS_INVOKE_SEND_FAILED;
            LOG_DebugRecord(NONE, "lookup invoke send fail\n");
        }
    }

    if (cli_ctx)
    {
        ubus_free(cli_ctx);
    }
    return ret;
}

ENUM_TUBUS_ERROR_TYPE TUBUS_NotifySend(char *pObjectName, const char *method, struct blob_attr *pMsg)
{
    STRU_NOTIFY_DATA notify_data;

    if ((pObjectName == NULL) || (method == NULL))
    {
        LOG_DebugRecord(NONE, "TUBUS_NotifySend Parameter invalid\n");
        return ENUM_TUBUS_INVALID_PARA;
    }

    memset(&notify_data, 0x00, sizeof(notify_data));

    memcpy(notify_data.sub_obj_name, pObjectName, TUBUS_MIN(strlen(pObjectName), NAME_LONG_MAX));
    memcpy(notify_data.sub_meth_name, method, TUBUS_MIN(strlen(method), NAME_LONG_MAX));
    memcpy(notify_data.data, (char *)pMsg, TUBUS_MIN(blob_raw_len(pMsg), NOTIFY_DATA_MAX));

    MyMq_Send(g_tubus_ctx.msgQRecv, (char *)&notify_data, sizeof(STRU_NOTIFY_DATA), 0, NO_WAIT);

    return ENUM_TUBUS_STATUS_OK;
}

ENUM_TUBUS_ERROR_TYPE TUBUS_EventSend(const char *event, struct blob_attr *pMsg)
{
    int ret = ENUM_TUBUS_STATUS_OK;
    struct ubus_context *cli_ctx = NULL;
    char *cli_path = NULL;

    cli_ctx = ubus_connect(cli_path);
    if (!cli_ctx)
    {
        LOG_DebugRecord(NONE, "TUBUS_EventSend connect failed\n");
        return ENUM_TUBUS_CONNECTION_FAILED;
    }

    /* 广播名为event的事件 */
    ret = ubus_send_event(cli_ctx, event, pMsg);
    if (ret != 0)
    {
        ret = ENUM_TUBUS_EVENT_SEND_FAILED;
    }
    if (cli_ctx)
    {
        ubus_free(cli_ctx);
    }

    return ret;
}

static void HandleTubusRecv(void *arg)
{
    int ret = 0;
    static int subCount = 0;

    while (1)
    {
        ret = TubusServerInit();
        if (ret != ENUM_TUBUS_STATUS_OK)
        {
            LOG_DebugRecord(NONE, "ServerFunc:ubus connect failed\n");
            goto out;
        }

        if (g_tubus_ctx.object_num != 0)
        {
            ret = TUbusObjectCreat();
            if (ret != ENUM_TUBUS_STATUS_OK)
            {
                LOG_DebugRecord(NONE, "ServerFunc:ubus Failed to add object\n");
                goto out;
            }
        }

        if (g_tubus_ctx.event_num != 0)
        {
            ret = TUbusEventRegister();
            if (ret != ENUM_TUBUS_STATUS_OK)
            {
                LOG_DebugRecord(NONE, "ServerFunc:ubus Failed to register_events\n");
                goto out;
            }
        }

        if (g_tubus_ctx.subscriber_num != 0)
        {
            while (ENUM_TUBUS_STATUS_OK != TUbusSubscriberRegister())
            {
                if (subCount >= TUBUS_SUBSCRIBE_TIMEOUT)
                {
                    LOG_DebugRecord(NONE, "Failed to subscriber %s in 1 min!!\n", "test2");
                    subCount = 0;
                    // goto out;
                    break;
                }
                else
                {
                    sleep(2);
                    subCount = subCount + 2;
                    continue;
                }
            }
        }

        subCount = 0;
        g_tubus_ctx.conn_state = true;
        uloop_timeout_set(&g_tubus_ctx.cycle_retry, 2000);
        uloop_run();

    out:

        ListStateReset();
        // 退出uloop，销毁现场
        if (g_tubus_ctx.ubus_ctx)
        {
            ubus_free(g_tubus_ctx.ubus_ctx);
        }

        uloop_done();

        sleep(2);
    }
}

ENUM_TUBUS_ERROR_TYPE TUBUS_ServerAdd(STRU_TUBUS_USER_OBJECT *pObjectGroup, int objectNum,
                                      STRU_TUBUS_USER_SUBSCRIBER *pSubscriberGroup, int subscriberNum,
                                      STRU_TUBUS_USER_EVENT *pEventGroup, int eventNum)
{
    int i = 0;
    STRU_TUBUS_OBJECT *object_node = NULL;
    STRU_TUBUS_USER_METHODS *pMethods = NULL;
    struct ubus_method *m = NULL;

    STRU_TUBUS_SUBSCRIBER *subs_node = NULL;
    STRU_TUBUS_EVENT *event_node = NULL;

    if (g_tubus_ctx.my_object == NULL)
    {
        g_tubus_ctx.my_object = ObjectListInit();
        if (g_tubus_ctx.my_object == NULL)
        {
            return ENUM_TUBUS_NO_MEMORY;
        }
    }

    if (g_tubus_ctx.my_subscriber == NULL)
    {
        g_tubus_ctx.my_subscriber = SubscribeListInit();
        if (g_tubus_ctx.my_subscriber == NULL)
        {
            return ENUM_TUBUS_NO_MEMORY;
        }
    }

    if (g_tubus_ctx.my_event == NULL)
    {
        g_tubus_ctx.my_event = EventListInit();
        if (g_tubus_ctx.my_event == NULL)
        {
            return ENUM_TUBUS_NO_MEMORY;
        }
    }

    if ((pObjectGroup != NULL) && (objectNum != 0))
    {
        for (i = 0; i < objectNum; i++)
        {
            if (pObjectGroup[i].object_name != NULL)
            {
                if (strlen(pObjectGroup[i].object_name) < NAME_LONG_MAX)
                {
                    object_node = ObjectListEach(g_tubus_ctx.my_object, pObjectGroup[i].object_name);
                    if ((object_node == NULL) && (g_tubus_ctx.object_num < METHODS_NUM_MAX))
                    {
                        object_node = ObjectListAddNode(g_tubus_ctx.my_object);
                        if (object_node != NULL)
                        {
                            // 定义 object方法
                            if ((pObjectGroup[i].user_method != NULL) && (pObjectGroup[i].method_num != 0) && (pObjectGroup[i].method_num < METHODS_NUM_MAX))
                            {
                                m = (struct ubus_method *)malloc(pObjectGroup[i].method_num * sizeof(struct ubus_method));
                                if (m == NULL)
                                {
                                    LOG_DebugRecord(NONE, "tubus method malloc failed\n");
                                    return ENUM_TUBUS_NO_MEMORY;
                                }
                                memset(m, 0x00, pObjectGroup[i].method_num * sizeof(struct ubus_method));
                                object_node->dubus_methods = m;

                                pMethods = (STRU_TUBUS_USER_METHODS *)malloc(pObjectGroup[i].method_num * sizeof(STRU_TUBUS_USER_METHODS));
                                if (pMethods == NULL)
                                {
                                    LOG_DebugRecord(NONE, "user method malloc failed\n");
                                    return ENUM_TUBUS_NO_MEMORY;
                                }
                                memset(pMethods, 0x00, pObjectGroup[i].method_num * sizeof(STRU_TUBUS_USER_METHODS));
                                memcpy(pMethods, pObjectGroup[i].user_method, pObjectGroup[i].method_num * sizeof(STRU_TUBUS_USER_METHODS));
                                object_node->duser_object.user_method = pMethods;
                                object_node->duser_object.method_num = pObjectGroup[i].method_num;
                            }
                            memcpy(object_node->duser_object.object_name, pObjectGroup[i].object_name, sizeof(pObjectGroup[i].object_name));
                            g_tubus_ctx.object_num++;
                        }
                    }
                }
            }
        }
    }

    if ((pSubscriberGroup != NULL) && (subscriberNum != 0))
    {
        for (i = 0; i < subscriberNum; i++)
        {
            if (pSubscriberGroup[i].subs_obj_name != NULL)
            {
                if (strlen(pSubscriberGroup[i].subs_obj_name) < NAME_LONG_MAX)
                {
                    subs_node = SubscribeListEach(g_tubus_ctx.my_subscriber, pSubscriberGroup[i].subs_obj_name);
                    if ((subs_node == NULL) && (g_tubus_ctx.subscriber_num < METHODS_NUM_MAX))
                    {
                        subs_node = SubscribeListAddNode(g_tubus_ctx.my_subscriber);
                        if (subs_node != NULL)
                        {
                            memcpy(&subs_node->duser_subscriber, &pSubscriberGroup[i], sizeof(STRU_TUBUS_USER_SUBSCRIBER));
                            g_tubus_ctx.subscriber_num++;
                        }
                    }
                }
            }
        }
    }

    if ((pEventGroup != NULL) && (eventNum != 0))
    {
        for (i = 0; i < eventNum; i++)
        {
            if (pEventGroup[i].event_name != NULL)
            {
                if (strlen(pEventGroup[i].event_name) < NAME_LONG_MAX)
                {
                    event_node = EventListEach(g_tubus_ctx.my_event, pEventGroup[i].event_name);
                    if ((event_node == NULL) && (g_tubus_ctx.event_num < METHODS_NUM_MAX))
                    {
                        event_node = EventListAddNode(g_tubus_ctx.my_event);
                        if (event_node != NULL)
                        {
                            memcpy(&event_node->duser_event, &pEventGroup[i], sizeof(STRU_TUBUS_USER_EVENT));
                            g_tubus_ctx.event_num++;
                        }
                    }
                }
            }
        }
    }
    return ENUM_TUBUS_STATUS_OK;
}

ENUM_TUBUS_ERROR_TYPE TUBUS_ServerCreat(STRU_TUBUS_USER_OBJECT *pObjectGroup, int objectNum,
                                        STRU_TUBUS_USER_SUBSCRIBER *pSubscriberGroup, int subscriberNum,
                                        STRU_TUBUS_USER_EVENT *pEventGroup, int eventNum)
{
    int ret = ERR;
    char recvThrName[32];

    // memset(&g_tubus_ctx, 0x00, sizeof(STRU_TUBUS_CONTEXT));

    TUBUS_ServerAdd(pObjectGroup, objectNum, pSubscriberGroup, subscriberNum, pEventGroup, eventNum);

    g_tubus_ctx.msgQRecv = MyMq_Open("/recvNotify", 10, sizeof(STRU_NOTIFY_DATA));
    sprintf(recvThrName, "tubus server thread");
    ret = MyPth_threadInitEx(&g_tubus_ctx.idThreadRecv, (THREAD_FUNC)HandleTubusRecv, NULL, PTHREAD_PRIO_MAX, recvThrName, TUBUS_PTHREAD_STACK_SIZE);
    if (ret == 0)
    {
        LOG_DebugRecord(NONE, "init %s OK!\n", recvThrName);
    }
    else
    {
        LOG_DebugRecord(NONE, "error:init %s failed!\n", recvThrName);
    }
    return ENUM_TUBUS_STATUS_OK;
}
