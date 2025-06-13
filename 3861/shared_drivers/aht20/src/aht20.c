#include "stdio.h"
#include "aht20.h"
#include "iot_i2c.h"
#include "iot_errno.h"

/* IIC的ID */
#define AHT20_I2C_IDX                   1
/* 上电等候 */
#define AHT20_POWER_ON_TIME             40*1000    //us
/* AHT20状态字 */
#define AHT20_STATUS_CMD                0x71
#define AHT20_STATUS_RESPONSE_MAX       
/* 从机地址 */
#define AHT20_ADDR_WRITE                (0x38 << 1) | 0x00
#define AHT20_ADDR_READ                 (0x38 << 1) | 0x01
/* AHT20初始化 */
#define AHT20_CALIBRATION_CMD           0xBE
#define AHT20_CALIBRATION_CMD_ARG0      0x08
#define AHT20_CALIBRATION_CMD_ARG1      0x00
#define AHT20_CALIBRATION_TIME          10*1000    //us
/* AHT20触发测量 */
#define AHT20_TRIGGER_MEASURE_CMD       0xAC
#define AHT20_TRIGGER_MEASURE_CMD_ARG0  0x33
#define AHT20_TRIGGER_MEASURE_CMD_ARG1  0x00
#define AHT20_TRIGGER_MEASURE_TIME      80*1000    //us
/* AHT20测量最大尝试次数 */
#define AHT20_MEASURE_TRY_MAX           10

/** 
 * @brief AHT20写入数据
 * 
 * 基于HAL接口函数 IoTI2cWrite
 * 
 * @param data 发送数据的指针
 * @param length 数据字节长度
 * 
 * @return 写入成功返回 {@link IOT_SUCCESS}，写入失败返回 {@link IOT_FAILURE}
*/
static uint32_t AHT20Write(const uint8_t* data, uint32_t length)
{
    // 写入数据
    uint32_t retval = IoTI2cWrite(AHT20_I2C_IDX, AHT20_ADDR_WRITE, data, length);
    // 判断返回值
    if (retval != IOT_SUCCESS) {
        printf("[AHT20 ERROR] 数据写入失败.\r\n");
        return IOT_FAILURE;
    }
    return IOT_SUCCESS;
}

/** 
 * @brief AHT20读取数据
 * 
 * 基于HAL接口函数 IoTI2cRead
 * 
 * @param[out] data 接收数据的指针
 * @param length 数据字节长度
 * 
 * @return 读取成功返回 {@link IOT_SUCCESS}，读取失败返回 {@link IOT_FAILURE}
*/
static uint32_t AHT20Read(uint8_t* data, uint32_t length)
{
    // 写入数据
    uint32_t retval = IoTI2cRead(AHT20_I2C_IDX, AHT20_ADDR_READ, data, length);
    // 判断返回值
    if (retval != IOT_SUCCESS) {
        printf("[AHT20 ERROR] 数据读取失败.\r\n");
        return IOT_FAILURE;
    }
    return IOT_SUCCESS;
}

/**
 * @brief 获取状态字
 * 
 * @param[out] statusWord 指向状态字的指针
 * @return  获取成功返回指定 {@link IOT_SUCCESS}，获取失败返回 {@link IOT_FAILURE}
 */
static uint32_t AHT20GetStatusWord(uint8_t* statusWord)
{ 
    uint8_t buffer[] = { AHT20_STATUS_CMD };
#ifdef Scheme
    // 发送获取状态字命令
    if (AHT20Write(buffer, sizeof(buffer)) == IOT_SUCCESS) {
        // 读取状态字
        if (AHT20Read(statusWord, 1) == IOT_SUCCESS) {
            // 打印状态位
            printf("[AHT20 INFO] Bit[%d]为%d\r\n", bit, data[0] & (0x01 << bit));
            // 返回状态位
            return statusWord;
        } else {
            return IOT_FAILURE;
        }
    } else {
        return IOT_FAILURE;
    }
#else
    // 发送获取状态字命令
    if (AHT20Write(buffer, sizeof(buffer)) != IOT_SUCCESS) {
        return IOT_FAILURE;
    }
    // 读取状态字
    if (AHT20Read(statusWord, 1) != IOT_SUCCESS) {
        return IOT_FAILURE;
    } 
    return IOT_SUCCESS;
#endif
}

/**
 * @brief 根据状态字解析指定状态位
 * 
 * 请先执行 {@link AHT20GetStatusWord} 获取状态字
 * 
 * @param statusWord 状态字
 * @param bit 要获取的状态位 {@link StatusBits}
 * @return 返回状态位
 */
static uint32_t AHT20GetStatusBit(uint8_t statusWord, StatusBits bit)
{
    uint32_t status_bit = ((uint32_t)statusWord & (0x01 << bit)) >> bit;
    printf("[AHT20 INFO] Bit[%d]为%d\r\n", bit, status_bit);
    return status_bit;
}

/**
 * @brief 发送AHT20校准命令
 * 
 * @return 校准成功返回指定 {@link IOT_SUCCESS}，校准失败返回 {@link IOT_FAILURE}
 */
static uint32_t AHT20Calibrate(void)
{
    static uint8_t buffer[3] = { AHT20_CALIBRATION_CMD, AHT20_CALIBRATION_CMD_ARG0, AHT20_CALIBRATION_CMD_ARG1 };
    return AHT20Write(buffer, sizeof(buffer));
}

/**
 * @brief 发送AHT20触发测量命令
 * 
 * @return 触发测量成功返回指定 {@link IOT_SUCCESS}，触发测量失败返回 {@link IOT_FAILURE}
 */
static uint32_t AHT20TriggerMeasure(void)
{
    static uint8_t buffer[3] = { AHT20_TRIGGER_MEASURE_CMD, AHT20_TRIGGER_MEASURE_CMD_ARG0, AHT20_TRIGGER_MEASURE_CMD_ARG1 };
    return AHT20Write(buffer, sizeof(buffer));
}

/** 
 * @brief 上电初始化
 * 
 * 上电后要等待40ms，读取温湿度值之前， 首先要看状态字的校准使能位Bit[3]是否为 1
 * (通过发送0x71可以获取一个字节的状态字)，如果不为1，要发送0xBE命令(初始化)，此
 * 命令参数有两个字节， 第一个字节为0x08，第二个字节为0x00,然后等待10ms。
 * 
 * @return 读取成功返回 {@link IOT_SUCCESS}，读取失败返回 {@link IOT_FAILURE}
*/
uint32_t AHT20Init(void)
{
    // 上电后等待40ms
    usleep(AHT20_POWER_ON_TIME);
    // 获取状态字
    uint8_t data[0];
    uint32_t retval = AHT20GetStatusWord(&data[0]);
    if (retval != IOT_SUCCESS) {
        printf("[AHT20 ERROR] 获取状态字失败\r\n");
        return IOT_FAILURE;
    }
    // 分析状态字的校准使能位Bit[3]
    if (AHT20GetStatusBit(data[0], STATUS_BITS_CAL_ENABLE) != CAL_ENABLE) {
        // 校准
        retval = AHT20Calibrate();
        if (retval != IOT_SUCCESS) {
            printf("[AHT20 ERROR] 校准失败\r\n");
            return IOT_FAILURE;
        }
        // 等待10ms
        usleep(AHT20_CALIBRATION_TIME);
        return retval;
    } 
    return IOT_SUCCESS;
}

/**
 * @brief 读取测量结果
 * 
 * @param humidity 指向湿度的指针
 * @param temperature 指向温度的指针
 * @return 读取成功返回 {@link IOT_SUCCESS}，读取失败返回 {@link IOT_FAILURE} 
 */
uint32_t AHT20MeasureResult(float* humidity, float* temperature)
{
    static uint8_t data[7] = {0}; 
    int i;
    // 触发测量
    if (AHT20TriggerMeasure() != IOT_SUCCESS) {
        printf("[AHT20 ERROR] 触发测量失败\r\n");
        return IOT_FAILURE;
    }
    // 等待80ms待测量完成
    usleep(AHT20_TRIGGER_MEASURE_TIME);
    // osDelay(8);
    // 读取1个字节状态字、5个字节温湿度数据和1字节CRC校验
    uint32_t retval = AHT20Read(data, 7);
    if (retval != IOT_SUCCESS) {
        printf("[AHT20 ERROR] 测量失败\r\n");
        return IOT_FAILURE;
    }
    // 解析状态字的Bit[7]是否测量完成
    for (i = 0 ; 
        AHT20GetStatusBit(data[0], STATUS_BITS_BUSY_INDICATION) != BUSY_NO && i < AHT20_MEASURE_TRY_MAX ; 
        i++ ) {
        // 设备忙继续等待
        usleep(AHT20_TRIGGER_MEASURE_TIME);
        // osDelay(8);
        // 再次读取
        retval = AHT20Read(data, 7);
        if (retval != IOT_SUCCESS) {
            printf("[AHT20 ERROR] 测量失败\r\n");
            return IOT_FAILURE;
        }
    }
    // 判断超时
    if (i > AHT20_MEASURE_TRY_MAX) {
        printf("[AHT20 ERROR] 测量超时\r\n");
        return IOT_FAILURE;
    }
    // 拼接温湿度字节
    uint32_t humi_raw = (data[1] << 8) | data[2];
    humi_raw = (humi_raw << 4) | ((data[3] & 0xF0) >> 4);
    
    uint32_t temp_raw = ((data[3] & 0x0F) << 8) | data[4];
    temp_raw = (temp_raw << 8) | data[5];
    // 计算温湿度真实值
    *humidity = humi_raw / (float)(1 << 20) * 100;
    *temperature = temp_raw / (float)(1 << 20) * 200 - 50;

    return IOT_SUCCESS;
}