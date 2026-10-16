#ifndef WEB_CORE_H
#define WEB_CORE_H
#include <stdbool.h>
#include <stddef.h>

/* Web server (web_core.c): login, the /config page frame (Device Information,
 * MQTT Settings, password, Save & Apply), the MQTT subscribe / publish tools,
 * /api/live and /status. The application adds its own rows, sections, script
 * and /api/... routes through the hooks below. */

/* ── Output helpers, valid while a hook is writing a page or JSON ── */
void web_out_str(const char *s);
void web_out_fmt(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
/* Text escaped for an HTML attribute value or element */
void web_out_html(const char *s);
/* Quoted, escaped JSON string */
void web_out_json(const char *s);
/* Labelled read-only value, one <td> pair; id may be NULL */
void web_td_ro(const char *label, const char *id, const char *val);

/* Reply {"ok":0|1,"msg":"..."} to an /api/... request */
void web_api_reply(int fd, bool ok, const char *msg);

/* ── Implemented by each showcase ─────────────────────────────── */
/* Extra cells of the Device Information table: up to 3 web_td_ro() pairs,
 * written into the last row (missing pairs are left empty) */
void web_app_info_cells(void);
/* Sections between "MQTT Settings" and the password section */
void web_app_sections(void);
/* Script of the page. Must define appPoll(d), called with the /api/live JSON
 * every 2 s; d.app is the MQTT status JSON of the showcase
 * (mqtt_app_build_status()), null before the first reading. The script may
 * use E(id), api(url, data, feedback_id) and fb(id, j). */
const char *web_app_js(void);
/* POST /api/<...> after login. Returns false when the path is not one of the
 * showcase's routes (the core then answers 404). */
bool web_app_api_post(int fd, const char *path, const char *body);

/* GET /status (no login) and "app" of /api/live: the MQTT status JSON, from
 * mqtt_app_build_status() of the showcase. */

#endif /* WEB_CORE_H */
