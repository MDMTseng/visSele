// THE ONE BUTTON MEANT TO BE FIRED BY KEYBOARD COULD NOT BE.
//
// 手動吹氣 is pressed while the operator watches the PLATE, not the screen:
// focus it once with the mouse, then press Enter when the part you want is
// where you want it. It was `disabled` for the duration of the request, and a
// browser takes focus off a disabled element -- so the first press worked,
// focus fell to <body>, and every Enter after it did nothing.
//
// Asserts the focus survives a press and that Enter sends another blow. The
// plate is stopped for this, so the board refuses every one of them; a REFUSED
// blow is still a blow that was asked for, which is what this checks.
import { makeCtl, toMain, dismissCamModal, freshPage, sleep } from './lib_enter.mjs';
const ctl = makeCtl('http://127.0.0.1:8765'); const { ev, api } = ctl;
const APP = process.argv[2] || 'http://127.0.0.1:8083/';
const SEL = '#blowbtn';

process.env.WEBCTL_COLD = '1';
await freshPage(ctl, APP);
await sleep(6000);
await toMain(ctl); await dismissCamModal(ctl);

// Open the 全檢設備v2 panel from the status bar.
const opened = await ev(`(function(){
  var b=Array.from(document.querySelectorAll('button')).find(function(e){
    return (e.textContent||'').indexOf('全檢設備v2')>=0; });
  if(!b) return 'no status row';
  b.click(); return 'clicked';})()`);
console.log('open panel:', opened);
await sleep(4000);

// 手動吹氣 lives inside the 站點時序 fold, which opens closed -- so the button
// is not in the DOM at all until the section is expanded. A suite that only
// searched for it would report "no blow button" and say nothing about focus.
console.log('expand 站點時序:', await ev(`(function(){
  var t=Array.from(document.querySelectorAll('*')).find(function(e){
    return e.children.length===0 && (e.textContent||'').trim().indexOf('站點時序')===0; });
  if(!t) return 'no fold';
  var n=t; for(var i=0;i<6&&n;i++){ if(typeof n.click==='function') n.click(); n=n.parentElement; }
  return 'clicked';})()`));
await sleep(1500);

// Tag the button so the press has a stable selector. The panel builds itself
// from a get_running_stat round trip, so wait for the row rather than assuming
// a fixed delay.
let tagged = 'no blow button';
for (let i = 0; i < 20 && tagged !== 'ok'; i++) {
  await sleep(1000);
  tagged = await ev(`(function(){
  var b=Array.from(document.querySelectorAll('button')).find(function(e){
    return (e.textContent||'').trim()==='吹 SEL1'; });
  if(!b) return 'no blow button';
  b.id='blowbtn';
  // The row sits well down a scrolling panel; playwright refuses to click what
  // is not in view, and so does a person.
  b.scrollIntoView({ block: 'center' });
  return 'ok';})()`);
}
await sleep(1500);
console.log('blow button:', tagged);
if (tagged !== 'ok') { console.log('\nFAILED'); process.exit(1); }

const blows = async () => {
  const l = (await api('/logs?' + new URLSearchParams({ since: '0' }))).logs || [];
  return l.filter((e) => String(e.text || '').indexOf('blow') >= 0).length;
};

let fail = 0;
const before = await blows();
await api('/click', { selector: SEL });
await sleep(2500);
const focused = await ev(`(function(){var a=document.activeElement;
  return a ? (a.tagName+':'+(a.textContent||'').trim()) : 'none';})()`);
const afterClick = await blows();
const ok1 = focused.indexOf('吹 SEL1') >= 0;
console.log('  ' + (ok1 ? 'ok  ' : 'FAIL') + ' focus survives the press: ' + focused);
if (!ok1) fail++;

await api('/press', { selector: SEL, key: 'Enter' });
await sleep(2500);
const afterEnter = await blows();
const ok2 = afterEnter > afterClick;
console.log('  ' + (ok2 ? 'ok  ' : 'FAIL') + ' Enter sends another blow: '
  + before + ' -> ' + afterClick + ' -> ' + afterEnter + ' log line(s)');
if (!ok2) fail++;

console.log(fail ? `\n${fail} FAILED` : '\nPASS');
process.exit(fail ? 1 : 0);
