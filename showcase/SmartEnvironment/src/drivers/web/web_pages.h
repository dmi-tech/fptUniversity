#ifndef WEB_PAGES_H
#define WEB_PAGES_H

/* FW_NAME, FW_VERSION: compile definitions from CMakeLists.txt */

/* Static HTML/CSS of the web UI. Included only by web_core.c (the arrays
 * are static so sizeof() works there). HTML_LOGIN is a printf format string.
 * FW_NAME comes from CMakeLists.txt. */

/* Login page — one %s placeholder for error message */
static const char HTML_LOGIN[] =
"<!DOCTYPE html><html><head>"
"<meta charset=\"UTF-8\">"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
"<title>" FW_NAME " Login</title>"
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
"<div class=\"logo\">DMI<br>" FW_NAME "</div>"
"<div class=\"sub\">Device Configuration Portal</div>"
"<form method=\"POST\" action=\"/login\">"
"<label>Password:</label>"
"<input type=\"password\" name=\"pw\" placeholder=\"Please enter password\" autofocus>"
"<button class=\"btn\" type=\"submit\">Login</button>"
"</form>"
"<div class=\"err\">%s</div>"
"</div></body></html>";

/* Config page style + script, streamed as-is (not printf formats) */
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
"table{width:100%;border-collapse:collapse;table-layout:fixed}"
"td{padding:5px 7px;font-size:13px;vertical-align:middle}"
"col.l{width:120px}col.s{width:96px}col.b{width:104px}"
".lb{color:#555;white-space:nowrap}"
".ro{display:block;background:#f5f5f5;border:1px solid #e0e0e0;border-radius:4px;padding:4px 9px;font-size:13px;color:#444;min-height:26px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}"
"input[type=text],input[type=password]{display:block;width:100%;border:1px solid #ccc;padding:4px 8px;border-radius:4px;font-size:13px;height:26px;outline:none}"
"input:focus,textarea:focus{border-color:#6aaa00}"
".sub{color:#5a9a00;font-weight:700;font-size:12px;padding-top:12px;border-bottom:1px solid #eee}"
".btn{width:100%;height:26px;background:#6aaa00;color:#fff;border:none;border-radius:4px;font-size:13px;font-weight:600;cursor:pointer}"
".btn:hover{background:#5a9000}"
".sw{display:inline-flex;border:1px solid #ccc;border-radius:4px;overflow:hidden;vertical-align:middle}"
".sw button{border:none;background:#f5f5f5;color:#777;height:24px;padding:0 16px;font-size:12px;font-weight:700;cursor:pointer}"
".sw button.on{background:#6aaa00;color:#fff}.sw button.off{background:#888;color:#fff}"
".by{font-size:11px;color:#888;font-style:italic;margin-left:8px}"
".fb{font-size:11px;color:#888;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;display:block}"
".fb.ok{color:#5a9a00}.fb.er{color:#c00}"
"textarea{display:block;width:100%;border:1px solid #e0e0e0;border-radius:4px;background:#f9f9f9;font:12px monospace;padding:6px 8px;resize:vertical;color:#333}"
".sv{text-align:center;padding:16px 0 8px}"
".sbtn{background:#6aaa00;color:#fff;border:none;padding:10px 50px;font-size:15px;font-weight:600;border-radius:5px;cursor:pointer}"
".sbtn:hover{background:#5a9000}"
"</style>";

/* Common script: helpers for the showcase script (web_app_js()), MQTT tools
 * and the 2 s poll of /api/live, which calls appPoll(d) of the showcase.
 * CFG_JS goes before the showcase script, CFG_JS_START after it. */
static const char CFG_JS[] =
"<script>"
"var E=function(i){return document.getElementById(i)};"
"function p2(n){return(n<10?'0':'')+n}"
"function hms(s){return p2(Math.floor(s/3600))+':'+p2(Math.floor(s/60)%60)+':'+p2(s%60)}"
"function fb(i,j){var e=E(i);e.textContent=j.msg||'';e.className='fb '+(j.ok?'ok':'er')}"
"function get(r){if(r.status==401){location='/login';throw 0}return r.json()}"
"function api(u,d,i){fetch(u,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},"
"body:new URLSearchParams(d).toString()}).then(get).then(function(j){if(i)fb(i,j);poll()})"
".catch(function(e){if(e!==0&&i)fb(i,{ok:0,msg:'no response'})})}"
"function sub(){api('/api/mqtt/sub',{topic:E('st').value},'ss')}"
"function pub(){api('/api/mqtt/pub',{topic:E('pt').value,msg:E('pm').value},'pf')}"
"function key(i,f){E(i).addEventListener('keydown',function(e){if(e.key=='Enter'){e.preventDefault();f()}})}"
"function poll(){fetch('/api/live').then(get).then(function(d){"
"E('ms').textContent=d.mqtt;"
"if(d.sub){E('sbr').style.display='';var s=E('ss');s.textContent=d.sub_state+': '+d.sub;"
"s.className='fb '+(d.sub_state=='subscribed'?'ok':'');"
"var t=d.msgs.map(function(m){return'['+hms(m.t)+'] '+(m.topic!=d.sub?m.topic+': ':'')+m.data.replace(/[\\r\\n]+/g,' ')+(m.cut?' ...':'')});"
"E('sm').value=t.length?t.join('\\n'):'(no message yet)'}else{E('sbr').style.display='none'}"
"if(window.appPoll&&d.app)appPoll(d);"
"}).catch(function(){})}"
"</script>";

static const char CFG_JS_START[] =
"<script>key('st',sub);key('pt',pub);key('pm',pub);poll();setInterval(poll,2000);</script>";

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
