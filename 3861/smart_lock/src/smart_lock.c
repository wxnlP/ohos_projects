#include "ohos_init.h"
#include <stdio.h>
#include "wifi_connecter.h"
#include "mqtt_lock.h"
#include "cJSON.h"

/* WiFi信息 */
// 账号
#define HOTSPOT_SSID "OpenHarmony"
// 密码
#define HOTSPOT_PASSWD "123456789"
// WIFI加密方式
#define HOTSPOT_TYPE WIFI_SEC_TYPE_PSK
/* 华为云信息 */
// MQTT连接参数
#define MQTT_HOST "9dfa5c258d.st1.iotda-device.cn-east-3.myhuaweicloud.com"
#define MQTT_PORT 1883
#define MQTT_CLIENT_ID "6715172e30d2f15dece161c7_2025_4_25_0_0_2025042510"
#define MQTT_USERNAME "6715172e30d2f15dece161c7_2025_4_25"
#define MQTT_PASSWD "d74af1e828a09594a65a6b707c8321ba029966350b832a15447cbb357cc89dc0"
#define MQTT_DEVICE_ID "6715172e30d2f15dece161c7_2025_4_25"
#define MQTT_SERVICE_ID "001"
// HUAWEICLOUDE平台的话题定义
#define MQTT_PublishTopic "$oc/devices/6715172e30d2f15dece161c7_2025_4_25/sys/properties/report"
#define MQTT_RequestTopic "$oc/devices/6715172e30d2f15dece161c7_2025_4_25/sys/shadow/get/request_id={request_id}"
#define MQTT_SubscribeTopic "$oc/devices/6715172e30d2f15dece161c7_2025_4_25/sys/shadow/get/response/#"
#define MQTT_ShadowSubscribeTopic "$oc/devices/6715172e30d2f15dece161c7_2025_4_25/sys/properties/set/#"
#define MQTT_ShadowResponseTopic "$oc/devices/6715172e30d2f15dece161c7_2025_4_25/sys/properties/set/response/request_id={request_id}"

// 事件标志组
#define LOCK_ON (1 << 0)
#define LOCK_OFF (1 << 1)


static void MqttTask(void *arg)
{
    (void)arg;
    MqttLockEventNew();
    /* 1.初始化WIFI参数 */
    WifiDeviceConfig apConfig = {
        // 热点名称
        .ssid = HOTSPOT_SSID,
        // 热点密码
        .preSharedKey = HOTSPOT_PASSWD,
        // 加密方式(PSK)
        .securityType = HOTSPOT_TYPE,
    };

    /* 2.连接WIFI */
    int netId = ConnectToHotspot(&apConfig);
    if (netId < 0)
    {
        printf("[ERROR] Connect to AP failed!\r\n");
    }

    /* 3.初始化并启动MQTT任务，连接MQTT服务器 */
    MqttTaskInit();
    printf("[MQTT INFO] 等待连接MQTT服务器\r\n");
    while (MqttTaskConnect(MQTT_HOST, MQTT_PORT, MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWD) != 0)
    {
        printf(".");
        osDelay(5);
    }
    printf("\r\n");
    printf("[MQTT INFO] 成功连接MQTT服务器\r\n");

    /* 4.订阅主题 */
    printf("[MQTT INFO] 等待订阅话题\r\n");
    while (MqttTaskSubscribe(MQTT_ShadowSubscribeTopic) != 0)
    {
        printf(".");
        osDelay(5);
    }
    printf("\r\n");
    printf("[MQTT INFO] 成功订阅话题\r\n");

}

static void LockWorkTask(void *arg)
{
    while (1)
    {
        uint32_t flags = MqttLockEventWait();
        bool lockStatus;
        if (flags & 0x80000000)
        {
            printf("[ERROR] Flag Error.\r\n");
        }
        else
        {
            if (flags == LOCK_ON)
            {
                lockStatus = false;
                /* 添加UART关锁 */
                printf("[INFO] LOCK ON\r\n");
            }
            else if (flags == LOCK_OFF)
            {
                lockStatus = true;
                /* 添加UART开锁 */
                printf("[INFO] LOCK OFF\r\n");
            }
            // 发布响应至华为云平台，否则会处于5分钟保护状态
            char *payload = MqttLockResponsePayload();
            MqttTaskPublish(MQTT_ShadowResponseTopic, payload);
            printf("[INFO] 发送响应\r\n");
            // 更新设备属性
            payload = MqttLockPublishPayload(lockStatus);
            MqttTaskPublish(MQTT_PublishTopic, payload);
            printf("[INFO] 发送属性更新\r\n");
            MqttLockEventClear();
        }
    }
}

/* 入口函数 */
static void LockEntry(void)
{
    if (ThreadNew("lockTask", LockWorkTask, 10240) == NULL)
    {
        printf("[ERROR] Thread Create Faild.\r\n");
    }
    if (ThreadNew("mqttTask", MqttTask, 10240) == NULL)
    {
        printf("[ERROR] Thread Create Faild.\r\n");
    }
}

// APP_FEATURE_INIT(LockEntry);