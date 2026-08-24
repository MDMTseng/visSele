// Which parts of the picture actually change?
//
// Guessing a clip and hashing it has now been wrong twice: once because the
// clip included the machine panel (its counters tick, so everything "changed"),
// once because the clip landed on the page background outside the image (so
// nothing ever changed). Both produced confident, opposite, wrong answers.
//
// So stop guessing coordinates. Read the canvas pixels themselves, checksum a
// grid of tiles, and print WHICH tiles move. The overlay occupies a few tiles;
// the photograph occupies many. If only the overlay tiles move, the frame is
// stale, and the grid shows it rather than asserting it.
import { chromium } from 'playwright';
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
console.log('machine start:',st);
await sleep(15000);

const grid = () => page.evaluate(() => {
  const el=document.querySelector('canvas'); if(!el) return null;
  const ctx=el.getContext('2d'); if(!ctx) return 'no2d';
  const W=el.width,H=el.height,N=8,out=[];
  for(let ty=0;ty<N;ty++){ const row=[];
    for(let tx=0;tx<N;tx++){
      const d=ctx.getImageData(Math.floor(tx*W/N),Math.floor(ty*H/N),
                               Math.floor(W/N),Math.floor(H/N)).data;
      let s=0; for(let i=0;i<d.length;i+=97) s=(s*31+d[i])>>>0;   // sparse checksum
      row.push(s);
    } out.push(row); }
  return {W,H,out};
});
const g0=await grid();
if(!g0||g0==='no2d'){ console.log('canvas is not 2d -- cannot read pixels:',g0); await browser.close(); process.exit(0); }
console.log('canvas',g0.W+'x'+g0.H);
await sleep(2500);
const g1=await grid();
await sleep(2500);
const g2=await grid();
let changed=0,total=0;
console.log('tiles that changed across 3 samples (X = changed, . = identical):');
for(let y=0;y<8;y++){ let line='  ';
  for(let x=0;x<8;x++){ total++;
    const c=(g0.out[y][x]!==g1.out[y][x])||(g1.out[y][x]!==g2.out[y][x]);
    if(c) changed++; line+= c?'X ':'. '; }
  console.log(line); }
console.log(`  ${changed}/${total} tiles changed`);
await browser.close();
