#include <libubox/uloop.h>
#include <libubus.h>

static struct ubus_context *ctx;

enum {
    GW_STATUS,
    __GW_STATUS_MAX
};

static const struct blobmsg_policy status_policy[__GW_STATUS_MAX] = {
    [GW_STATUS] = { .name = "status", .type = BLOBMSG_TYPE_STRING },
};

static int gw_get_status(struct ubus_context *ctx, struct ubus_object *obj,
                        struct ubus_request_data *req, const char *method,
                        struct blob_attr *msg)
{
    struct blob_buf b = {};
    blob_buf_init(&b, 0);
    
    // 获取实际状态
    blobmsg_add_string(&b, "status", get_gw_status());
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    return UBUS_STATUS_OK;
}

// 新增命令处理方法
static int gw_handle_command(struct ubus_context *ctx, struct ubus_object *obj,
                            struct ubus_request_data *req, const char *method,
                            struct blob_attr *msg)
{
    struct blob_buf b = {};
    blob_buf_init(&b, 0);
    
    // 解析命令参数
    struct blob_attr *tb[__COMMAND_ARGS_MAX];
    static const struct blobmsg_policy command_policy[] = {
        { .name = "action", .type = BLOBMSG_TYPE_STRING },
        { .name = "param",  .type = BLOBMSG_TYPE_STRING }
    };
    
    blobmsg_parse(command_policy, ARRAY_SIZE(command_policy), tb, blob_data(msg), blob_len(msg));
    
    const char *action = blobmsg_get_string(tb[0]);
    const char *param = tb[1] ? blobmsg_get_string(tb[1]) : NULL;
    
    // 执行命令
    int ret = 0;
    if (strcmp(action, "reload") == 0) {
        ret = reload_configuration();
        blobmsg_add_string(&b, "result", ret == 0 ? "success" : "failed");
    } else if (strcmp(action, "switch") == 0) {
        ret = switch_interface(param);
        blobmsg_add_string(&b, "new_interface", param);
    } else {
        blobmsg_add_string(&b, "error", "invalid command");
        ret = UBUS_STATUS_INVALID_ARGUMENT;
    }
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    return ret;
}

// 更新方法列表
static struct ubus_method gw_methods[] = {
    UBUS_METHOD("get_status", gw_get_status, status_policy),
    UBUS_METHOD("command",    gw_handle_command, command_policy)
};

static struct ubus_object_type gw_obj_type = 
    UBUS_OBJECT_TYPE("virtualgw", gw_methods);

static struct ubus_object gw_obj = {
    .name = "virtualgw",
    .type = &gw_obj_type,
    .methods = gw_methods,
    .n_methods = ARRAY_SIZE(gw_methods),
};

void ubus_init(void)
{
    ctx = ubus_connect(NULL);
    ubus_add_object(ctx, &gw_obj);
}
