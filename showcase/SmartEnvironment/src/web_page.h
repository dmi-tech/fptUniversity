#ifndef WEB_PAGE_H
#define WEB_PAGE_H

/* Static parts of this showcase's web page (no logic), used by the web hooks
 * in main.c:
 *   WEB_SECTIONS  the Environment and Devices sections; the values are filled
 *                 in by the script
 *   WEB_JS        the actions (POST /api/...) and appPoll(d), called every 2 s
 *                 with d.app = the MQTT status JSON (mqtt_app_build_status()):
 *                 {"lcd":{...},"pump":{...},"servo":{...},"env":{...},
 *                  "sensor":{...},"relay":{...}}
 * Value cells: <td class="lb">label</td><td><span class="ro" id="..">. */

#define TD_RO(label, id) "<td class=\"lb\">" label "</td><td><span class=\"ro\" id=\"" id "\">--</span></td>"
#define SW2(id_a, a, on_a, id_b, b, on_b) \
    "<span class=\"sw\"><button type=\"button\" id=\"" id_a "\" onclick=\"" on_a "\">" a "</button>" \
    "<button type=\"button\" id=\"" id_b "\" onclick=\"" on_b "\">" b "</button></span>"
#define COLS8 "<colgroup><col class=\"l\"><col><col class=\"s\"><col><col class=\"s\"><col><col class=\"s\"><col></colgroup>"

static const char WEB_SECTIONS[] =
    "<style>"
    ".bd{display:inline-block;padding:3px 10px;border-radius:4px;font-weight:700;font-size:12px;color:#fff;background:#888}"
    ".bd.l0{background:#5a9a00}.bd.l1{background:#e09a00}.bd.l2{background:#c62828}"
    ".adv{background:#fafafa;border-left:4px solid #888;padding:7px 10px;font-size:13px;color:#333;border-radius:3px}"
    ".adv.l0{border-color:#5a9a00}.adv.l1{border-color:#e09a00}.adv.l2{border-color:#c62828}"
    "input[type=range]{width:100%;vertical-align:middle}"
    ".btn:disabled{background:#bbb;cursor:default}"
    "</style>"

    /* Environment */
    "<div class=\"sec\"><div class=\"st\">Environment</div><div class=\"sb\"><table>" COLS8
    "<tr>" TD_RO("Temperature", "tv") TD_RO("Humidity", "hv") TD_RO("Heat index", "hi")
    TD_RO("Reads", "rc") "</tr>"
    "<tr><td class=\"lb\">Situation</td><td><span class=\"bd\" id=\"lvl\">--</span></td>"
    "<td colspan=\"6\"><span class=\"ro\" id=\"et\">--</span></td></tr>"
    "<tr><td class=\"lb\">Advice</td><td colspan=\"7\"><div class=\"adv\" id=\"ea\">--</div></td></tr>"
    "</table></div></div>"

    /* Devices */
    "<div class=\"sec\"><div class=\"st\">Devices</div><div class=\"sb\"><table>" COLS8
    "<tr><td class=\"lb\">Pump</td><td colspan=\"3\">"
    SW2("p1", "ON", "pump(1)", "p0", "OFF", "pump(0)") "<span class=\"by\" id=\"pby\"></span></td>"
    TD_RO("Auto", "pa")
    "<td colspan=\"2\"><button type=\"button\" class=\"btn\" id=\"rearm\" onclick=\"rearm()\" disabled>Re-enable auto</button></td></tr>"
    "<tr><td class=\"lb\">Watering</td><td colspan=\"3\"><span class=\"ro\" id=\"ps\">--</span></td>"
    TD_RO("Cycles/h", "pc") "<td colspan=\"2\"><span class=\"fb\" id=\"pf\"></span></td></tr>"
    "<tr><td class=\"lb\">Servo</td><td colspan=\"3\">"
    SW2("sa", "AUTO", "smode('auto')", "sm", "MANUAL", "smode('manual')") "<span class=\"by\" id=\"sw\"></span></td>"
    TD_RO("Angle", "an") "<td colspan=\"2\"><span class=\"fb\" id=\"sf\"></span></td></tr>"
    "<tr><td class=\"lb\">Set angle</td>"
    "<td colspan=\"5\"><input type=\"range\" id=\"sl\" min=\"0\" max=\"180\" value=\"90\" disabled></td>"
    "<td><span class=\"ro\" id=\"slv\">90°</span></td>"
    "<td><button type=\"button\" class=\"btn\" id=\"sb\" onclick=\"sang()\" disabled>Set</button></td></tr>"
    "<tr>" TD_RO("Relay 1", "rl")
    "<td colspan=\"6\"><span class=\"by\">blinks slowly on a warning, fast on a critical situation</span></td></tr>"
    "</table></div></div>";

static const char WEB_JS[] =
    "function pump(on){api('/api/pump',{on:on},'pf')}"
    "function rearm(){api('/api/pump/rearm',{},'pf')}"
    "function smode(m){api('/api/servo',{mode:m},'sf')}"
    "function sang(){api('/api/servo/angle',{deg:E('sl').value},'sf')}"
    "E('sl').addEventListener('input',function(){E('slv').textContent=this.value+'\\u00b0'});"
    "function appPoll(d){var a=d.app,s=a.sensor,e=a.env,p=a.pump,v=a.servo,h=s.temp!=null;"
    "E('ie').textContent=e.code;"
    "E('is').textContent=s.fault?'FAULT':(s.ok?'OK':'ERR');"
    "E('il').textContent=a.lcd.ok?'OK':'ERR';"
    "E('tv').textContent=h?s.temp+' \\u00b0C':'--';"
    "E('hv').textContent=h?s.humi+' %':'--';"
    "E('hi').textContent=h&&s.hi_valid?s.heat_index+' \\u00b0C':'--';"
    "E('rc').textContent=s.reads;"
    "var l=E('lvl');l.textContent=['OK','WARNING','CRITICAL'][e.level];l.className='bd l'+e.level;"
    "E('et').textContent=e.title+' ('+e.code+')';"
    "var x=E('ea');x.textContent=e.advice;x.className='adv l'+e.level;"
    "E('p1').className=p.on?'on':'';E('p0').className=p.on?'':'off';"
    "E('pby').textContent=p.by?'(by '+p.by+')':'';"
    "E('pa').textContent=p.auto=='inhibited'?'INHIBITED':'ARMED';"
    "E('rearm').disabled=p.auto!='inhibited';"
    "E('ps').textContent=p.session?(p.resting?'auto, resting '+p.left_s+' s':'auto, watering '+p.left_s+' s')"
    ":(p.on&&p.by!='auto'?'manual, stops in '+p.left_s+' s':(e.water_request?'requested':'-'));"
    "E('pc').textContent=p.cycles;"
    "var m=v.mode=='manual';"
    "E('sa').className=m?'':'on';E('sm').className=m?'on':'';"
    "E('sw').textContent=m?'(BTN3/BTN4 or slider)':(v.sweeping?'(sweeping)':'(parked until the pump runs)');"
    "E('an').textContent=v.angle+'\\u00b0';"
    "E('sl').disabled=E('sb').disabled=!m;"
    "if(!m||document.activeElement!=E('sl')){E('sl').value=v.angle;E('slv').textContent=v.angle+'\\u00b0'}"
    "E('rl').textContent=a.relay.mode;"
    "}";

#undef TD_RO
#undef SW2
#undef COLS8

#endif /* WEB_PAGE_H */
