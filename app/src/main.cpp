#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#define SLEEP_TIME_MS 1000

/* The devicetree node identifier for the "led0" alias. */
#define LED_NODE DT_ALIAS(app_led)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/** @brief Task to control the LED */
void led_task(void*, void*, void*)
{
    bool led_state = true;

    if (!gpio_is_ready_dt(&led)) return;

    if (gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE) < 0) return;

    while (1) {
        if (gpio_pin_toggle_dt(&led) < 0) return;

        led_state = !led_state;
        k_msleep(CONFIG_APP_HEARTBEAT_PERIOD_MS);
    }
}

K_THREAD_DEFINE(led_blink_task, 1024, led_task, NULL, NULL, NULL, K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

int main(void)
{
    LOG_INF("Hello World! %s", CONFIG_BOARD);
    return 0;
}
