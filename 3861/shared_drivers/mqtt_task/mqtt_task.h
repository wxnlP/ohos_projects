#ifndef MQTT_TASK_H
#define MQTT_TASK_H

#include <stdbool.h>



void MqttTaskInit(void);
void MqttTaskDeinit(void);
int MqttTaskConnect(const char *host, unsigned short port, 
    const char *clientId, const char *username, const char *password);
int MqttTaskSubscribe(char* topic);
int MqttTaskUnSubscribe(char* topic);
int MqttTaskPublish(char *topic, char *payload);
int MqttTaskDisconnect(void);
int MqttTaskIsConnected(void);

#endif