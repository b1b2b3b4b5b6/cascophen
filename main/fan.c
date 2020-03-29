#include "fan.h"

#define STD_LOCAL_LOG_LEVEL STD_LOG_DEBUG
#define FAN_LEVEL "fan_level"
#define TASK_PRI ESP_TASK_MAIN_PRIO + 1
#define TASK_SIZE 2048
#define PIN_LEVEL_3 5
#define BLINK_TIME_MS 300
#define LONG_PRESS_TIME_MS 2000
#define SHARK_TIME_MS 100

#define PIN_INPUT 23
#define PIN_LED 22
#define PIN_LEVEL_1 16
#define PIN_LEVEL_2 17

static enum {
    PRESS,
    UP,
}action_t;

typedef struct press_time_t{
    uint32_t time_ms;
    int action;
}press_time_t;

static int g_level = 0;
static xQueueHandle g_queue = NULL;

void fan_led_restore()
{
    if(g_level == 0)
        fan_led_set(0);
    else
        fan_led_set(1);
}

void fan_led_set(int status)
{
    gpio_set_level(PIN_LED, status);
    STD_LOGI("heater len: %d", status);
}

static void fan_stop()
{
    gpio_set_level(PIN_LEVEL_1, 0);
    gpio_set_level(PIN_LEVEL_2, 0);
    gpio_set_level(PIN_LEVEL_3, 0);
}

static void led_blink(int times)
{
    
    for(int n = 0; n < times; n++)
    {
        fan_led_set(0);
        vTaskDelay(BLINK_TIME_MS/portTICK_PERIOD_MS);
        fan_led_set(1);
        vTaskDelay(BLINK_TIME_MS/portTICK_PERIOD_MS);
    }

    if(times == 0)
        fan_led_set(0);
}

static void fan_level_1()
{
    gpio_set_level(PIN_LEVEL_1, 1);
    gpio_set_level(PIN_LEVEL_2, 0);
    gpio_set_level(PIN_LEVEL_3, 0);
}

static void fan_level_2()
{
    gpio_set_level(PIN_LEVEL_1, 0);
    gpio_set_level(PIN_LEVEL_2, 1);
    gpio_set_level(PIN_LEVEL_3, 0);
}

static void fan_level_3()
{
    gpio_set_level(PIN_LEVEL_1, 0);
    gpio_set_level(PIN_LEVEL_2, 0);
    gpio_set_level(PIN_LEVEL_3, 1);
}

static void fan_switch(int value)
{
    switch(value)
    {
        case 0:
            fan_stop();
            break;

        case 1:
            fan_level_1();
            break;

        case 2:
            fan_level_2();
            break;

        case 3:
            fan_level_3();
            break;
        
        default:
            STD_END("not here");
            break;
    }
    STD_LOGI("set fan to level[%d]", value);
    led_blink(value);
}

static void fan_on_around()
{
    STD_LOGI("on around");
    switch(g_level)
    {
        case 0:
            STD_LOGI("off, no around");
            return;
            break;

        case 1:
            STD_LOGI("1--->2");
            g_level = 2;
            break;

        case 2:
            STD_LOGI("2---3>");
            g_level = 3;
            break;

        case 3:
            STD_LOGI("3---1>");
            g_level = 1;
            break;
    }
    std_nvs_save(FAN_LEVEL, &g_level, sizeof(int));
    fan_switch(g_level);  
}

static void fan_turn_around()
{
    STD_LOGI("turn around");
    if(g_level == 0)
    {
        std_nvs_load(FAN_LEVEL, &g_level, sizeof(int));
        fan_switch(g_level);
    }
    else
    {
        g_level = 0;
        fan_switch(g_level);
    }    
}

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    static press_time_t press_time;
    if(gpio_get_level(PIN_INPUT) == 0)
        press_time.action = PRESS;
    else
        press_time.action = UP;
    press_time.time_ms = esp_log_timestamp();
    xQueueSendFromISR(g_queue, &press_time, &xHigherPriorityTaskWoken);
}

static int trig_init()
{
    gpio_config_t io_conf;
	//interrupt of rising edge
    io_conf.intr_type = GPIO_PIN_INTR_ANYEDGE;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = 1ULL<<PIN_INPUT;
    //set as input mode    
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    io_conf.pull_down_en = 0;
    gpio_config(&io_conf);
    gpio_set_intr_type(PIN_INPUT, GPIO_INTR_ANYEDGE);
	    //install gpio isr service
    gpio_install_isr_service(0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(PIN_INPUT, gpio_isr_handler, NULL);
	return 0;
}

static void fan_task(void *arg)
{
    BaseType_t res;
    static press_time_t press_time;
    static uint32_t last_time;
    for(;;)
    {
        res = xQueueReceive(g_queue, &press_time, portMAX_DELAY);
        STD_ASSERT(res == pdTRUE);
        if(press_time.action != PRESS)
        {
            STD_LOGV("not PRESS ,drop");
            continue;
        }
            
        last_time = press_time.time_ms;
        res = xQueueReceive(g_queue, &press_time, LONG_PRESS_TIME_MS/portTICK_PERIOD_MS);
        if(res == pdTRUE)
        {
            if((press_time.time_ms - last_time) < SHARK_TIME_MS)
            {
                STD_LOGV("shaking ,drop");
                continue;
            }
                
            fan_turn_around();
        }
        else
        {
            fan_on_around();
        }
    }
}

static void gpio_pin_init()
{
    gpio_set_direction(PIN_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_LEVEL_1, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_LEVEL_2, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_LEVEL_3, GPIO_MODE_OUTPUT);
}


void stop_msg_cb(void *arg)
{

    fan_on_around();
}

void fan_init()
{
    g_queue = xQueueCreate(10, sizeof(press_time_t));
    xTaskCreate(fan_task, "fan task", TASK_SIZE, NULL, TASK_PRI, NULL);
    if(!std_nvs_is_exist(FAN_LEVEL))
    {
        int value = 1;
        std_nvs_save(FAN_LEVEL, &value, sizeof(int));
        led_blink(value);
        STD_LOGI("fan level nvs init");
    }  
    gpio_pin_init();
    fan_switch(g_level);
    led_blink(0);
    trig_init();
    STD_LOGI("fan init");
}

