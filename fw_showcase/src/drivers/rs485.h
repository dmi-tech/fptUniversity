#ifndef RS485_H
#define RS485_H
#include <stdbool.h>

/* UART4 (PA0 TX / PA1 RX) + direction pin PC3, 9600 8N1 (from devicetree) */
bool rs485_init(void);
/* true once rs485_init() has succeeded */
bool rs485_is_ready(void);
/* Blocking transmit of a NUL-terminated string; DIR is released afterwards */
void rs485_send(const char *s);

#endif /* RS485_H */
