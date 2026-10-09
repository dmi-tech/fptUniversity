/* web_server.c — Lightweight HTTP server for fw_showcase 3.1.0
 *
 * Routes:
 *   GET  /            → redirect to /login
 *   GET  /login       → login page HTML
 *   POST /login       → check pw, set cookie, redirect /config
 *   GET  /config      → device config page (auth required)
 *   POST /save        → save config to flash and reboot (auth required)
 *   GET  /logout      → clear session, redirect /login
 *   GET  /status      → JSON with the latest sensor data + alarm/motor
 *
 * Live tools on /config (auth required, JSON {"ok":0|1,"msg":"..."}, no reboot):
 *   GET  /api/live      → sensor/LCD/motor/MQTT state + subscribed messages
 *   POST /api/motor     → on=1|0
 *   POST /api/rs485     → text=...          (sent as one line + CRLF)
 *   POST /api/mqtt/sub  → topic=...         (empty = unsubscribe)
 *   POST /api/mqtt/pub  → topic=...&msg=...
 *
 * Auth: single session token (uptime-based), stored in cookie "sid".
 * Only ONE session active at a time. Token cleared on reboot.
 *
 * Socket strategy: 1 server socket + 1 client socket = 2 total.
 * Each client handled synchronously, closed immediately after response.
 * This avoids the socket exhaustion issue from v2.5.1.
 */
#include "web_pages.h"
#include "http.h"
#include "app_state.h"
#include "app_config.h"
#include "alarm.h"
#include "motor.h"
#include "lcd_pcf8574.h"
#include "mqtt_app.h"
#include "rs485.h"
#include "version.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/net/socket.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ── Session ─────────────────────────────────────────────────── */
static char  s_token[16]   = "";
static bool  s_active      = false;
static int   s_fail_delay  = 0;   /* anti-brute-force: ms to sleep */

static void gen_token(void)
{
    snprintf(s_token, sizeof(s_token), "%08X", (uint32_t)k_uptime_get());
    s_active = true;
}

static bool check_token(const char *req)
{
    if (!s_active || s_token[0] == '\0') return false;
    char needle[32];
    snprintf(needle, sizeof(needle), "sid=%s", s_token);
    return strstr(req, needle) != NULL;
}

/* ── Send login page ──────────────────────────────────────────── */
static void serve_login(int fd, bool err)
{
    static char buf[sizeof(HTML_LOGIN) + 64];
    int n = snprintf(buf, sizeof(buf), HTML_LOGIN,
                     err ? "Wrong password, please try again." : "");
    http_respond(fd, 200, "text/html", buf, n, NULL);
}

/* ── Streamed response (no Content-Length, "Connection: close") ── */
static struct {
    int    fd;
    size_t n;
    char   buf[1024];
} s_out;

static void out_begin(int fd, const char *ctype)
{
    char hdr[160];
    int n = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\nContent-Type: %s\r\n"
        "Cache-Control: no-store\r\nConnection: close\r\n\r\n", ctype);
    s_out.fd = fd;
    s_out.n  = 0;
    http_send_all(fd, hdr, n);
}

static void out_flush(void)
{
    if (s_out.n) http_send_all(s_out.fd, s_out.buf, s_out.n);
    s_out.n = 0;
}

static void out_ch(char c)
{
    if (s_out.n == sizeof(s_out.buf)) out_flush();
    s_out.buf[s_out.n++] = c;
}

static void out_str(const char *s)
{
    while (*s) out_ch(*s++);
}

static void out_fmt(const char *fmt, ...)
{
    char tmp[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n >= (int)sizeof(tmp)) printk("[WEB] output part truncated\n");
    out_str(tmp);
}

/* Text escaped for an HTML attribute value or element */
static void out_html(const char *s)
{
    for (; *s; s++) {
        switch (*s) {
        case '&':  out_str("&amp;");  break;
        case '<':  out_str("&lt;");   break;
        case '>':  out_str("&gt;");   break;
        case '"':  out_str("&quot;"); break;
        case '\'': out_str("&#39;");  break;
        default:   out_ch(*s);
        }
    }
}

/* Quoted JSON string */
static void out_json(const char *s)
{
    out_ch('"');
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { out_ch('\\'); out_ch((char)c); }
        else if (c < 0x20 || c == 0x7F) out_fmt("\\u%04x", c);
        else out_ch((char)c);
    }
    out_ch('"');
}

static void out_end(void)
{
    out_flush();
}

/* ── Config page ──────────────────────────────────────────────── */
/* Labelled read-only value / text input, one <td> pair each */
static void td_ro(const char *label, const char *id, const char *val)
{
    out_fmt("<td class=\"lb\">%s</td><td><span class=\"ro\"%s%s%s>", label,
            id ? " id=\"" : "", id ? id : "", id ? "\"" : "");
    out_html(val);
    out_str("</span></td>");
}

static void td_input(const char *label, const char *type, const char *name,
                     const char *val, int span)
{
    out_fmt("<td class=\"lb\">%s</td><td colspan=\"%d\"><input form=\"cfg\" type=\"%s\" name=\"%s\" value=\"",
            label, span, type, name);
    out_html(val);
    out_str("\"></td>");
}

static void serve_config(int fd)
{
    char port[8];

    out_begin(fd, "text/html; charset=UTF-8");
    out_str("<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
            "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
            "<title>fw_showcase Configuration</title>");
    out_str(CFG_CSS);
    out_str("</head><body>");

    /* Header */
    out_str("<div class=\"hdr\">"
            "<div><div class=\"hdr-logo\">DASHBOARD</div>"
            "<div class=\"hdr-sub\">Device configuration</div></div>"
            "<div style=\"display:flex;align-items:center;gap:14px\">"
            "<div class=\"hdr-ip\">IP: ");
    out_html(g_device_ip);
    out_str("</div><a href=\"/logout\"><button class=\"lbtn\">Logout</button></a>"
            "</div></div>");

    /* Only the MQTT settings and the password belong to this form (form="cfg");
     * the live tools below post to /api/... and never reboot the device. */
    out_str("<div class=\"content\"><form id=\"cfg\" method=\"POST\" action=\"/save\"></form>");

    /* Device Info — 3 label/value pairs per row */
    out_str("<div class=\"sec\"><div class=\"st\">Device Information</div><div class=\"sb\"><table>"
            "<colgroup><col class=\"l\"><col><col class=\"l\"><col><col class=\"l\"><col></colgroup><tr>");
    td_ro("Device Name", NULL, g_cfg.device_name);
    td_ro("Firmware Version", NULL, FW_VERSION);
    td_ro("Device MAC", NULL, g_device_mac);
    out_str("</tr><tr>");
    td_ro("Current IP", NULL, g_device_ip);
    td_ro("IP Mode", NULL, g_cfg.ip_mode ? "Static" : "DHCP");
    snprintf(port, sizeof(port), "%u", g_cfg.web_port);
    td_ro("Web Port", NULL, port);
    out_str("</tr><tr>");
    td_ro("Alarm", "al", alarm_active() ? alarm_reason_str(alarm_reason()) : "OK");
    td_ro("RS485", NULL, rs485_is_ready() ? "OK" : "OFF");
    out_str("<td></td><td></td></tr></table></div></div>");

    /* MQTT settings + subscribe / publish tools */
    out_str("<div class=\"sec\"><div class=\"st\">MQTT Settings</div><div class=\"sb\"><table>"
            "<colgroup><col class=\"l\"><col><col class=\"l\"><col><col class=\"l\"><col></colgroup><tr>");
    td_input("Broker Host", "text", "mqtt_host", g_cfg.mqtt_host, 3);
    snprintf(port, sizeof(port), "%u", g_cfg.mqtt_port);
    td_input("Port", "text", "mqtt_port", port, 1);
    out_str("</tr><tr>");
    td_input("Client ID", "text", "mqtt_cid", g_cfg.mqtt_client_id, 1);
    td_input("Username", "text", "mqtt_user", g_cfg.mqtt_user, 1);
    td_input("Password", "password", "mqtt_pass", g_cfg.mqtt_pass, 1);
    out_str("</tr><tr><td class=\"lb\">Status</td><td colspan=\"5\"><span class=\"ro\" id=\"ms\">");
    out_html(mqtt_app_status());
    out_str("</span></td></tr>"
            "<tr><td colspan=\"6\" class=\"sub\">Subscribe</td></tr>"
            "<tr><td class=\"lb\">Topic</td>"
            "<td colspan=\"3\"><input type=\"text\" id=\"st\" maxlength=\"63\" placeholder=\"e.g. demo/cmd or demo/#\"></td>"
            "<td><button type=\"button\" class=\"btn\" onclick=\"sub()\">Subscribe</button></td>"
            "<td><span class=\"fb\" id=\"ss\"></span></td></tr>"
            "<tr id=\"sbr\" style=\"display:none\"><td></td><td colspan=\"5\">"
            "<textarea id=\"sm\" rows=\"7\" readonly></textarea></td></tr>"
            "<tr><td colspan=\"6\" class=\"sub\">Publish</td></tr>"
            "<tr><td class=\"lb\">Topic</td>"
            "<td colspan=\"3\"><input type=\"text\" id=\"pt\" maxlength=\"63\" placeholder=\"full topic, sent as typed\"></td>"
            "<td></td><td></td></tr>"
            "<tr><td class=\"lb\">Message</td>"
            "<td colspan=\"3\"><input type=\"text\" id=\"pm\" maxlength=\"128\" placeholder=\"payload text\"></td>"
            "<td><button type=\"button\" class=\"btn\" onclick=\"pub()\">Publish</button></td>"
            "<td><span class=\"fb\" id=\"pf\"></span></td></tr>"
            "</table></div></div>");

    /* Devices — 4 label/value pairs per row; first column as wide as the
     * other sections so the value boxes start on the same line */
    char tv[20] = "--", hv[20] = "--";
    bool on = motor_is_on();
    enum motor_src by = motor_last_src();
    if (g_web_have_data) {
        snprintf(tv, sizeof(tv), "%d °C", (int)(g_web_temp_mc / 1000));
        snprintf(hv, sizeof(hv), "%d %%", (int)(g_web_humi_mp / 1000));
    }
    out_str("<div class=\"sec\"><div class=\"st\">Devices</div><div class=\"sb\"><table>"
            "<colgroup><col class=\"l\"><col><col class=\"s\"><col><col class=\"s\"><col><col class=\"s\"><col></colgroup><tr>");
    td_ro("LCD", NULL, "20x4");
    td_ro("Status", "lv", lcd_is_ready() ? "OK" : "ERR");
    out_fmt("<td class=\"lb\">Motor</td><td colspan=\"3\"><span class=\"sw\">"
            "<button type=\"button\" id=\"m1\" class=\"%s\" onclick=\"motor(1)\">ON</button>"
            "<button type=\"button\" id=\"m0\" class=\"%s\" onclick=\"motor(0)\">OFF</button></span>"
            "<span class=\"by\" id=\"mby\">",
            on ? "on" : "", on ? "" : "off");
    if (by != MOTOR_SRC_NONE) out_fmt("(by %s)", motor_src_str(by));
    out_str("</span></td></tr><tr>");
    td_ro("Sensor", NULL, "DHT11");
    td_ro("Status", "sv", g_web_sensor_ok ? "OK" : "ERR");
    td_ro("Temperature", "tv", tv);
    td_ro("Humidity", "hv", hv);
    out_str("</tr><tr><td class=\"lb\">RS485</td>"
            "<td colspan=\"5\"><input type=\"text\" id=\"rt\" maxlength=\"60\" placeholder=\"text line sent over RS485 (CRLF added)\"></td>"
            "<td><button type=\"button\" class=\"btn\" onclick=\"rs()\">Send</button></td>"
            "<td><span class=\"fb\" id=\"rf\"></span></td></tr>"
            "</table></div></div>");

    /* Password */
    out_str("<div class=\"sec\"><div class=\"st\">Modify Web Login Password</div><div class=\"sb\"><table>"
            "<colgroup><col class=\"l\"><col><col class=\"l\"><col><col class=\"l\"><col></colgroup><tr>"
            "<td class=\"lb\">New Password</td>"
            "<td><input form=\"cfg\" type=\"password\" name=\"new_pw\" placeholder=\"leave blank to keep\"></td>"
            "<td class=\"lb\">Confirm Password</td>"
            "<td><input form=\"cfg\" type=\"password\" name=\"cfm_pw\" placeholder=\"re-enter new password\"></td>"
            "<td></td><td></td></tr>"
            "</table></div></div>");

    out_str("<div class=\"sv\"><button form=\"cfg\" class=\"sbtn\" type=\"submit\">Save &amp; Apply</button></div>");
    out_str(CFG_JS);
    out_str("</div></body></html>");
    out_end();
}

/* ── Handle POST /login ──────────────────────────────────────── */
static void handle_login(int fd, const char *hdr, int hlen)
{
    char body[256] = {0};
    http_read_body(fd, hdr, hlen, body, sizeof(body));

    char pw[64] = {0};
    http_form_field(body, "pw", pw, sizeof(pw));

    if (s_fail_delay > 0) {
        k_msleep(s_fail_delay);
    }

    if (strncmp(pw, g_cfg.web_password, sizeof(g_cfg.web_password)) == 0) {
        gen_token();
        s_fail_delay = 0;
        char cookie[80];
        snprintf(cookie, sizeof(cookie),
                 "Set-Cookie: sid=%s; Path=/; HttpOnly\r\n", s_token);
        /* Redirect to config page */
        char redirect[200];
        int n = snprintf(redirect, sizeof(redirect),
            "HTTP/1.1 302 Found\r\nLocation: /config\r\n%sContent-Length: 0\r\nConnection: close\r\n\r\n",
            cookie);
        http_send_all(fd, redirect, n);
        printk("[WEB] Login OK — session=%s\n", s_token);
    } else {
        s_fail_delay = (s_fail_delay < 3000) ? s_fail_delay + 500 : 3000;
        printk("[WEB] Login FAIL\n");
        /* Redirect back to login with error flag */
        http_redirect(fd, "/login?err=1");
    }
}

/* ── Handle POST /save ───────────────────────────────────────── */
/* POST bodies (web thread only) */
static char s_body[2048];

static void handle_save(int fd, const char *hdr, int hlen)
{
    char *body = s_body;
    char passbuf[280];
    body[0] = '\0';
    http_read_body(fd, hdr, hlen, s_body, sizeof(s_body));

    char tmp[128];
    app_config_t nc;
    memcpy(&nc, &g_cfg, sizeof(nc));

    /* IP mode */
    if (http_form_field(body, "ip_mode", tmp, sizeof(tmp)))
        nc.ip_mode = (uint8_t)atoi(tmp);

    /* IPs */
    if (http_form_field(body, "static_ip", tmp, sizeof(tmp)))
        str_to_ip4(tmp, nc.static_ip);
    if (http_form_field(body, "subnet", tmp, sizeof(tmp)))
        str_to_ip4(tmp, nc.subnet);
    if (http_form_field(body, "gateway", tmp, sizeof(tmp)))
        str_to_ip4(tmp, nc.gateway);
    if (http_form_field(body, "dns", tmp, sizeof(tmp)))
        str_to_ip4(tmp, nc.dns);

    /* MQTT */
    if (http_form_field(body, "mqtt_host", tmp, sizeof(tmp)))
        strncpy(nc.mqtt_host, tmp, sizeof(nc.mqtt_host) - 1);
    if (http_form_field(body, "mqtt_port", tmp, sizeof(tmp)))
        nc.mqtt_port = (uint16_t)atoi(tmp);
    if (http_form_field(body, "mqtt_cid", tmp, sizeof(tmp)))
        strncpy(nc.mqtt_client_id, tmp, sizeof(nc.mqtt_client_id) - 1);
    if (http_form_field(body, "mqtt_user", tmp, sizeof(tmp)))
        strncpy(nc.mqtt_user, tmp, sizeof(nc.mqtt_user) - 1);
    if (http_form_field(body, "mqtt_pass", passbuf, sizeof(passbuf)))
        strncpy(nc.mqtt_pass, passbuf, sizeof(nc.mqtt_pass) - 1);

    /* Password change (only if new_pw == cfm_pw && non-empty) */
    char new_pw[64] = {0}, cfm_pw[64] = {0};
    http_form_field(body, "new_pw", new_pw, sizeof(new_pw));
    http_form_field(body, "cfm_pw", cfm_pw, sizeof(cfm_pw));
    if (new_pw[0] && strcmp(new_pw, cfm_pw) == 0)
        strncpy(nc.web_password, new_pw, sizeof(nc.web_password) - 1);

    /* Save */
    int r = config_save(&nc);
    if (r == 0) {
        memcpy(&g_cfg, &nc, sizeof(g_cfg));
        printk("[WEB] Config saved OK\n");
    } else {
        printk("[WEB] Config save ERR: %d\n", r);
    }

    http_respond(fd, 200, "text/html",
                 HTML_SAVED, sizeof(HTML_SAVED) - 1, NULL);

    /* Reboot after a short delay */
    k_msleep(1500);
    sys_reboot(SYS_REBOOT_COLD);
}

/* ── Handle GET /status (JSON) ───────────────────────────────── */
static void handle_status(int fd)
{
    char buf[288];
    int n = snprintf(buf, sizeof(buf),
        "{\"temp\":%d,\"humi\":%d,\"sensor_ok\":%d,\"cnt\":%u,\"ip\":\"%s\","
        "\"alarm\":%d,\"reason\":\"%s\",\"motor\":%d,\"motor_by\":\"%s\"}",
        (int)(g_web_temp_mc / 1000), (int)(g_web_humi_mp / 1000),
        g_web_sensor_ok ? 1 : 0, g_web_cnt, g_device_ip,
        alarm_active() ? 1 : 0, alarm_reason_str(alarm_reason()),
        motor_is_on() ? 1 : 0, motor_src_str(motor_last_src()));
    http_respond(fd, 200, "application/json", buf, n, NULL);
}

/* ── Live tools (/api/...) ───────────────────────────────────── */
static void api_reply(int fd, bool ok, const char *msg)
{
    out_begin(fd, "application/json");
    out_fmt("{\"ok\":%d,\"msg\":", ok ? 1 : 0);
    out_json(msg);
    out_str("}");
    out_end();
}

static void api_live(int fd)
{
    static struct mqtt_sub_view v;
    char topic[MQTT_TOPIC_MAX];

    mqtt_app_sub_view(&v);
    mqtt_app_status_topic(topic, sizeof(topic));

    out_begin(fd, "application/json");
    out_fmt("{\"temp\":%d,\"humi\":%d,\"have\":%d,\"sensor_ok\":%d,\"lcd_ok\":%d,"
            "\"alarm\":%d,\"reason\":\"%s\",\"motor\":%d,\"motor_by\":\"%s\",\"mqtt\":",
            (int)(g_web_temp_mc / 1000), (int)(g_web_humi_mp / 1000),
            g_web_have_data ? 1 : 0, g_web_sensor_ok ? 1 : 0, lcd_is_ready() ? 1 : 0,
            alarm_active() ? 1 : 0, alarm_reason_str(alarm_reason()),
            motor_is_on() ? 1 : 0, motor_src_str(motor_last_src()));
    out_json(mqtt_app_status());
    out_str(",\"status_topic\":");
    out_json(topic);
    out_str(",\"sub\":");
    out_json(v.topic);
    out_str(",\"sub_state\":");
    out_json(v.state);
    out_str(",\"msgs\":[");
    for (int i = 0; i < v.count; i++) {
        out_fmt("%s{\"t\":%u,\"cut\":%d,\"topic\":", i ? "," : "",
                v.msgs[i].uptime_s, v.msgs[i].truncated ? 1 : 0);
        out_json(v.msgs[i].topic);
        out_str(",\"data\":");
        out_json(v.msgs[i].data);
        out_str("}");
    }
    out_str("]}");
    out_end();
}

static void api_motor(int fd, const char *body)
{
    char v[4];
    if (!http_form_field(body, "on", v, sizeof(v)) || (strcmp(v, "0") && strcmp(v, "1"))) {
        api_reply(fd, false, "on must be 0 or 1");
        return;
    }
    motor_set(v[0] == '1', MOTOR_SRC_WEB);
    api_reply(fd, true, v[0] == '1' ? "motor ON" : "motor OFF");
}

static void api_rs485(int fd, const char *body)
{
    char text[RS485_LINE_MAX + 2];   /* room for the CRLF */

    http_form_field(body, "text", text, RS485_LINE_MAX - 3);
    text[strcspn(text, "\r\n")] = '\0';            /* one line only */
    if (!text[0])           { api_reply(fd, false, "enter a text"); return; }
    if (!rs485_is_ready())  { api_reply(fd, false, "RS485 not ready"); return; }
    strcat(text, "\r\n");
    rs485_send(text);
    api_reply(fd, true, "sent");
}

static void api_mqtt_sub(int fd, const char *body)
{
    char topic[MQTT_TOPIC_MAX + 1];

    http_form_field(body, "topic", topic, sizeof(topic));
    if (mqtt_app_subscribe(topic) != 0) {
        api_reply(fd, false, "topic too long (max 63)");
        return;
    }
    api_reply(fd, true, topic[0] ? "subscribing" : "unsubscribed");
}

static void api_mqtt_pub(int fd, const char *body)
{
    char topic[MQTT_TOPIC_MAX + 1], msg[MQTT_MSG_MAX + 1], sent[MQTT_TOPIC_MAX + 16];

    http_form_field(body, "topic", topic, sizeof(topic));
    http_form_field(body, "msg", msg, sizeof(msg));
    int rc = mqtt_app_publish(topic, msg, sent, sizeof(sent) - 12);
    if (rc == 0) {
        memmove(sent + 3, sent, strlen(sent) + 1);   /* "-> " before the topic */
        memcpy(sent, "-> ", 3);
    }
    api_reply(fd, rc == 0,
              rc == 0          ? sent :
              rc == -ENOTCONN  ? "MQTT not connected" :
              rc == -EBUSY     ? "busy, try again" :
              rc == -EMSGSIZE  ? "message too long (max 128)" :
                                 "invalid topic (no + or #, max 63)");
}

/* POST /api/... after authentication */
static void handle_api_post(int fd, const char *path, const char *hdr, int hlen)
{
    s_body[0] = '\0';
    http_read_body(fd, hdr, hlen, s_body, sizeof(s_body));

    if      (strcmp(path, "/api/motor") == 0)    api_motor(fd, s_body);
    else if (strcmp(path, "/api/rs485") == 0)    api_rs485(fd, s_body);
    else if (strcmp(path, "/api/mqtt/sub") == 0) api_mqtt_sub(fd, s_body);
    else if (strcmp(path, "/api/mqtt/pub") == 0) api_mqtt_pub(fd, s_body);
    else {
        const char *nf = "Not Found";
        http_respond(fd, 404, "text/plain", nf, strlen(nf), NULL);
    }
}

/* ── Handle one client ───────────────────────────────────────── */
static char s_req[1024];

static void handle_client(int fd)
{
    int n = zsock_recv(fd, s_req, sizeof(s_req) - 1, 0);
    if (n <= 0) return;
    s_req[n] = '\0';

    /* Parse method + path */
    bool is_get  = (strncmp(s_req, "GET ",  4) == 0);
    bool is_post = (strncmp(s_req, "POST ", 5) == 0);
    const char *path_start = s_req + (is_post ? 5 : 4);
    const char *path_end   = strchr(path_start, ' ');
    char path[64] = {0};
    if (path_end) {
        size_t plen = (size_t)(path_end - path_start);
        if (plen >= sizeof(path)) plen = sizeof(path) - 1;
        memcpy(path, path_start, plen);
    }
    /* Strip query string for routing */
    char *q = strchr(path, '?');
    bool has_err = q && strstr(q, "err=1");
    if (q) *q = '\0';

    printk("[WEB] %s %s\n", is_post ? "POST" : "GET", path);

    bool authed = check_token(s_req);

    if (is_get && (strcmp(path, "/") == 0 || strcmp(path, "") == 0)) {
        http_redirect(fd, "/login");
    } else if (is_get && strcmp(path, "/login") == 0) {
        serve_login(fd, has_err);
    } else if (is_post && strcmp(path, "/login") == 0) {
        handle_login(fd, s_req, n);
    } else if (is_get && strcmp(path, "/config") == 0) {
        if (!authed) { http_redirect(fd, "/login"); return; }
        serve_config(fd);
    } else if (is_post && strcmp(path, "/save") == 0) {
        if (!authed) { http_redirect(fd, "/login"); return; }
        handle_save(fd, s_req, n);
    } else if (is_get && strcmp(path, "/logout") == 0) {
        s_active = false; s_token[0] = '\0';
        http_redirect(fd, "/login");
    } else if (is_get && strcmp(path, "/status") == 0) {
        handle_status(fd);
    } else if (strncmp(path, "/api/", 5) == 0) {
        if (!authed) {
            const char *body = "{\"ok\":0,\"msg\":\"login required\"}";
            http_respond(fd, 401, "application/json", body, strlen(body), NULL);
        } else if (is_get && strcmp(path, "/api/live") == 0) {
            api_live(fd);
        } else if (is_post) {
            handle_api_post(fd, path, s_req, n);
        } else {
            const char *body = "Not Found";
            http_respond(fd, 404, "text/plain", body, strlen(body), NULL);
        }
    } else {
        const char *body = "Not Found";
        http_respond(fd, 404, "text/plain", body, strlen(body), NULL);
    }
}

/* ── Web server thread ───────────────────────────────────────── */
static void web_server_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    /* Wait for network to come up (max 30s) */
    for (int i = 0; i < 60; i++) {
        if (app_state_has_ip()) break;
        k_msleep(500);
    }
    printk("[WEB] Starting HTTP server on port %d  IP=%s\n",
           g_cfg.web_port, g_device_ip);

    int srv = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (srv < 0) { printk("[WEB] socket() fail\n"); return; }

    int opt = 1;
    zsock_setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(g_cfg.web_port),
        .sin_addr.s_addr = INADDR_ANY,
    };
    if (zsock_bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printk("[WEB] bind() fail\n"); zsock_close(srv); return;
    }
    if (zsock_listen(srv, 1) < 0) {
        printk("[WEB] listen() fail\n"); zsock_close(srv); return;
    }

    printk("[WEB] Ready — http://%s/\n", g_device_ip);

    while (1) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        int cli = zsock_accept(srv, (struct sockaddr *)&cli_addr, &cli_len);
        if (cli < 0) {
            k_msleep(10);
            continue;
        }
        /* Set 2-second receive timeout */
        struct zsock_timeval tv = {.tv_sec = 2, .tv_usec = 0};
        zsock_setsockopt(cli, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        handle_client(cli);
        zsock_close(cli);   /* always close immediately */
    }
}

/* Started 2 s after boot; waits for an IP address itself */
K_THREAD_DEFINE(web_tid, 6144, web_server_thread, NULL, NULL, NULL, 5, 0, 2000);
