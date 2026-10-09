#ifndef RS485_H
#define RS485_H
#include <stdbool.h>
#include <stddef.h>
#include <zephyr/kernel.h>

/* UART4 (PA0 TX / PA1 RX) + direction pin PC3, 9600 8N1 (from devicetree).
 * Half duplex: transmit with DE/RE high, otherwise receive ASCII lines. */
#define RS485_LINE_MAX 64   /* longest received line, incl. the NUL */

bool rs485_init(void);
/* true once rs485_init() has succeeded */
bool rs485_is_ready(void);
/* Blocking transmit of a NUL-terminated string; DIR is released afterwards */
void rs485_send(const char *s);
/* Next received line (without CR/LF), NUL-terminated. 0 or -EAGAIN. A line
 * ends on CR/LF or after a short pause (50 ms) in the incoming bytes. Lines
 * longer than RS485_LINE_MAX - 1 are dropped. */
int  rs485_read_line(char out[RS485_LINE_MAX], k_timeout_t timeout);

#endif /* RS485_H */
