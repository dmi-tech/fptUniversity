#ifndef WEB_PAGE_H
#define WEB_PAGE_H

/* Static parts of this showcase's web page (no logic), used by the web hooks
 * in main.c:
 *   WEB_SECTIONS  the Light, Lamp and Settings sections; the values are filled
 *                 in by the script
 *   WEB_JS        the actions (POST /api/...) and appPoll(d), called every 2 s
 *                 with d.app = the MQTT status JSON (mqtt_app_build_status()):
 *                 {"lcd":{...},"lamp":{...},"light":{...},"mode":{...},
 *                  "sensor":{...}}
 *                 The Settings inputs are filled once, from the first poll.
 * Value cells: <td class="lb">label</td><td><span class="ro" id="..">. */

#define TD_RO(label, id) "<td class=\"lb\">" label "</td><td><span class=\"ro\" id=\"" id "\">--</span></td>"
#define TD_NUM(label, id, min, max) "<td class=\"lb\">" label "</td><td><input type=\"number\" id=\"" id \
    "\" min=\"" #min "\" max=\"" #max "\"></td>"
#define SW2(id_a, a, on_a, id_b, b, on_b) \
    "<span class=\"sw\"><button type=\"button\" id=\"" id_a "\" onclick=\"" on_a "\">" a "</button>" \
    "<button type=\"button\" id=\"" id_b "\" onclick=\"" on_b "\">" b "</button></span>"
#define COLS8 "<colgroup><col class=\"l\"><col><col class=\"s\"><col><col class=\"s\"><col><col class=\"s\"><col></colgroup>"

static const char WEB_SECTIONS[] =
    "<style>"
    "input[type=number]{display:block;width:100%;border:1px solid #ccc;padding:4px 8px;"
    "border-radius:4px;font-size:13px;height:26px;outline:none}"
    ".bd{display:inline-block;padding:3px 10px;border-radius:4px;font-weight:700;font-size:12px;color:#fff;background:#888}"
    ".bd.dk{background:#3949ab}.bd.br{background:#e0a000}"
    "</style>"

    /* Light */
    "<div class=\"sec\"><div class=\"st\">Light</div><div class=\"sb\"><table>" COLS8
    "<tr>" TD_RO("Light", "lx") TD_RO("Ambient", "am") TD_RO("Lamp light", "ll") TD_RO("Reads", "rc") "</tr>"
    "<tr><td class=\"lb\">State</td><td><span class=\"bd\" id=\"st2\">--</span></td>"
    "<td colspan=\"6\"><span class=\"ro\" id=\"pd\">--</span></td></tr>"
    "</table></div></div>"

    /* Lamp */
    "<div class=\"sec\"><div class=\"st\">Lamp</div><div class=\"sb\"><table>" COLS8
    "<tr><td class=\"lb\">Lamp</td><td colspan=\"3\">"
    SW2("l1", "ON", "lamp(1)", "l0", "OFF", "lamp(0)") "<span class=\"by\" id=\"lby\"></span></td>"
    "<td class=\"lb\">Mode</td><td colspan=\"3\">"
    SW2("ma", "AUTO", "mode('auto')", "mm", "MANUAL", "mode('manual')") "<span class=\"by\" id=\"mh\"></span></td></tr>"
    "<tr><td class=\"lb\">Off timer</td><td><span class=\"ro\" id=\"th\">--</span></td>"
    "<td><button type=\"button\" class=\"btn\" onclick=\"tmr(-1)\">-1 h</button></td>"
    "<td><button type=\"button\" class=\"btn\" onclick=\"tmr(1)\">+1 h</button></td>"
    TD_RO("Left", "tl") "<td colspan=\"2\"><span class=\"fb\" id=\"lf\"></span></td></tr>"
    "</table></div></div>"

    /* Settings */
    "<div class=\"sec\"><div class=\"st\">Settings</div><div class=\"sb\"><table>"
    "<colgroup><col class=\"l\"><col><col class=\"l\"><col><col class=\"l\"><col></colgroup><tr>"
    TD_NUM("Lamp on below (lx)", "son", 1, 5000) TD_NUM("Lamp off above (lx)", "sof", 1, 5000)
    TD_NUM("Confirm time (s)", "scf", 5, 600)
    "</tr><tr><td></td><td colspan=\"3\"><span class=\"fb\" id=\"sf\"></span></td>"
    "<td></td><td><button type=\"button\" class=\"btn\" onclick=\"sset()\">Save</button></td></tr>"
    "</table></div></div>";

static const char WEB_JS[] =
    "var tmrH=0,setDone=0;"
    "function lamp(on){api('/api/lamp',{on:on},'lf')}"
    "function mode(m){api('/api/mode',{mode:m},'lf')}"
    "function tmr(d){api('/api/timer',{hours:Math.max(0,Math.min(12,tmrH+d))},'lf')}"
    "function sset(){api('/api/settings',{on_lux:E('son').value,off_lux:E('sof').value,"
    "confirm_s:E('scf').value},'sf')}"
    "function hm(s){var m=Math.ceil(s/60);return Math.floor(m/60)+' h '+p2(m%60)+' min'}"
    "function appPoll(d){var a=d.app,s=a.sensor,g=a.light,p=a.lamp;"
    "if(!setDone){E('son').value=g.on_lux;E('sof').value=g.off_lux;E('scf').value=g.confirm_s;setDone=1}"
    "E('il').textContent=g.state=='dark'?'Dark':g.state=='bright'?'Bright':'--';"
    "E('is').textContent=s.fault?'FAULT':(s.ok?'OK':'ERR');"
    "E('id').textContent=a.lcd.ok?'OK':'ERR';"
    "E('lx').textContent=s.have?g.lux+' lx':'--';"
    "E('am').textContent=s.have?g.ambient_lux+' lx':'--';"
    "E('ll').textContent=g.lamp_lux+' lx';"
    "E('rc').textContent=s.reads;"
    "var b=E('st2');b.textContent=g.state=='dark'?'DARK':g.state=='bright'?'BRIGHT':'--';"
    "b.className='bd '+(g.state=='dark'?'dk':g.state=='bright'?'br':'');"
    "E('pd').textContent=s.fault?'sensor fault: the lamp keeps its state':"
    "(g.pending_s?(g.state=='dark'?'getting bright':'getting dark')+', confirmed in '+g.pending_s+' s':"
    "'on below '+g.on_lux+' lx, off above '+g.off_lux+' lx (after '+g.confirm_s+' s)');"
    "E('l1').className=p.on?'on':'';E('l0').className=p.on?'':'off';"
    "E('lby').textContent=p.by?'(by '+p.by+')':'';"
    "var au=a.mode.mode=='auto';E('ma').className=au?'on':'';E('mm').className=au?'':'on';"
    "E('mh').textContent=au&&p.hold?'(held off by the timer until day)':'';"
    "tmrH=p.timer_h;E('th').textContent=p.timer_h?p.timer_h+' h':'off';"
    "E('tl').textContent=p.on&&p.timer_left_s?hm(p.timer_left_s):'-';"
    "}";

#undef TD_RO
#undef TD_NUM
#undef SW2
#undef COLS8

#endif /* WEB_PAGE_H */
