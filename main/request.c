/* HTTP GET Example using plain POSIX sockets

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "std_wifi.h"
#include "esp_http_client.h"
#include "std_wifi.h"

#define STD_LOCAL_LOG_LEVEL STD_LOG_DEBUG

#define SERVER_IP "http://101.132.42.189:5001"
#define API_PATH "/api/upload/methanal/info"

static esp_http_client_handle_t client;

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            STD_LOGD("HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            //STD_LOGD("HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            //STD_LOGD("HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            //STD_LOGD("HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            // STD_LOGD("HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            // if (!esp_http_client_is_chunked_response(evt->client)) {
            //     printf("%.*s", evt->data_len, (char*)evt->data);
            // }
            break;
        case HTTP_EVENT_ON_FINISH:
            //STD_LOGD("HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            //STD_LOGD("HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}

void request_init()
{

}

static char *build_bodystr(float value)
{
    char *bodystr = STD_CALLOC(1000, 1);
    sprintf(bodystr, "{\"mac\":\"%s\",\"hcho\":%d}", std_wifi_get_stamac_str(), (int)value);
    return bodystr;
}

int request_post(float value)
{
    int err;
    static int fail_sum = 0;
    if(!std_wifi_is_connect())
        return -1;

    esp_http_client_config_t config = {
        .url = SERVER_IP""API_PATH,
        .event_handler = _http_event_handler,
    };
    client = esp_http_client_init(&config);

    esp_http_client_set_url(client, SERVER_IP""API_PATH);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    char *bodystr = build_bodystr(value);
    STD_LOGI("body[%d]: %s",strlen(bodystr), bodystr);
    esp_http_client_set_post_field(client, bodystr, strlen(bodystr));
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        STD_LOGI("HTTP POST Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
        fail_sum = 0;
    }
    else 
    {
        STD_LOGE("HTTP POST request failed: %s", esp_err_to_name(err));
        fail_sum ++ ;
        if(fail_sum >= 3)
        {
            STD_LOGE("fail count overflow ,restart");
            std_wifi_disconnect();
            std_wifi_connect(NULL,NULL);
        } 
    }
    STD_FREE(bodystr);
    esp_http_client_cleanup(client);
    return 0;
}