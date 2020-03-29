/* UART Select Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "sensor2.h"

#define STD_LOCAL_LOG_LEVEL STD_LOG_DEBUG
#define EX_UART_NUM UART_NUM_2
#define RD_BUF_SIZE (2048)
#define EVENT_QUENE_LENGTH  20
#define TX_IO 32
#define RX_IO 33
#define DATA_RES_TIME_MS 1000

typedef enum sensor_mode_t {
    SENSOR_ASK = 1,
    SENSOR_AUTO,
} sensor_mode_t;

static uint8_t ask_code[8] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x84, 0x0a};

static QueueHandle_t uart_air_queue;
static QueueHandle_t data_queue;

static void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    uint8_t *buf;
    uint len ;
    for(;;) {
        //Waiting for UART event.
        if(xQueueReceive(uart_air_queue, (void * )&event, (portTickType)portMAX_DELAY)) {
            STD_LOGV( "uart[%d] event:", EX_UART_NUM);
            switch(event.type) {
                //Event of UART receving data
                /*We'd better handler data event fast, there would be much more data events than
                other types of events. If we take too much time on data event, the queue might
                be full.*/
                case UART_DATA:
                    
                    buf = STD_CALLOC(100,1);
                    len = uart_read_bytes(EX_UART_NUM, buf, 100, 100 / portTICK_PERIOD_MS);
                    //STD_LOGD("recevie data[%d]", len);
                    if(len == 7)
                    {
                        PRINT_HEX(buf, 7);
                        xQueueSend(data_queue, &buf, portMAX_DELAY);
                    }
                    break;
                //Event of HW FIFO overflow detected
                case UART_FIFO_OVF:
                    STD_LOGE("hw fifo overflow");
                    // If fifo overflow happened, you should consider adding flow control for your application.
                    // The ISR has already reset the rx FIFO,
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(EX_UART_NUM);
                    xQueueReset(uart_air_queue);
                    break;
                //Event of UART ring buffer full
                case UART_BUFFER_FULL:
                    STD_LOGE("ring buffer full");
                    // If buffer full happened, you should consider encreasing your buffer size
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(EX_UART_NUM);
                    xQueueReset(uart_air_queue);
                    break;
                //Event of UART RX break detected
                case UART_BREAK:
                    STD_LOGD( "uart rx break");
                    break;
                //Event of UART parity check error
                case UART_PARITY_ERR:
                    STD_LOGE("uart parity error");
                    break;
                //Event of UART frame error
                case UART_FRAME_ERR:
                    STD_LOGE("uart frame error");
                    break;
                //UART_PATTERN_DET 
                case UART_PATTERN_DET:
                    STD_LOGE("UART_PATTERN_DET");
                    break;
                //Othersp
                default:
                    STD_LOGE("uart event type: %d", event.type);
                    break;
            }
        }
    }

    vTaskDelete(NULL);
}

uint8_t *get_sensor_data(int time_ms)
{
    uint8_t *data;
    if(xQueueReceive(data_queue, &data, time_ms/portTICK_PERIOD_MS) == pdTRUE)
        return data;
    else
        return NULL;
}

int sensor2_singal_measure(float *value)
{
    uint8_t *buf;
    uart_write_bytes(EX_UART_NUM, (const char *)ask_code, 8);
    buf = get_sensor_data(DATA_RES_TIME_MS);
    if(buf == NULL)
    {
        STD_LOGE("wait fail");
        return -1;
    }
    uint8_t low = buf[4];
    uint8_t high = buf[3];
    int raw_value = high*256+low;
    *value = raw_value;
    *value = *value * 0.01 * 1.341;
    *value = *value * 1000;

    STD_LOGM("sensor measure: %f", *value);

    STD_FREE(buf);
    return 0;
}

int sensor2_init()
{
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    ESP_ERROR_CHECK(uart_param_config(EX_UART_NUM, &uart_config));
    

    //Set UART pins (using UART0 default pins ie no changes.)
    ESP_ERROR_CHECK(uart_set_pin(EX_UART_NUM, TX_IO, RX_IO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    //Install UART driver, and get the queue.
    ESP_ERROR_CHECK(uart_driver_install(EX_UART_NUM, RD_BUF_SIZE, 0, EVENT_QUENE_LENGTH, &uart_air_queue, 0));
    //Set uart pattern detect function.
    //ESP_ERROR_CHECK(uart_enable_pattern_det_intr(EX_UART_NUM, (char)PATTERN_CHR, PATTERN_CHR_NUM, 10000, 10, 10));
    //Reset the pattern queue length to record at most 20 pattern positions.
    //ESP_ERROR_CHECK(uart_pattern_queue_reset(EX_UART_NUM, PATTERN_LENGTH));

    //Create a task to handler UART event from ISR
    xTaskCreate(uart_event_task, "uart_event_task", 2048, NULL, 12, NULL);
    data_queue = xQueueCreate(10, sizeof(uint8_t *));
    return ESP_OK;
}