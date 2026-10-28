#include <zephyr/init.h>

static int sam3u4e_ek_init(void)
{
	return 0;
}

SYS_INIT(sam3u4e_ek_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
