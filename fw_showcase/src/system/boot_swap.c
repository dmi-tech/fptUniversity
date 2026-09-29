/* boot_swap.c — image confirmation and the NRST swap gesture (see boot_swap.h) */
#include "boot_swap.h"
#include "status_led.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/storage/flash_map.h>
#include <string.h>

#ifdef CONFIG_MCUBOOT_IMG_MANAGER
#include <zephyr/dfu/mcuboot.h>
#endif

#define MARKER_WINDOW      0xAA55AA55UL   /* boot in progress, reset = swap */
#define MARKER_SWAP        0xDEADBEEFUL   /* swap requested */
#define MARKER_ERASE_SIZE  8192           /* one flash page */
#define MARKER_WRITE_SIZE  16             /* one flash write block */
#define BOOT_SWAP_WINDOW_S 3

static int marker_read(uint32_t *value)
{
    const struct flash_area *fa;
    uint8_t buf[MARKER_WRITE_SIZE];
    int r = flash_area_open(PARTITION_ID(marker_partition), &fa);

    if (r) return r;
    r = flash_area_read(fa, 0, buf, sizeof(buf));
    if (!r) memcpy(value, buf, sizeof(*value));
    flash_area_close(fa);
    return r;
}

static int marker_write(uint32_t value)
{
    const struct flash_area *fa;
    uint8_t buf[MARKER_WRITE_SIZE];
    int r = flash_area_open(PARTITION_ID(marker_partition), &fa);

    if (r) return r;
    r = flash_area_erase(fa, 0, MARKER_ERASE_SIZE);
    if (r) {
        flash_area_close(fa);
        return r;
    }
    memset(buf, 0xFF, sizeof(buf));
    memcpy(buf, &value, sizeof(value));
    r = flash_area_write(fa, 0, buf, sizeof(buf));
    flash_area_close(fa);
    return r;
}

static int marker_erase(void)
{
    const struct flash_area *fa;
    int r = flash_area_open(PARTITION_ID(marker_partition), &fa);

    if (r) return r;
    r = flash_area_erase(fa, 0, MARKER_ERASE_SIZE);
    flash_area_close(fa);
    return r;
}

static void request_swap(void)
{
#ifdef CONFIG_MCUBOOT_IMG_MANAGER
    if (boot_request_upgrade(BOOT_UPGRADE_TEST) == 0) {
        k_msleep(200);
        sys_reboot(SYS_REBOOT_COLD);
    }
#endif
}

void boot_swap_confirm_image(void)
{
#ifdef CONFIG_MCUBOOT_IMG_MANAGER
    if (!boot_is_img_confirmed()) boot_write_img_confirmed();
#endif
}

void boot_swap_check(void)
{
    uint32_t m = 0;

    marker_read(&m);
    if (m == MARKER_SWAP) {
        marker_erase();
        request_swap();
        return;
    }
    if (m == MARKER_WINDOW) {
        marker_write(MARKER_SWAP);
        k_msleep(200);
        sys_reboot(SYS_REBOOT_COLD);
        return;
    }

    printk("\n| NRST %ds |\n", BOOT_SWAP_WINDOW_S);
    marker_write(MARKER_WINDOW);
    for (int i = 0; i < BOOT_SWAP_WINDOW_S * 10; i++) {
        status_led_toggle();
        k_msleep(100);
    }
    marker_erase();
    status_led_set(false);
}
