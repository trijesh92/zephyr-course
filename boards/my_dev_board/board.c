#include <zephyr/init.h>
#include <zephyr/kernel.h>

static int my_dev_board_init(void)
{
    printf("Board Initialized\n");
    return 0;
}

SYS_INIT(my_dev_board_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);