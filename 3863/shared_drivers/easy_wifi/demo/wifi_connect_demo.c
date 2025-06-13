 /*
 Copyright (C) 2024 HiHope Open Source Organization .
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

#include <stdio.h>     
#include <string.h>    
#include "ohos_init.h" 
#include "cmsis_os2.h" 
#include "wifi_device.h"
#include "wifi_connecter.h" 

#define SSID "HUAWEI_Gao"
#define KEY "11111111"


static void WifiConnectTask(void)
{
    int timeout = 60;
    if (ConnectToHotspot(SSID,KEY) != 0)
    {
        printf("Connect to AP failed!\n");
    }
    else
    {
        printf("Connect to AP success!\n");
    }

     
    while (timeout--) {
        printf("After %d seconds I will disconnect with AP!\r\n", timeout);
        osDelay(100);
    }

     // 断开热点连接
    DisconnectWithHotspot();
}


static void WifiConnectDemo(void)
{

    osThreadAttr_t attr;
    attr.name = "WifiConnectTask";
    attr.stack_size = 0x1000;
    attr.priority = osPriorityNormal;
    if (osThreadNew(WifiConnectTask, NULL, &attr) == NULL)
    {
        printf("[WifiConnectDemo] Falied to create WifiConnectTask!\n");
    }
}

SYS_RUN(WifiConnectDemo);
