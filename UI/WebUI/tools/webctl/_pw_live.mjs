// Is the Inspection view showing a LIVE image, or a frozen one?
// Three shots a few seconds apart; if the picture is live the bytes differ.
import { chromium } from 'playwright';
import crypto from 'node:crypto';
const OUT='C:/Users/w2110/Downloads/pw';
const sleep=(ms)=>new Promise(r=>setTimeout(r,ms));
const browser=await chromium.launch({headless:true});
const ctx=await browser.newContext({viewport:{width:1600,height:950}});
const page=await ctx.newPage();
await page.goto('http://localhost:8082/',{waitUntil:'domcontentloaded'});
await sleep(6000);
const clickText=(label,tag='*')=>page.evaluate(({label,tag})=>{
  for(const e of document.querySelectorAll(tag)){
    if(e.children.length||!e.offsetParent) continue;
    if((e.innerText||'').trim()!==label) continue;
    e.click(); return true; } return false; },{label,tag});
const clickIcon=(icon,nth=0)=>page.evaluate(({icon,nth})=>{
  let k=0; for(const b of document.querySelectorAll('button')){
    if(!b.offsetParent||b.disabled) continue;
    if(!b.querySelector('[class*="'+icon+'"]')) continue;
    if(k++===nth){b.click();return true;} } return false; },{icon,nth});

await clickText('跳過相機連線','button'); await sleep(1500);
await clickIcon('anticon-folder-open'); await sleep(3000);
await clickText('test1.hydef'); await sleep(800);
await page.evaluate(()=>{for(const e of document.querySelectorAll('*')){
  if(e.children.length||!e.offsetParent) continue;
  if((e.innerText||'').trim()!=='test1.hydef') continue;
  e.dispatchEvent(new MouseEvent('dblclick',{bubbles:true})); return; }});
await sleep(5000);
await clickText('11沖壓成形'); await sleep(1200);
await clickText('全檢'); await sleep(1200);
await clickIcon('anticon-caret-right'); await sleep(9000);   // into the Inspection UI
await page.evaluate(()=>{const x=document.querySelector('.ant-drawer-close'); if(x&&x.offsetParent) x.click();});
await sleep(2000);

// Start it from THIS session and keep the session open.
//
// Closing the browser closes the last BPG client, the core then deletes the
// peripheral channel, and the board loses its host and stops. Every previous
// script therefore left the machine stopped as it exited -- and the next run
// found the frozen last frame and called the image "static". The picture was
// fine; the machine was not running.
const startBtn = await page.evaluate(()=>{
  const b=[...document.querySelectorAll('button')].find(x=>x.querySelector('[class*=caret-right]'));
  if(!b) return 'none';
  if(b.disabled) return 'disabled';
  b.click(); return 'clicked';
});
console.log('  machine start button:', startBtn);
await sleep(15000);
const panelTxt = await page.evaluate(()=>{
  for(const e of document.querySelectorAll('div')){
    const t=e.innerText||''; if(t.includes('rpm')&&t.length<300) return t.replace(/\s+/g,' ').trim(); }
  return ''; });
console.log('  panel:', panelTxt.slice(0,120));

// What is actually rendering the picture?
const surf = await page.evaluate(()=>{
  const out=[];
  document.querySelectorAll('canvas,img,video').forEach(e=>{
    const r=e.getBoundingClientRect();
    if(r.width<40||r.height<40) return;
    out.push({tag:e.tagName, w:Math.round(r.width), h:Math.round(r.height),
              x:Math.round(r.x), y:Math.round(r.y),
              src:(e.src||'').slice(0,50)});
  });
  return out;
});
console.log('image surfaces:', JSON.stringify(surf));

const hashes=[];
for(let i=0;i<3;i++){
  const b = surf.length
    ? await page.screenshot({clip:{x:surf[0].x,y:surf[0].y,width:surf[0].w,height:surf[0].h}})
    : await page.screenshot();
  hashes.push(crypto.createHash('md5').update(b).digest('hex').slice(0,12));
  await page.screenshot({path:`${OUT}/live_${i}.png`});
  console.log(`  shot ${i}  hash ${hashes[i]}`);
  await sleep(4000);
}
const uniq=[...new Set(hashes)];
console.log(uniq.length>1 ? 'LIVE: the image changes between shots'
                          : 'STATIC: identical bytes across 12s');
await browser.close();
