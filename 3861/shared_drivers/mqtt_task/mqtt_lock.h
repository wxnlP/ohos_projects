#ifndef MQTT_LOCK_H
#define MQTT_LOCK_H

#include "mqtt_task.h"
#include "cmsis_os2.h"

osMutexId_t MutexNew(char *name);
osThreadId_t ThreadNew(char *name_, osThreadFunc_t func, uint32_t stackSize);
char* MqttShadowRequestPayload(const char* device_id, const char* service_id);
char* MqttLockPublishPayload(bool lock);
char* MqttLockResponsePayload(void);
void MqttLockEventNew(void);
uint32_t MqttLockEventWait(void);
void MqttLockEventClear(void);

#endif