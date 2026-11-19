import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import vm from "node:vm";
import test from "node:test";

const code = ["panel-dates", "timezones"].map(name => readFileSync(
  new URL("../../components/webui/www/js/" + name + ".js", import.meta.url), "utf8")).join("\n");
const catalog = JSON.parse(readFileSync(new URL("../../components/webui/i18n/es.json", import.meta.url)));
catalog["settings.time.no_matches"] = "No matching timezones";
catalog["settings.time.legacy_hint"] = "Existing timezone is preserved";
catalog["settings.time.clock_unavailable"] = "Device time unavailable";
const names = Object.keys(JSON.parse(readFileSync(new URL("../../main/net/tzdb/zones.json", import.meta.url))));
const clockSamples = {
  "America/New_York": ["2025-12-31T19:30:00", "M/D/Y", "12/31/2025 19:30:00"],
  "Europe/Berlin": ["2026-01-01T01:30:00", "D.M.Y", "01.01.2026 01:30:00"],
  "Asia/Tokyo": ["2026-01-01T09:30:00", "Y/M/D", "2026/01/01 09:30:00"],
};
function sample(zone) {
  const [local_time, date_pattern] = clockSamples[zone];
  return { active_timezone: zone, clock_valid: true, local_time, date_pattern };
}
function setup() {
  const nodes = new Map();
  function node() { return { dispatchEvent() {}, value: "", textContent: "", hidden: true, children: [], attrs: {}, offsetParent: {},
    classList: { toggle() {} }, setAttribute(k,v) { this.attrs[k]=v; }, removeAttribute(k) { delete this.attrs[k]; },
    replaceChildren(...v) { this.children=v; }, append(v) { this.children.push(v); }, scrollIntoView() {}, select() {},
    focus() { this.onfocus?.(); }, contains() { return false; } }; }
  const get = id => { if (!nodes.has(id)) nodes.set(id,node()); return nodes.get(id); };
  let now = 0, intervals = 0, requests = 0;
  const clock = { clock_valid:true, local_time:"2026-09-23T05:42:31", active_timezone:"Africa/Ceuta", date_pattern:"D/M/Y" };
  const context = vm.createContext({
    el:{settingsTimezone:get("settingsTimezone")}, editor:{activePane:"settings",activeSettingsSection:"settingsUiSection",settings:{ui:{language:"en"}}},
    document:{getElementById:get,createElement:node,hidden:false},
    window:{setInterval(){intervals++;},setTimeout(){},clearTimeout(){}},
    performance:{now:()=>now}, t:key=>catalog[key], Intl, Date, AbortController, Event,
    apiGet:async path=>{assert.equal(path,"/api/settings?time_only=1");requests++;return clock;},
    renderBuildTime(){},renderFirmwareInfo(){},
  });
  vm.runInContext(code,context);
  const time = {...clock,timezone:"Africa/Ceuta",timezones:names,ntp_server:"pool.ntp.org"};
  context.editor.settings.time = time;
  vm.runInContext("receiveTimezoneClock(editor.settings.time, ++timezonePicker.requestId)", context);
  context.renderTimezoneSettings(time);
  return {context,get,time,advance:ms=>{now+=ms;},intervals:()=>intervals,requests:()=>requests};
}
test("search shang returns Shanghai and selection keeps exact identifier",()=>{
  const {context:c,get}=setup();
  get("settingsTimezoneSearch").value="shang";
  get("settingsTimezoneSearch").oninput();
  const options=get("settingsTimezoneOptions");
  assert.deepEqual(options.children.map(n=>n.textContent),["Asia/Shanghai"]);
  options.children[0].onclick();
  assert.equal(get("settingsTimezone").value,"Asia/Shanghai");
  assert.equal(get("settingsTimezoneSearch").value,"Asia/Shanghai");
  assert.equal(options.hidden,true);
  c.renderTimezoneSettings({timezone:"Asia/Shanghai",timezones:names});
  assert.equal(get("settingsTimezoneSearch").value,"Asia/Shanghai");
});
test("keyboard selection, no results and Escape preserve valid selection",()=>{
  const {context:c,get}=setup();
  c.showTimezoneOptions("SHANG");
  c.timezoneKeydown({key:"ArrowDown",preventDefault(){}});
  assert.equal(get("settingsTimezoneSearch").attrs["aria-activedescendant"],"timezone-option-0");
  c.timezoneKeydown({key:"Enter",preventDefault(){}});
  assert.equal(get("settingsTimezone").value,"Asia/Shanghai");
  c.showTimezoneOptions("no_such_zone");
  assert.equal(get("settingsTimezoneOptions").children[0].textContent,"No matching timezones");
  c.timezoneKeydown({key:"Escape",preventDefault(){}});
  assert.equal(get("settingsTimezoneSearch").value,"Asia/Shanghai");
});
test("legacy values remain stored but are not exposed as editable POSIX text",()=>{
  const {context:c,get,intervals}=setup();
  const legacy="CET-1CEST,M3.5.0,M10.5.0/3";
  c.renderTimezoneSettings({timezone:legacy,timezones:names});
  assert.equal(get("settingsTimezone").value,legacy);
  assert.equal(get("settingsTimezoneSearch").value,"");
  assert.match(get("settingsTimezoneHint").textContent,/preserved/);
  assert.equal(intervals(),1);
});
test("clock uses device local time, ticks, and expires stale data",()=>{
  const {context:c,get,advance,time}=setup();
  assert.equal(get("settingsLocalTime").textContent,"23/09/2026 05:42:31");
  advance(2000);c.updateTimezoneClock();
  assert.match(get("settingsLocalTime").textContent,/05:42:33/);
  c.renderTimezoneSettings(time); // rerender must not reset the device snapshot
  assert.match(get("settingsLocalTime").textContent,/05:42:33/);
  advance(15000);c.updateTimezoneClock();
  assert.match(get("settingsLocalTime").textContent,/unavailable/);
});
test("clock polling is throttled and paused when hidden",async()=>{
  const {context:c,advance,requests}=setup();
  await c.tickTimezoneClock();assert.equal(requests(),0);
  advance(5000);await c.tickTimezoneClock();assert.equal(requests(),1);
  advance(5000);c.document.hidden=true;await c.tickTimezoneClock();assert.equal(requests(),1);
});

test("native timezone changes update the selected location and date without saving", async () => {
  const {context:c,get,advance}=setup();
  c.putSettings = () => assert.fail("A clock read must not save settings");
  for (const zone of Object.keys(clockSamples)) {
    c.apiGet = async () => sample(zone);
    advance(5000);
    await c.tickTimezoneClock();
    assert.equal(c.editor.settings.time.timezone, zone);
    assert.equal(c.editor.settings.time.ntp_server, "pool.ntp.org");
    assert.equal(get("settingsTimezone").value, zone);
    assert.equal(get("settingsTimezoneSearch").value, zone);
    assert.equal(get("settingsActiveTimezone").textContent, zone);
    assert.equal(get("settingsLocalTime").textContent, clockSamples[zone][2]);
  }
});

test("a native change preserves a typed search, caret and keyboard selection", async () => {
  const {context:c,get,advance}=setup();
  const search=get("settingsTimezoneSearch");
  const options=get("settingsTimezoneOptions");
  search.value="toky";
  search.selectionStart=1;
  search.selectionEnd=2;
  search.oninput();
  c.timezoneKeydown({key:"ArrowDown",preventDefault(){}});
  const option=options.children[0];
  c.apiGet = async () => sample("Asia/Tokyo");
  advance(5000);
  await c.tickTimezoneClock();
  c.renderTimezoneSettings(c.editor.settings.time);
  assert.equal(get("settingsTimezone").value,"Asia/Tokyo");
  assert.equal(get("settingsLocalTime").textContent,"2026/01/01 09:30:00");
  assert.equal(options.hidden,false);
  assert.equal(options.children[0],option);
  assert.equal(option.attrs["aria-selected"],"true");
  assert.equal(search.attrs["aria-activedescendant"],option.id);
  assert.equal(search.value,"toky");
  assert.deepEqual([search.selectionStart,search.selectionEnd],[1,2]);
  c.timezoneKeydown({key:"Escape",preventDefault(){}});
  assert.equal(search.value,"Asia/Tokyo");
});

test("a saved timezone refreshes the preview immediately and an older pending poll cannot undo it", async () => {
  const {context: c, get, advance} = setup();
  let finishPoll, save;
  c.apiGet = () => new Promise(resolve => { finishPoll = resolve; });
  advance(5000);
  const poll = c.tickTimezoneClock();
  c.setTimeout = fn => { save = fn; return 1; };
  c.clearTimeout = () => {};
  c.renderBuildTime = c.renderFirmwareInfo = () => {};
  c.putSettings = async payload => {
    assert.equal(payload.time.timezone, "America/New_York");
    assert.equal(payload.reboot, false);
    return {active_timezone:"America/New_York", date_pattern:"M/D/Y", timezone_rules:"EST5EDT,M3.2.0,M11.1.0",
      clock_valid:true, local_time:"2025-12-31T19:30:00"};
  };
  get("settingsTimezone").value = "America/New_York";
  c.pushTimezoneLive();
  await save();
  finishPoll({active_timezone:"Europe/Berlin", date_pattern:"D.M.Y", clock_valid:true, local_time:"2026-01-01T01:30:00"});
  await poll;
  assert.equal(c.editor.settings.time.active_timezone, "America/New_York");
  assert.equal(get("settingsLocalTime").textContent, "12/31/2025 19:30:00");
});

test("a poll started before a choice cannot reset the form during debounce", async () => {
  const {context:c,get,advance}=setup();
  let reply;
  c.apiGet = () => new Promise(resolve => { reply=resolve; });
  advance(5000);
  const poll=c.tickTimezoneClock();
  c.setTimeout=()=>1;
  c.clearTimeout=()=>{};
  c.selectTimezone("Asia/Tokyo");
  c.pushTimezoneLive();
  reply(sample("Europe/Berlin"));
  await poll;
  assert.equal(get("settingsTimezoneSearch").value,"Asia/Tokyo");
  assert.equal(get("settingsActiveTimezone").textContent,"Africa/Ceuta");
  assert.equal(get("settingsLocalTime").textContent,"23/09/2026 05:42:36");
});

test("rapid browser choices serialize writes and send only the latest queued choice", async () => {
  const {context:c,get}=setup();
  let runSave;
  c.setTimeout=fn=>{ runSave=fn; return 1; };
  c.clearTimeout=()=>{};
  c.renderBuildTime = c.renderFirmwareInfo = ()=>{};
  const writes=[];
  c.putSettings=payload=>new Promise(resolve=>writes.push({payload,resolve}));
  const select = zone => {
    c.selectTimezone(zone);
    c.pushTimezoneLive();
    return runSave();
  };
  const first=select("America/New_York");
  await Promise.resolve();
  const superseded=select("Europe/Berlin");
  const last=select("Asia/Tokyo");
  await Promise.resolve();
  assert.deepEqual(writes.map(w=>w.payload.time.timezone),["America/New_York"]);
  writes[0].resolve(sample("America/New_York"));
  await first;
  await superseded;
  await Promise.resolve();
  assert.equal(get("settingsTimezoneSearch").value,"Asia/Tokyo");
  assert.equal(get("settingsActiveTimezone").textContent,"Africa/Ceuta");
  assert.deepEqual(writes.map(w=>w.payload.time.timezone),["America/New_York","Asia/Tokyo"]);
  writes[1].resolve(sample("Asia/Tokyo"));
  await last;
  assert.equal(get("settingsActiveTimezone").textContent,"Asia/Tokyo");
  assert.equal(get("settingsLocalTime").textContent,"2026/01/01 09:30:00");
});

test("a failed save releases the picker and allows the next device update", async () => {
  const {context:c,get}=setup();
  let runSave, problem;
  c.setTimeout=fn=>{ runSave=fn; return 1; };
  c.clearTimeout=()=>{};
  c.putSettings=async ()=>{ throw new Error("Save rejected"); };
  c.setStatus=text=>{ problem=text; };
  c.selectTimezone("America/New_York");
  c.pushTimezoneLive();
  await runSave();
  assert.equal(problem,"Save rejected");
  assert.equal(get("settingsTimezoneSearch").value,"Africa/Ceuta");
  c.apiGet=async ()=>sample("Europe/Berlin");
  await c.tickTimezoneClock();
  assert.equal(get("settingsTimezoneSearch").value,"Europe/Berlin");
  assert.equal(get("settingsLocalTime").textContent,"01.01.2026 01:30:00");
});
