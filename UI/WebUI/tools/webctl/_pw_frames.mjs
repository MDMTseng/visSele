import { chromium } from 'playwright';
const OUT='C:/Users/w2110/Downloads/pw';
const sleep=(ms)=>new Promise(r=>setTimeout(r,ms));
const browser=await chromium.launch({headless:true});
const page=await (await browser.newContext({viewport:{width:1600,height:950}})).newPage();
await page.goto('http://localhost:8082/',{waitUntil:'domcontentloaded'}); await sleep(6000);
const cT=(l,t='*')=>page.evaluate(({l,t})=>{for(const e of document.querySelectorAll(t)){
  if(e.children.length||!e.offsetParent) continue; if((e.innerText||'').trim()!==l) continue; e.click(); return true;} return false;},{l,t});
const cI=(i,n=0)=>page.evaluate(({i,n})=>{let k=0;for(const b of document.querySelectorAll('button')){
  if(!b.offsetParent||b.disabled) continue; if(!b.querySelector('[class*="'+i+'"]')) continue;
  if(k++===n){b.click();return true;}} return false;},{i,n});
await cT('跳過相機連線','button'); await sleep(1500);
await cI('anticon-folder-open'); await sleep(3000);
await cT('test1.hydef'); await sleep(800);
await page.evaluate(()=>{for(const e of document.querySelectorAll('*')){
  if(e.children.length||!e.offsetParent) continue; if((e.innerText||'').trim()!=='test1.hydef') continue;
  e.dispatchEvent(new MouseEvent('dblclick',{bubbles:true})); return;}});
await sleep(5000);
await cT('11沖壓成形'); await sleep(1200); await cT('全檢'); await sleep(1200);
await cI('anticon-caret-right'); await sleep(9000);
await page.evaluate(()=>{const x=document.querySelector('.ant-drawer-close'); if(x&&x.offsetParent) x.click();});
await sleep(1500);
const st=await page.evaluate(()=>{const b=[...document.querySelectorAll('button')].find(x=>x.querySelector('[class*=caret-right]'));
  if(!b) return 'none'; if(b.disabled) return 'disabled'; b.click(); return 'clicked';});
console.log('start:',st);
await sleep(14000);
const c=await page.evaluate(()=>{const el=document.querySelector('canvas'); const r=el.getBoundingClientRect();
  const w=Math.round(r.width),h=Math.round(r.height);
  return {x:Math.round(r.x+w*0.38),y:Math.round(r.y+h*0.34),width:Math.round(w*0.24),height:Math.round(h*0.32)};});
for(let i=0;i<3;i++){
  await page.screenshot({path:`${OUT}/frame_${i}.png`, clip:c});
  console.log('  frame',i);
  await sleep(1500);
}
await browser.close();
