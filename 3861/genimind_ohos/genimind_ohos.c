#include "ohos_init.h"
#include <stdio.h>
#include <string.h>
#include "cmsis_os2.h"
#include "iot_i2c.h"
#include "iot_gpio.h"
#include "iot_uart.h"
#include "iot_errno.h"
#include "hi_io.h"
#include "aht20.h"
#include "wifi_connecter.h"
#include "mqtt_task.h"
#include "cJSON.h"

#define  GENIMIND_DEBUG 1

/* WiFi信息 */
// 账号
#define  HOTSPOT_SSID           "OpenHarmony"
// 密码 
#define  HOTSPOT_PASSWD         "123456789"
// 加密方式 
#define  HOTSPOT_TYPE           WIFI_SEC_TYPE_PSK
/* 华为云信息 */
// MQTT连接参数               
#define  MQTT_HOST              "9dfa5c258d.st1.iotda-device.cn-east-3.myhuaweicloud.com"     
#define  MQTT_PORT              1883
#define  MQTT_CLIENT_ID         "677388cfbab900244b135588_DATAS_0_0_2024123106"
#define  MQTT_USERNAME          "677388cfbab900244b135588_DATAS"
#define  MQTT_PASSWD            "ad31747e0b97a6ca1287043ca46a3571796f0d20e1056ecabec4f05fc2db545f"
#define  MQTT_DEVICE_ID         "677388cfbab900244b135588_DATAS"
#define  MQTT_SERVICE_ID        "Upload"
// HUAWEICLOUDE平台的话题定义  
#define  MQTT_PublishTopic      "$oc/devices/677388cfbab900244b135588_DATAS/sys/properties/report"
#define  MQTT_RequestTopic      "$oc/devices/677388cfbab900244b135588_DATAS/sys/shadow/get/request_id={request_id}"
#define  MQTT_SubscribeTopic    "$oc/devices/677388cfbab900244b135588_DATAS/sys/shadow/get/response/#"
// AHT20信息
#define  AHT20_BAUDRATE         400 * 1000
#define  AHT20_I2C_IDX          1
// MQ-2 
#define  MQ_2_HAZARDOUS         0
#define UART_ID        1
#define UART_TX_PIN    6
#define UART_RX_PIN    5


// 温湿度数据
static float g_temperature = 0;
static float g_humidity = 0; 
static IotGpioValue g_mq2;
static IotGpioValue g_fall;
// 互斥锁保护全局变量
osMutexId_t g_mutexTempAndHumi;
osMutexId_t g_mutexMQ2;
osMutexId_t g_mutexFall;

/**
 * @brief 封装cmsis_os2的 {@link osMutexNew} 函数
 * 
 * @param name 互斥锁名称
 * @return 返回互斥锁ID 
 */
static osMutexId_t MutexNew(char* name)
{
    osMutexAttr_t attr_mutex = {
        .name = name,
        .attr_bits = 0,
        .cb_mem = NULL,
        .cb_size = 0
    };
    osMutexId_t mutex_id = osMutexNew(&attr_mutex);
    if (mutex_id == NULL) {
        printf("[Mutex Create] osMutexNew(%s) failed.\r\n", name);
    } else{
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
static osThreadId_t ThreadNew(char *name_, osThreadFunc_t func, uint32_t stackSize)
{
    osThreadAttr_t attr = {
        .name = name_,
        .stack_size = stackSize,
        .priority = osPriorityNormal
    };
    osThreadId_t thread_id = osThreadNew(func, NULL, &attr);
    if (thread_id == NULL) {
        printf("[Thread Create] osThreadNew(%s) failed.\r\n", name_);
    } else{
        printf("[Thread Create] osThreadNew(%s) success, thread id: %d.\r\n", name_, thread_id);
    }
    return thread_id;
}

static void MutexInit(void)
{
    /* 创建互斥锁 */
    g_mutexTempAndHumi = MutexNew("mutexTempAndHumi");
    g_mutexMQ2 = MutexNew("mutexMQ2");
    g_mutexFall = MutexNew("mutexFall");
}

static void Uart1Init(void)
{
    /* 复用UART1到GPIO5、GPIO6 */
    IoTGpioInit(UART_TX_PIN);
    IoTGpioInit(UART_RX_PIN);
    hi_io_set_func(UART_TX_PIN, HI_IO_FUNC_GPIO_6_UART1_TXD);
    hi_io_set_func(UART_RX_PIN, HI_IO_FUNC_GPIO_5_UART1_RXD);
    /* 配置UART1的属性 */
    IotUartAttribute uartAttr = {
        // 波特率
        .baudRate = 9600,
        // 数据位长度
        .dataBits = IOT_UART_DATA_BIT_8,
        // 停止位长度
        .stopBits = IOT_UART_STOP_BIT_1,
        // 奇偶校验
        .parity = IOT_UART_PARITY_NONE,
        // 发送且接收
        .rxBlock = IOT_UART_BLOCK_STATE_BLOCK,
        .txBlock = IOT_UART_BLOCK_STATE_BLOCK,
        .pad = 0,
    };
    if (IoTUartInit(UART_ID, &uartAttr) != IOT_SUCCESS) {
        printf("[ERROR] UART INIT ERR.\r\n");
    }
}

/**
 * @brief 发布消息，上传温湿度、MQ2数据
 * 
 * @param temp 温度
 * @param humidity 湿度
 * @param mq2 MQ2是否超阈值
 * @return 形成的JSON发送格式
 */
static char* MqttPublishPayload(float temp, float humidity, int mq2, int fall) {
    // 创建JSON结构
    cJSON *root = cJSON_CreateObject();
    cJSON *services = cJSON_CreateArray();
    
    // 构建服务节点
    cJSON *upload = cJSON_CreateObject();
    cJSON_AddStringToObject(upload, "service_id", "Upload");
    
    // 添加属性
    cJSON *props = cJSON_CreateObject();
    cJSON_AddNumberToObject(props, "temperature", temp);
    cJSON_AddNumberToObject(props, "humidity", humidity);
    cJSON_AddNumberToObject(props, "MQ2", mq2);
    cJSON_AddNumberToObject(props, "fall", fall);
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

static void MqttDataUploadTask(void* arg)
{
    (void)arg;
    /* 初始化WIFI参数 */
    WifiDeviceConfig apConfig = {
        // 热点名称
        .ssid = HOTSPOT_SSID,
        // 热点密码
        .preSharedKey = HOTSPOT_PASSWD,
        // 加密方式(PSK)
        .securityType = HOTSPOT_TYPE,
    };

    /* 连接WIFI */
    int netId = ConnectToHotspot(&apConfig);
    if (netId < 0) {
        printf("[WIFI ERROR] Connect to AP failed!\r\n");
    }

    /* 初始化并启动MQTT任务，连接MQTT服务器 */
    MqttTaskInit(); 
    printf("[MQTT INFO] 等待连接MQTT服务器\r\n");
    while (MqttTaskConnect(MQTT_HOST, MQTT_PORT, MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWD) != 0) {
        printf(".");
        osDelay(5);
    }
    printf("\r\n");
    printf("[MQTT INFO] 成功连接MQTT服务器\r\n");

    while (1)
    {
        /* 检测MQTT服务器连接状态 */
        printf("[DEBUG] MQTT 服务器状态: %d\n", MqttTaskIsConnected());
        if (MqttTaskIsConnected() != 1) {
            printf("[MQTT INFO] 尝试重新连接MQTT服务器\r\n");
            while (MqttTaskConnect(MQTT_HOST, MQTT_PORT, MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWD) != 0) {
                osDelay(5);
            }
            printf("[MQTT INFO] 成功重新连接MQTT服务器\r\n");
        }

        /* 发布消息 */
        char* payload = NULL;
        if (osMutexAcquire(g_mutexFall, 0) == osOK) {
            if (osMutexAcquire(g_mutexTempAndHumi, 0) == osOK) {
                if (osMutexAcquire(g_mutexMQ2, 0) == osOK) {
                    payload = MqttPublishPayload(g_temperature, g_humidity, 1, g_fall); 
                    
                    osMutexRelease(g_mutexMQ2);
                } 
                #if GENIMIND_DEBUG
                else {
                    printf("[MQTT WARN] 未获得互斥锁g_mutexMQ2\r\n");
                }
                #endif
                osMutexRelease(g_mutexTempAndHumi);
            } 
            #if GENIMIND_DEBUG
            else {
                printf("[MQTT WARN] 未获得互斥锁g_mutexTempAndHumi\r\n");
            }
            #endif
            osMutexRelease(g_mutexFall);
        }
        if (payload != NULL) {
            MqttTaskPublish(MQTT_PublishTopic, payload); 
            cJSON_free(payload);
        }
        
        // if (rc != 0) {
        //     // 发布失败，输出错误信息
        //     printf("[MQTT ERROR] MQTT Publish failed!\r\n"); 
        // } else {
        //     // 发布成功，输出成功信息
        //     printf("[MQTT INFO] MQTT Publish OK\r\n");
        // }
        osDelay(300);
    }
}

static void DataGetTask(void* arg)
{
    // 引脚初始化
    IoTGpioInit(HI_IO_NAME_GPIO_0);
    IoTGpioInit(HI_IO_NAME_GPIO_1);
    IoTGpioInit(HI_IO_NAME_GPIO_11);

    hi_io_set_func(HI_IO_NAME_GPIO_0, HI_IO_FUNC_GPIO_0_I2C1_SDA);
    hi_io_set_func(HI_IO_NAME_GPIO_1, HI_IO_FUNC_GPIO_1_I2C1_SCL);
    hi_io_set_func(HI_IO_NAME_GPIO_11, HI_IO_FUNC_GPIO_11_GPIO);

    IoTI2cInit(AHT20_I2C_IDX, AHT20_BAUDRATE);
    IoTGpioSetDir(HI_IO_NAME_GPIO_11, IOT_GPIO_DIR_IN);

    // AHT20初始化
    uint32_t retval = AHT20Init();
    printf("[AHT20 INFO] AHT20校准结果 %d\r\n", retval);
    /* 处理环境传感器数据 */
    while (1)
    {   
        /* 1.有害气体浓度检测 */
        if (osMutexAcquire(g_mutexMQ2, 0) == osOK) {
            IoTGpioGetInputVal(HI_IO_NAME_GPIO_11, &g_mq2);
            // 释放互斥锁
            osMutexRelease(g_mutexMQ2);
        } 
        #if GENIMIND_DEBUG
        else {
            printf("[MQ-2 WARN] 未获得互斥锁\r\n");
        }
        #endif
        if (g_mq2 == MQ_2_HAZARDOUS) {
            /* 添加危险警报⚠⚠⚠ */
            printf("[MQ-2 WARN] 有害气体浓度过高\r\n");
        } 
        #if GENIMIND_DEBUG
        else {
            printf("[MQ-2 INFO] 无有害气体危险\r\n");
        }
        #endif
        /* 2.温湿度检测 */
        if (osMutexAcquire(g_mutexTempAndHumi, 0) == osOK) {
            if (AHT20MeasureResult(&g_humidity, &g_temperature) == IOT_SUCCESS) {
                /* 添加温湿度异常警报⚠⚠⚠ */
                printf("[AHT20 INFO] 湿度 %f 温度 %f\r\n", g_humidity, g_temperature);
            } else {
                printf("[AHT20 ERROR] 获取测量结果失败\r\n");
            }
            // 释放互斥锁
            osMutexRelease(g_mutexTempAndHumi);
        }
        #if GENIMIND_DEBUG
        else {
            printf("[AHT20 WARN] 未获得互斥锁\r\n");
        }
        #endif
        osDelay(200);
    }
}

static void UartTask(void* arg)
{
    /* 初始化串口 */
    Uart1Init();
    static char buffer[2] = {0};
    IotGpioValue flag;
    while (1) 
    {
        if (IoTUartRead(UART_ID, buffer, 1) < 0) {
            printf("[ERROR] IoTUartWrite ERR.\r\n");
        } else {
            printf("[INFO] IoTUartWrite RTT.\r\n");
            printf("[INFO] 收到串口数据 %c\r\n", buffer[0]);
            if (buffer[0] == 'F') {
                flag = IOT_GPIO_VALUE1;
            } else {
                flag = IOT_GPIO_VALUE0;
            }
            // 获取互斥锁修改全局变量
            if (osMutexAcquire(g_mutexFall, 0) == osOK) {
                g_fall = flag;
                // 释放互斥锁
                osMutexRelease(g_mutexFall);
            }
        }
        osDelay(100);
    }
}

static void GenimindEntry(void)
{
    MutexInit();
    ThreadNew("mqttTask", MqttDataUploadTask, 10240);
    ThreadNew("dataGetTask", DataGetTask, 1024*3);
    ThreadNew("uartTask", UartTask, 1024);
}

APP_FEATURE_INIT(GenimindEntry);