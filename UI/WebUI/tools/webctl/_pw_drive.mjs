import { chromium } from 'playwright';
const OUT='C:/Users/w2110/Downloads/pw';
const sleep=(ms)=>new Promise(r=>setTimeout(r,ms));
const browser=await chromium.launch({headless:true});
const ctx=await browser.newContext({viewport:{width:1600,height:950}});
const page=await ctx.newPage();
page.on('pageerror',e=>console.log('  [pageerror]',e.message.slice(0,110)));
await page.goto('http://localhost:8082/',{waitUntil:'domcontentloaded'});
await sleep(6000);
const clickText=async(label,tag='*')=>{
  const ok=await page.evaluate(({label,tag})=>{
    for(const e of document.querySelectorAll(tag)){
      const t=(e.innerText||'').trim();
      if(t!==label||e.children.length||!e.offsetParent) continue;
      e.click(); return true; } return false; },{label,tag});
  console.log(`  "${label}" -> ${ok?'ok':'NOT FOUND'}`); return ok; };
const clickIcon=async(icon)=>{
  const ok=await page.evaluate((icon)=>{ for(const b of document.querySelectorAll('button')){
    if(!b.offsetParent||b.disabled) continue;
    if(b.querySelector('[class*="'+icon+'"]')){b.click();return true;}} return false; },icon);
  console.log(`  ${icon} -> ${ok?'ok':'no/disabled'}`); return ok; };

await clickText('跳過相機連線','button'); await sleep(1500);
await clickIcon('anticon-folder-open'); await sleep(3000);
await clickText('test1.hydef'); await sleep(900);
await page.evaluate(()=>{for(const e of document.querySelectorAll('*')){
  if((e.innerText||'').trim()==='test1.hydef'&&!e.children.length&&e.offsetParent){
    e.dispatchEvent(new MouseEvent('dblclick',{bubbles:true}));return;}}});
await sleep(5000);
await clickText('11沖壓成形'); await sleep(1200);
await clickText('全檢'); await sleep(1200);
console.log('pressing play');
await clickIcon('anticon-caret-right');
await sleep(9000);
await page.screenshot({path:OUT+'/07_insp.png'});
console.log('shot -> 07_insp.png');
const btns=await page.$$eval('button',bs=>bs.map((b,i)=>({i,
  t:(b.innerText||'').trim().replace(/\s+/g,' ').slice(0,16),
  icon:((b.querySelector('[class*=anticon]')||{}).className||'').replace('anticon ',''),
  dis:b.disabled})).filter(x=>(x.t||x.icon)));
console.log('buttons:',JSON.stringify(btns).slice(0,900));
await browser.close();
