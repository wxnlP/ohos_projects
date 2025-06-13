#include "ohos_init.h"
#include <stdio.h>
#include "cJSON.h"
#include "mqtt_lock.h"

static osEventFlagsId_t g_smartLockEvent;

/**
 * @brief 封装cmsis_os2的 {@link osMutexNew} 函数
 *
 * @param name 互斥锁名称
 * @return 返回互斥锁ID
 */
osMutexId_t MutexNew(char *name)
{
    osMutexAttr_t attr_mutex = {
        .name = name,
        .attr_bits = 0,
        .cb_mem = NULL,
        .cb_size = 0};
    osMutexId_t mutex_id = osMutexNew(&attr_mutex);
    if (mutex_id == NULL)
    {
        printf("[Mutex Create] osMutexNew(%s) failed.\r\n", name);
    }
    else
    {
        printf("[Mutex Create] osMutexNew(%s) success, thread id: %d.\r\n", name, mutex_id);
    }
    return mutex_id;
}

/**
 * @brief 封装cmsis_os2的 {@link osThreadNew} 函数(无参形式)
 *
 * @param name_ 线程名称
 * @param func 线程函数
 * @param stackSize 堆大小
 * @return 返回线程ID
 */
osThreadId_t ThreadNew(char *name_, osThreadFunc_t func, uint32_t stackSize)
{
    osThreadAttr_t attr = {
        .name = name_,
        .stack_size = stackSize,
        .priority = osPriorityNormal};
    osThreadId_t thread_id = osThreadNew(func, NULL, &attr);
    if (thread_id == NULL)
    {
        printf("[Thread Create] osThreadNew(%s) failed.\r\n", name_);
    }
    else
    {
        printf("[Thread Create] osThreadNew(%s) success, thread id: %d.\r\n", name_, thread_id);
    }
    return thread_id;
}

/**
 * @brief 请求设备影子的json请求格式数据
 *
 * @param device_id 设备ID
 * @param service_id 服务ID
 * @return 形成的JSON请求格式
 */
char *MqttShadowRequestPayload(const char *device_id, const char *service_id)
{
    // 参数有效性检查
    if (!device_id || !service_id || strlen(device_id) == 0 || strlen(service_id) == 0)
    {
        return NULL;
    }

    // 创建JSON根对象
    cJSON *root = cJSON_CreateObject();
    if (!root)
        return NULL;

    // 添加设备ID字段
    cJSON_AddStringToObject(root, "object_device_id", device_id);
    // 添加服务ID字段
    cJSON_AddStringToObject(root, "service_id", service_id);

    // 生成紧凑型JSON
    char *payload = cJSON_PrintUnformatted(root);

    // 清理cJSON结构
    cJSON_Delete(root);

    return payload;
}

/**
 * @brief 发布消息，上传智能锁状态
 *
 * @param lock 智能锁状态
 * @return 形成的JSON发送格式
 */
char *MqttLockPublishPayload(bool lock)
{
    // 创建JSON结构
    cJSON *root = cJSON_CreateObject();
    cJSON *services = cJSON_CreateArray();

    // 构建服务节点
    cJSON *upload = cJSON_CreateObject();
    cJSON_AddStringToObject(upload, "service_id", "001");

    // 添加属性
    cJSON *props = cJSON_CreateObject();

    cJSON_AddBoolToObject(props, "lock", lock);
    cJSON_AddItemToObject(upload, "properties", props);

    // 组合结构
    cJSON_AddItemToArray(services, upload);
    cJSON_AddItemToObject(root, "services", services);

    // 生成字符串
    char *payload = cJSON_PrintUnformatted(root);

    // 释放cJSON树（注意：不释放payload字符串）
    cJSON_Delete(root);

    return payload;
}

/**
 * @brief 发送响应消息
 *
 * @return 形成的JSON响应格式
 */
char *MqttLockResponsePayload(void)
{
    // 1.创建JSON根对象
    cJSON *root = cJSON_CreateObject();
    if (!root)
        return NULL;

    // 2. 添加整型字段 result_code
    cJSON_AddNumberToObject(root, "result_code", 0);

    // 3. 添加字符串字段 result_desc
    cJSON_AddStringToObject(root, "result_desc", "success");

    // 4.生成字符串
    char *payload = cJSON_PrintUnformatted(root);

    // 5.释放cJSON树（注意：不释放payload字符串）
    cJSON_Delete(root);

    return payload;
}

/**
 * @brief 重写订阅话题的业务函数
 * 
 * @param payload 订阅话题下发的json格式数据
 * 
 */
void SubscribeTopicCallback(char* payload)
{
    /* 处理订阅的主题内容 */
    cJSON *root = cJSON_Parse((char *)payload);
    if (!root)
    {
        printf("[ERROR] JSON parse error\r\n");
        return;
    }
    // 1. 提取 object_device_id
    cJSON *obj_id = cJSON_GetObjectItem(root, "object_device_id");
    if (cJSON_IsString(obj_id))
    {
        printf("[INFO] Device ID: %s\r\n", obj_id->valuestring);
    }
    // 2. 处理 services 数组
    cJSON *services_arr = cJSON_GetObjectItem(root, "services");
    if (cJSON_IsArray(services_arr) && cJSON_GetArraySize(services_arr) > 0)
    {
        // 解析 services 数组第一个参数(多参数时需要套 for 循环)
        cJSON *first_entry = cJSON_GetArrayItem(services_arr, 0);

        // 2.1 提取 service_id
        cJSON *service_id = cJSON_GetObjectItem(first_entry, "service_id");
        if (cJSON_IsString(service_id))
        {
            printf("[INFO] Service ID: %s\r\n", service_id->valuestring);
        }

        // 2.2 处理 properties 属性
        cJSON *properties = cJSON_GetObjectItem(first_entry, "properties");
        // 处理 lock 属性
        if (properties)
        {
            cJSON *lock = cJSON_GetObjectItem(properties, "lock");
            if (cJSON_IsBool(lock))
            {
                // 日志输出
                printf("[INFO] Desired lock status (bool): %s\r\n", lock->valueint ? "True" : "False");
                // 记录智能锁状态
                bool lock_status = lock->valueint;
                // 发布事件标志
                osEventFlagsSet(g_smartLockEvent, (1 << (int)lock_status));
            }
        }
    }
    cJSON_Delete(root);
}

void MqttLockEventNew(void)
{
    osEventFlagsAttr_t attr = {
        .name = "lockEvent",
        .cb_size = 0,
        .attr_bits = 0,
        .cb_mem = NULL,
    };
    g_smartLockEvent = osEventFlagsNew(&attr);
}

uint32_t MqttLockEventWait(void)
{
    return osEventFlagsWait(g_smartLockEvent, 0x03, osFlagsWaitAny, osWaitForever);
}

void MqttLockEventClear(void)
{
    osEventFlagsClear(g_smartLockEvent, 0x03);
}

