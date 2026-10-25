#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define LED0_NODE DT_NODELABEL(green_led)
#define LED_STACK_SIZE 1024

// define stack for led_task thread

K_THREAD_STACK_DEFINE(led_stack, LED_STACK_SIZE);
struct k_thread led_thread;

void led_task(void *p1, void *p2, void *p3)
{
    const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
    if (!device_is_ready(led.port)) {
        LOG_ERR("LED device not ready");
        return;
    }
    // blink led in look every 1 second
    while (1) {
        gpio_pin_toggle_dt(&led);
        k_sleep(K_MSEC(1000));
    }
}

int main(void)
{
    LOG_INF("Hello World! %s", CONFIG_BOARD);
    // create a thread for led_task

    k_thread_create(&led_thread, led_stack, K_THREAD_STACK_SIZEOF(led_stack),
                    (k_thread_entry_t)led_task, NULL, NULL, NULL,
                    K_PRIO_PREEMPT(1), 0, K_NO_WAIT);
    return 0;
}
