/* rs485_cmd.c — RS485 command lines from the gateway.
 *
 *   ON  / OFF   (case-insensitive, surrounding blanks ignored; with or
 *               without CR/LF after it)
 *               switch the motor and answer "ACK MOTOR ON" / "ACK MOTOR OFF"
 * Other lines are logged and ignored.
 */
#include "rs485.h"
#include "motor.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <ctype.h>
#include <string.h>

#define RS485_TURNAROUND_MS 2   /* let the gateway switch back to receive */

static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t')) s[--n] = '\0';
    return s;
}

static bool is_word(const char *s, const char *word)
{
    while (*s && *word) {
        if (toupper((unsigned char)*s++) != *word++) return false;
    }
    return *s == '\0' && *word == '\0';
}

static void rs485_cmd_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);
    char line[RS485_LINE_MAX];

    while (1) {
        if (rs485_read_line(line, K_FOREVER) != 0) continue;
        char *cmd = trim(line);
        printk("[RS485] RX '%s'\n", cmd);

        bool on;
        if (is_word(cmd, "ON"))       on = true;
        else if (is_word(cmd, "OFF")) on = false;
        else continue;

        motor_set(on, MOTOR_SRC_RS485);
        k_msleep(RS485_TURNAROUND_MS);
        rs485_send(on ? "ACK MOTOR ON\r\n" : "ACK MOTOR OFF\r\n");
    }
}

K_THREAD_DEFINE(rs485_cmd_tid, 1536, rs485_cmd_thread, NULL, NULL, NULL, 6, 0, 0);
