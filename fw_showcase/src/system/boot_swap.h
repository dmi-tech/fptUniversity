#ifndef BOOT_SWAP_H
#define BOOT_SWAP_H

/* Mark the running MCUboot image as confirmed so it is not reverted */
void boot_swap_confirm_image(void);

/* NRST gesture to switch to the image in the secondary slot.
 *
 * On every boot the LED blinks fast for BOOT_SWAP_WINDOW_S seconds while a
 * "window" marker is kept in the swapmark partition:
 *   - reset (NRST) inside the window -> the next boot sees the window marker,
 *     writes the "swap" marker and reboots;
 *   - that boot sees the swap marker, erases it and requests an MCUboot test
 *     upgrade, so the secondary slot image is swapped in.
 * Without a reset the marker is erased and the boot continues.
 * Needs status_led_init() first. */
void boot_swap_check(void);

#endif /* BOOT_SWAP_H */
