

#include "std_port_common.h"
#include "std_rdebug.h"
#include "request.h"
#include "sensor1.h"
#include "sensor2.h"
#include "std_reboot.h"
#include "std_blufi.h"
#include "std_ota.h"
#include "heater.h"
#include "fan.h"

#define STD_LOCAL_LOG_LEVEL STD_LOG_DEBUG

#define MAX_ERROR 50

#define TASK_SIZE 2048
#define TASK_PRI ESP_TASK_MAIN_PRIO
static int mesure_peroid = 1000;

enum {
    NET_TURN_OFF,
    NET_TURN_ON,
    NET_OFFING,
    NET_ONING,
}net_status;

static void network_status()
{
    static int flag = NET_OFFING;
    for(;;)
    {
        switch(flag)
        {
            case NET_OFFING:
                STD_LOGI("NET_OFFING");
                if(std_wifi_is_connect())
                {
                    flag = NET_TURN_ON;
                    break;
                }

                fan_led_set(0);
                heater_led_set(0);
                vTaskDelay(500 / portTICK_PERIOD_MS);
                fan_led_set(1);
                heater_led_set(1);
                vTaskDelay(500 / portTICK_PERIOD_MS);
                break;

            case NET_TURN_ON:
                STD_LOGI("NET_TURN_ON");
                fan_led_restore();
                heater_led_restore();
                flag = NET_ONING;
                break;

            case NET_ONING:
                //STD_LOGI("NET_ONING");
                if(!std_wifi_is_connect())
                flag = NET_TURN_OFF;
                break;

            case NET_TURN_OFF:
                STD_LOGI("NET_TURN_OFF");
                flag = NET_OFFING;
                break;
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void app_main()
{
    int res;
    std_nvs_init();
    std_reboot_init();
    std_rdebug_init(false);
    std_wifi_init();
    fan_init();
    heater_init();
    xTaskCreate(network_status, "network", TASK_SIZE, NULL, TASK_PRI, NULL);
    std_wifi_connect("airdr", "airdr1234");
    std_wifi_wait_connect(0);
    std_ota_init(NULL, "cascophen.bin");

    char *version = std_ota_upstream_version();
    if(version != NULL)
    {
        res = std_ota_check_version(version);
        if(res == 0)
        {
            fan_led_set(1);
            heater_led_set(1);
            std_ota_http_image();
            fan_led_restore(0);
            heater_led_restore(0);
        }
    }
    
    request_init();
    sensor1_init();

    float value;
    while(1)
    {
        vTaskDelay(mesure_peroid / portTICK_PERIOD_MS);
        sensor1_singal_measure(&value);
        STD_LOGI("[%d]", (int)value);
        request_post(value);   
    }

	vTaskDelay(portMAX_DELAY);
}
