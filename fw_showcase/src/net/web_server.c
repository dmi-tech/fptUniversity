/* web_server.c — built-in HTTP server (port g_cfg.web_port, default 80)
 *
 * Routes:
 *   GET  /            → redirect to /login
 *   GET  /login       → login page HTML
 *   POST /login       → check pw, set cookie, redirect /config
 *   GET  /config      → device config page (auth required)
 *   POST /save        → save config to flash and reboot (auth required)
 *   GET  /logout      → clear session, redirect /login
 *   GET  /status      → JSON with the last reported sensor data + alarm/motor
 *
 * Auth: single session token (uptime-based), stored in cookie "sid".
 * Only ONE session active at a time. Token cleared on reboot.
 *
 * Socket strategy: 1 server socket + 1 client socket = 2 total.
 * Each client handled synchronously, closed immediately after response.
 */
#include "web_pages.h"
#include "http.h"
#include "app_config.h"
#include "app_state.h"
#include "alarm.h"
#include "mqtt_app.h"
#include "rs485.h"
#include "version.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/net/socket.h>
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

/* ── Send config page (built in static buffer) ─────────────────── */
static char s_page[12288];

static void serve_config(int fd)
{
    /* ── Build page ── */
    int off = 0;
    int rem = (int)sizeof(s_page);

#define AP(...) do { int _n = snprintf(s_page+off, rem, __VA_ARGS__); off += _n; rem -= _n; } while(0)

    AP("<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
       "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
       "<title>fw_showcase Configuration</title>");
    AP("%s", CFG_CSS);
    AP("</head><body>");

    /* Header */
    AP("<div class=\"hdr\">"
       "<div><div class=\"hdr-logo\">fw_showcase</div>"
       "<div class=\"hdr-sub\">Device Configuration Portal</div></div>"
       "<div style=\"display:flex;align-items:center;gap:14px\">"
       "<div class=\"hdr-ip\">IP: %s</div>"
       "<a href=\"/logout\"><button class=\"lbtn\">Logout</button></a>"
       "</div></div>", g_device_ip);

    AP("<div class=\"content\"><form method=\"POST\" action=\"/save\">");

    /* Device Info */
    AP("<div class=\"sec\"><div class=\"st\">Device Information</div><div class=\"sb\"><table>"
       "<tr><td class=\"lb\">Device Name</td><td><span class=\"ro\">%s</span></td>"
       "<td class=\"lb\">Firmware Version</td><td><span class=\"ro\">" FW_VERSION "</span></td>"
       "<td class=\"lb\">Device MAC</td><td><span class=\"ro\">%s</span></td></tr>"
       "<tr><td class=\"lb\">Current IP</td><td><span class=\"ro\">%s</span></td>"
       "<td class=\"lb\">IP Mode</td><td><span class=\"ro\">%s</span></td>"
       "<td class=\"lb\">Web Port</td><td><span class=\"ro\">%d</span></td></tr>"
       "<tr><td class=\"lb\">Alarm</td><td><span class=\"ro\" id=\"al\">%s</span></td>"
       "<td class=\"lb\">Motor</td><td><span class=\"ro\" id=\"mo\">%s</span></td>"
       "<td class=\"lb\">RS485</td><td><span class=\"ro\" id=\"rs\">%s</span></td></tr>"
       "</table></div></div>",
       g_cfg.device_name, g_device_mac, g_device_ip,
       g_cfg.ip_mode ? "Static" : "DHCP", g_cfg.web_port,
       alarm_active() ? alarm_reason_str(alarm_reason()) : "OK",
       motor_is_on() ? "ON" : "OFF",
       rs485_is_ready() ? "OK" : "OFF");

    /* MQTT */
    AP("<div class=\"sec\"><div class=\"st\">MQTT Settings</div><div class=\"sb\"><table>"
       "<tr><td class=\"lb\">Broker Host</td>"
       "<td colspan=\"3\"><input type=\"text\" name=\"mqtt_host\" value=\"%s\" style=\"width:300px\"></td>"
       "<td class=\"lb\">Port</td>"
       "<td><input type=\"text\" name=\"mqtt_port\" value=\"%d\"></td></tr>"
       "<tr><td class=\"lb\">Client ID</td>"
       "<td><input type=\"text\" name=\"mqtt_cid\" value=\"%s\"></td>"
       "<td class=\"lb\">Topic</td>"
       "<td colspan=\"3\"><input type=\"text\" name=\"mqtt_topic\" value=\"%s\" style=\"width:260px\"></td></tr>"
       "<tr><td class=\"lb\">Username</td>"
       "<td><input type=\"text\" name=\"mqtt_user\" value=\"%s\"></td>"
       "<td class=\"lb\">Password</td>"
       "<td colspan=\"3\"><input type=\"password\" name=\"mqtt_pass\" value=\"%s\" style=\"width:300px\"></td></tr>"
       "<tr><td class=\"lb\">Status</td>"
       "<td colspan=\"5\"><span class=\"ro\">%s</span></td></tr>"
       "</table></div></div>",
       g_cfg.mqtt_host, g_cfg.mqtt_port,
       g_cfg.mqtt_client_id, g_cfg.mqtt_topic,
       g_cfg.mqtt_user, g_cfg.mqtt_pass, mqtt_app_status());

    /* Sensors (read-only info) */
    AP("<div class=\"sec\"><div class=\"st\">Sensor &amp; Display</div><div class=\"sb\"><table>"
       "<tr><td class=\"lb\">Temperature Sensor</td><td><span class=\"ro\">DHT11</span></td>"
       "<td class=\"lb\">LCD</td><td><span class=\"ro\">20x4</span></td>"
       "<td></td><td></td></tr>"
       "<tr><td class=\"lb\">Temp (last report)</td><td><span class=\"ro\" id=\"tv\">%d &deg;C</span></td>"
       "<td class=\"lb\">Humidity (last report)</td><td><span class=\"ro\" id=\"hv\">%d %%</span></td>"
       "<td class=\"lb\">Sensor</td><td><span class=\"ro\" id=\"sv\">%s</span></td></tr>"
       "</table>"
       "<div class=\"note\">Web/MQTT values update every 60 s and immediately when the alarm changes.</div>"
       "</div></div>",
       (int)(g_web_temp_mc / 1000), (int)(g_web_humi_mp / 1000),
       g_web_sensor_ok ? "OK" : "ERROR");

    /* Password */
    AP("<div class=\"sec\"><div class=\"st\">Modify Web Login Password</div><div class=\"sb\"><table>"
       "<tr><td class=\"lb\">New Password</td>"
       "<td><input type=\"password\" name=\"new_pw\" placeholder=\"leave blank to keep\"></td>"
       "<td class=\"lb\">Confirm Password</td>"
       "<td><input type=\"password\" name=\"cfm_pw\" placeholder=\"re-enter new password\"></td>"
       "<td></td><td></td></tr>"
       "</table></div></div>");

    AP("<div class=\"sw\"><button class=\"sbtn\" type=\"submit\">Save &amp; Apply</button></div>");
    AP("<script>"
       "function poll(){fetch('/status').then(function(r){return r.json();}).then(function(d){"
       "document.getElementById('al').textContent=d.alarm?d.reason:'OK';"
       "document.getElementById('mo').textContent=d.motor?'ON':'OFF';"
       "document.getElementById('tv').textContent=d.temp+' \\u00b0C';"
       "document.getElementById('hv').textContent=d.humi+' %%';"
       "document.getElementById('sv').textContent=d.sensor_ok?'OK':'ERROR';"
       "}).catch(function(){});}"
       "poll();setInterval(poll,5000);"
       "</script>");
    AP("</form></div></body></html>");

#undef AP

    http_respond(fd, 200, "text/html", s_page, off, NULL);
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
static void handle_save(int fd, const char *hdr, int hlen)
{
    char body[2048] = {0};
    char passbuf[280];
    http_read_body(fd, hdr, hlen, body, sizeof(body));

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
    if (http_form_field(body, "mqtt_topic", tmp, sizeof(tmp)))
        strncpy(nc.mqtt_topic, tmp, sizeof(nc.mqtt_topic) - 1);
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
    char buf[256];
    int n = snprintf(buf, sizeof(buf),
        "{\"temp\":%d,\"humi\":%d,\"sensor_ok\":%d,\"cnt\":%u,\"ip\":\"%s\","
        "\"alarm\":%d,\"reason\":\"%s\",\"motor\":%d}",
        (int)(g_web_temp_mc / 1000), (int)(g_web_humi_mp / 1000),
        g_web_sensor_ok ? 1 : 0, g_web_cnt, g_device_ip,
        alarm_active() ? 1 : 0, alarm_reason_str(alarm_reason()),
        motor_is_on() ? 1 : 0);
    http_respond(fd, 200, "application/json", buf, n, NULL);
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
K_THREAD_DEFINE(web_tid, 4096, web_server_thread, NULL, NULL, NULL, 5, 0, 2000);
