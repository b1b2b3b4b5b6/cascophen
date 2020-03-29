#include "heater.h"

#define STD_LOCAL_LOG_LEVEL STD_LOG_DEBUG
#define TASK_PRI ESP_TASK_MAIN_PRIO + 1
#define TASK_SIZE 2048
#define SHAKE_TIME_MS 200

#define PIN_EN 4
#define PIN_LED 19
#define PIN_INPUT 21


static int g_level = 0;
static xQueueHandle g_queue = NULL;

void heater_led_restore()
{
    if(g_level == 0)
        heater_led_set(0);
    else
        heater_led_set(1);
}

void heater_led_set(int status)
{
    gpio_set_level(PIN_LED, status);
    STD_LOGI("heater len: %d", status);
}

static void led_on()
{
    heater_led_set(1);
}

static void led_off()
{
    heater_led_set(0);
}

static void heater_on()
{
    gpio_set_level(PIN_EN, 1);
}

static void heater_off()
{
    gpio_set_level(PIN_EN, 0);
}

static void heater_switch(int value)
{
    switch(value)
    {
        case 0:
            heater_off();
            led_off();
            break;
        case 1:
            heater_on();
            led_on();
            break;
        default:
            STD_END("not here");
            break;
    }
    STD_LOGI("heater[%d]", value);
}

static void heater_turn_around()
{
    switch(g_level)
    {
        case 0:
            g_level = 1;
            break;
        case 1:
            g_level = 0;
            break;
        default:
            STD_END("not here");
            break;
    }
    heater_switch(g_level);
}

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    static uint32_t press_time;
    if(gpio_get_level(PIN_INPUT) == 0)
    {
        press_time = esp_log_timestamp();
        xQueueSendFromISR(g_queue, &press_time, &xHigherPriorityTaskWoken);
    }

}

static int trig_init()
{
    gpio_config_t io_conf;
	//interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = 1ULL<<PIN_INPUT;
    //set as input mode    
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    io_conf.pull_down_en = 0;
    gpio_config(&io_conf);
    gpio_set_intr_type(PIN_INPUT, GPIO_INTR_NEGEDGE);
	//install gpio isr service
    gpio_install_isr_service(0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(PIN_INPUT, gpio_isr_handler, NULL);
	return 0;
}

static void gpio_pin_init()
{
    gpio_set_direction(PIN_EN, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_LED, GPIO_MODE_OUTPUT);
}

static void heater_task(void *arg)
{
    BaseType_t res;
    static uint32_t now_time;
    static uint32_t last_time = 0;
    for(;;)
    {
        res = xQueueReceive(g_queue, &now_time, portMAX_DELAY);
        STD_ASSERT(res == pdTRUE);
        if((now_time - last_time) < SHAKE_TIME_MS)
        {
            STD_LOGV("shake, drop");
            continue;
        }
        else
        {
            heater_turn_around();
        }
        last_time = now_time;
    }
}

void heater_init()
{
    g_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(heater_task, "heater task", TASK_SIZE, NULL, TASK_PRI, NULL);
    gpio_pin_init();
    trig_init();
    heater_off();
    led_off();
    STD_LOGI("heater init");
}


