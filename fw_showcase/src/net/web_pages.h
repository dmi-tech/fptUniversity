#ifndef WEB_PAGES_H
#define WEB_PAGES_H

/* Static HTML/CSS of the web UI. Included only by web_server.c (the arrays
 * are static so sizeof() works there). HTML_LOGIN is a printf format string. */

/* Login page — one %s placeholder for error message */
static const char HTML_LOGIN[] =
"<!DOCTYPE html><html><head>"
"<meta charset=\"UTF-8\">"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
"<title>fw_showcase Login</title>"
"<style>"
"*{margin:0;padding:0;box-sizing:border-box}"
"body{background:#eef0f3;display:flex;justify-content:center;align-items:center;min-height:100vh;font-family:Arial,sans-serif}"
".card{background:#fff;border-radius:10px;padding:44px 52px;box-shadow:0 4px 20px rgba(0,0,0,.13);width:340px;text-align:center}"
".logo{font-size:26px;font-weight:700;color:#5a9a00;margin-bottom:4px}"
".sub{font-size:12px;color:#999;margin-bottom:30px}"
"label{display:block;font-size:13px;font-weight:700;color:#5a9a00;text-align:left;margin-bottom:6px}"
"input{width:100%%;padding:9px 12px;border:1px solid #d0d0d0;border-radius:5px;font-size:14px;outline:none}"
"input:focus{border-color:#6aaa00}"
".btn{width:100%%;background:#6aaa00;color:#fff;border:none;padding:11px;font-size:15px;font-weight:600;border-radius:5px;cursor:pointer;margin-top:16px}"
".btn:hover{background:#5a9000}"
".err{color:#c00;font-size:12px;margin-top:12px;min-height:16px}"
"</style></head><body>"
"<div class=\"card\">"
"<div class=\"logo\">fw_showcase</div>"
"<div class=\"sub\">Device Configuration Portal</div>"
"<form method=\"POST\" action=\"/login\">"
"<label>Password:</label>"
"<input type=\"password\" name=\"pw\" placeholder=\"Please enter password\" autofocus>"
"<button class=\"btn\" type=\"submit\">Login</button>"
"</form>"
"<div class=\"err\">%s</div>"
"</div></body></html>";

/* Config page — built in sections via send_config_page() */
static const char CFG_CSS[] =
"<style>"
"*{margin:0;padding:0;box-sizing:border-box}"
"body{font-family:Arial,sans-serif;background:#eef0f3}"
".hdr{background:#5a9a00;color:#fff;padding:10px 24px;display:flex;justify-content:space-between;align-items:center}"
".hdr-logo{font-size:18px;font-weight:700}"
".hdr-sub{font-size:11px;opacity:.8;margin-top:2px}"
".hdr-ip{font-size:12px;background:rgba(255,255,255,.2);padding:4px 10px;border-radius:4px}"
".lbtn{background:#fff;color:#5a9a00;border:none;padding:5px 14px;border-radius:4px;cursor:pointer;font-size:13px;font-weight:600}"
".content{max-width:960px;margin:16px auto;padding:0 14px}"
".sec{background:#fff;border:1px solid #e0e0e0;border-radius:6px;margin-bottom:12px;overflow:hidden}"
".st{background:#f7f7f7;color:#5a9a00;font-weight:700;font-size:13px;padding:8px 16px;border-bottom:1px solid #e8e8e8}"
".sb{padding:10px 16px}"
"table{width:100%%;border-collapse:collapse}"
"td{padding:5px 7px;font-size:13px;vertical-align:middle}"
".lb{color:#555;width:155px;white-space:nowrap}"
".ro{display:inline-block;background:#f5f5f5;border:1px solid #e0e0e0;border-radius:4px;padding:3px 9px;font-size:13px;min-width:168px;color:#444}"
"input[type=text],input[type=password]{border:1px solid #ccc;padding:4px 8px;border-radius:4px;font-size:13px;width:168px;outline:none}"
"input:focus{border-color:#6aaa00}"
"select{border:1px solid #ccc;padding:4px 6px;border-radius:4px;font-size:13px;background:#fff;outline:none}"
"select:focus{border-color:#6aaa00}"
".note{font-size:11px;color:#888;padding:4px 16px 8px;font-style:italic}"
".sw{text-align:center;padding:16px 0 8px}"
".sbtn{background:#6aaa00;color:#fff;border:none;padding:10px 50px;font-size:15px;font-weight:600;border-radius:5px;cursor:pointer}"
".sbtn:hover{background:#5a9000}"
".iob{display:inline-block;color:#fff;font-weight:600;font-size:12px;padding:3px 12px;border-radius:10px;min-width:46px;text-align:center;background:#bbb}"
".iobtn{border:none;color:#fff;font-weight:600;font-size:12px;padding:6px 16px;border-radius:5px;cursor:pointer;background:#999;min-width:78px}"
"</style>";

/* Success page after save */
static const char HTML_SAVED[] =
"<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
"<meta http-equiv=\"refresh\" content=\"3;url=/login\">"
"<title>Saved</title>"
"<style>body{font-family:Arial,sans-serif;background:#eef0f3;display:flex;justify-content:center;align-items:center;height:100vh}"
".box{background:#fff;border-radius:8px;padding:40px 50px;text-align:center;box-shadow:0 4px 16px rgba(0,0,0,.12)}"
".ok{color:#5a9a00;font-size:48px;margin-bottom:12px}"
"h2{color:#333;margin-bottom:8px}"
"p{color:#888;font-size:14px}</style></head><body>"
"<div class=\"box\"><div class=\"ok\">&#10003;</div>"
"<h2>Configuration Saved</h2>"
"<p>Device is applying settings and will reboot.<br>Redirecting to login in 3 seconds...</p>"
"</div></body></html>";

#endif /* WEB_PAGES_H */
