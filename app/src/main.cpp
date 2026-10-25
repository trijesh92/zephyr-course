#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define GREEN_LED_NODE DT_NODELABEL(green_led)
#define RED_LED_NODE DT_NODELABEL(red_led)
#define LED_STACK_SIZE 1024

// stacks for led task threads

K_THREAD_STACK_DEFINE(green_led_stack, LED_STACK_SIZE);
K_THREAD_STACK_DEFINE(red_led_stack, LED_STACK_SIZE);
struct k_thread green_led_thread;
struct k_thread red_led_thread;

static void green_led_blink(void *p1, void *p2, void *p3)
{
    const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(GREEN_LED_NODE, gpios);
    if (!device_is_ready(led.port)) {
        LOG_ERR("Green LED device not ready");
        return;
    }
    if (gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE) < 0) {
        LOG_ERR("Failed to configure Green LED GPIO pin");
        return;
    }
    // blink green led every 500ms
    bool led_on = false;
    while (1) {
        gpio_pin_toggle_dt(&led);
        led_on = !led_on;
        LOG_INF("Green LED state changed: %s", led_on ? "ON" : "OFF");
        k_sleep(K_MSEC(500));
    }
}

static void red_led_blink(void *p1, void *p2, void *p3)
{
    const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(RED_LED_NODE, gpios);
    if (!device_is_ready(led.port)) {
        LOG_ERR("Red LED device not ready");
        return;
    }
    if (gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE) < 0) {
        LOG_ERR("Failed to configure Red LED GPIO pin");
        return;
    }
    // blink red led every 1000ms
    bool led_on = false;
    while (1) {
        gpio_pin_toggle_dt(&led);
        led_on = !led_on;
        LOG_INF("Red LED state changed: %s", led_on ? "ON" : "OFF");
        k_sleep(K_MSEC(1000));
    }
}

int main(void)
{
    LOG_INF("Hello World! %s", CONFIG_BOARD);
    // create threads for led_task

    k_thread_create(&green_led_thread, green_led_stack, K_THREAD_STACK_SIZEOF(green_led_stack),
                    (k_thread_entry_t)green_led_blink, NULL, NULL, NULL,
                    K_PRIO_PREEMPT(1), 0, K_NO_WAIT);
    k_thread_create(&red_led_thread, red_led_stack, K_THREAD_STACK_SIZEOF(red_led_stack),
                    (k_thread_entry_t)red_led_blink, NULL, NULL, NULL,
                    K_PRIO_PREEMPT(1), 0, K_NO_WAIT);
    return 0;
}
