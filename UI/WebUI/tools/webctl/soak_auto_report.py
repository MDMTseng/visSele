# Turn soak_auto.jsonl into the report page.
#
#   python soak_auto_report.py <out.html>
#
# Rebuilt from scratch on every hourly update rather than appended to: the whole
# trace is the argument, and a page assembled in pieces would have to be trusted
# to have assembled them consistently.
import io, json, sys, os, time

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'soak_auto.jsonl')
OUT = sys.argv[1] if len(sys.argv) > 1 else 'soak_auto_report.html'

rows = []
for line in io.open(SRC, encoding='utf-8'):
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get('noreply'):
        continue
    rows.append(r)

if not rows:
    print('no samples yet')
    sys.exit(1)

# ONE CONTIGUOUS RUN, NOT THE WHOLE FILE.
#
# The log is appended to across restarts, and the first version treated it as a
# single trace: with an aborted run and a gap before the real one, it reported
# 6.9 hours and 3.3 parts/s for 1.4 hours at 17.7/s. Every headline number was
# wrong and the x-axis was mostly empty time.
#
# A run boundary is either the counters resetting (re-entering inspection zeroes
# GATE_ACCEPT) or a gap far longer than the sampling period. Earlier runs are
# dropped rather than stitched: they were different configurations of a machine
# that was restarted for a reason.
segs = [[rows[0]]]
for r in rows[1:]:
    prev = segs[-1][-1]
    if r.get('accept', 0) < prev.get('accept', 0) or (r['ts'] - prev['ts']) > 5 * 60000:
        segs.append([r])
    else:
        segs[-1].append(r)
dropped = len(segs) - 1
rows = segs[-1]

t0 = rows[0]['ts']
def mins(r): return (r['ts'] - t0) / 60000.0

# Counters are cumulative; what a reader wants per sample is the rate. Deltas
# against the previous sample, guarded against the counter resets that a
# re-entry into inspection performs.
series = []
prev = None
for r in rows:
    d = {}
    if prev:
        for k in ('accept', 'SEL1', 'SEL3', 'NA', 'SKIP', 'UNANS', 'rej_load',
                  'rej_rate', 'rej_busy'):
            a, b = prev.get(k), r.get(k)
            d[k] = (b - a) if (isinstance(a, int) and isinstance(b, int) and b >= a) else 0
    series.append({
        't': round(mins(r), 2),
        'state': r.get('state'),
        'err': 1 if r.get('err') else 0,
        'camfps': r.get('cam_fps'), 'camstale': 1 if r.get('cam_stale') else 0,
        'sepeff': r.get('min_sep_eff'), 'sep': r.get('min_sep'),
        'proceff': r.get('proc_eff'), 'procadd': r.get('proc_add'),
        'rho': r.get('proc_rho'), 'svc': r.get('proc_svc'),
        'capn': r.get('proc_cap_n') or 0,
        'wait': r.get('waiting') or 0,
        'accept': d.get('accept', 0), 'judged': d.get('SEL1', 0) + d.get('SEL3', 0) + d.get('NA', 0),
        'na': d.get('NA', 0), 'unans': d.get('UNANS', 0), 'skip': d.get('SKIP', 0),
        'rejload': d.get('rej_load', 0), 'rejrate': d.get('rej_rate', 0),
        'consec': r.get('consec_unans') or 0, 'nomatch': r.get('nomatch_consec') or 0,
        'resid': r.get('resid'), 'drift': r.get('drift'), 'valid': r.get('valid'),
        'rebuilds': r.get('rebuilds') or 0, 'rejected': r.get('rejected') or 0,
        'rst': r.get('recal_stealth') or 0, 'rsok': r.get('recal_stealth_ok') or 0,
        'rfb': r.get('recal_fallback') or 0,
        'lat': r.get('lat_avg'), 'latmax': r.get('lat_max'),
        'heap': r.get('heap'),
    })
    # WITHOUT THIS EVERY DELTA IS ZERO. `prev` was declared and never assigned,
    # so `if prev:` never ran: the per-minute feed, verdicts, unanswered, SKIP
    # and throttle-rejection series were all flat zero, and the page read as a
    # perfect run because it was drawing nothing. The headline "無判決 0" was
    # not a measurement, it was the absence of one. Cross-checking the summary
    # against the raw counters is what found it.
    prev = r

last = rows[-1]
first = rows[0]
hours = mins(rows[-1]) / 60.0
tot_accept = (last.get('accept') or 0) - (first.get('accept') or 0)
tot_judged = sum(s['judged'] for s in series)
tot_unans = sum(s['unans'] for s in series)
tot_skip = sum(s['skip'] for s in series)
tot_rejload = sum(s['rejload'] for s in series)
errs = sum(s['err'] for s in series)
states = sorted(set(s['state'] for s in series if s['state'] is not None))
capn = last.get('proc_cap_n') or 0
rebuilds = last.get('rebuilds') or 0
rfb = last.get('recal_fallback') or 0
rst = last.get('recal_stealth') or 0
rsok = last.get('recal_stealth_ok') or 0
stale_n = sum(s['camstale'] for s in series)
heapmin = min((s['heap'] for s in series if s['heap']), default=0)
heap0 = next((s['heap'] for s in series if s['heap']), 0)

def fmt(v, d=1):
    return '—' if v is None else (f'{v:.{d}f}' if isinstance(v, float) else str(v))

verdict = [
    ('時長', f'{hours:.1f} h', f'{len(series)} 個取樣,每分鐘一次'),
    ('進料', f'{tot_accept:,}', f'{tot_accept/max(hours,0.01)/3600:.1f} 顆/秒 平均'),
    ('無判決', f'{tot_unans:,}', 'UNANSWERED 累計增量'),
    ('停機', str(errs), 'state ' + '/'.join(str(x) for x in states)),
    ('節流觸頂', str(capn), 'proc_auto_cap_n — 非零代表失控'),
    ('時鐘重建', str(rebuilds), f'stealth recal {rsok}/{rst} · 退回 {rfb}'),
]

DATA = json.dumps(series, separators=(',', ':'))

html = '''<title>八小時自調節實跑</title>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=IBM+Plex+Sans+Condensed:wght@500;600&family=IBM+Plex+Sans:wght@400;500&family=IBM+Plex+Mono:wght@400;500&display=swap">
<style>
  :root{--ground:#fff;--panel:#f2f7f8;--panel2:#e8f0f2;--ink:#16212a;--dim:#5d7078;
        --faint:#8fa2a8;--line:#d8e2e4;--accent:#2f7f96;--accent2:#7fc0cc;--warm:#c25a2e;
        --good:#2f8a4a;--bad:#b8352c;--warn:#c0821f;--grid:#e6eef0;}
  @media (prefers-color-scheme:dark){:root:not([data-theme="light"]){
    --ground:#0f171b;--panel:#16232a;--panel2:#1c2c34;--ink:#dde8ea;--dim:#93a8b0;
    --faint:#6b8189;--line:#26383f;--accent:#67b6cd;--accent2:#3d7f92;--warm:#e08757;
    --good:#5cba76;--bad:#e2705f;--warn:#d8a34a;--grid:#20323a;}}
  :root[data-theme="dark"]{--ground:#0f171b;--panel:#16232a;--panel2:#1c2c34;--ink:#dde8ea;
    --dim:#93a8b0;--faint:#6b8189;--line:#26383f;--accent:#67b6cd;--accent2:#3d7f92;
    --warm:#e08757;--good:#5cba76;--bad:#e2705f;--warn:#d8a34a;--grid:#20323a;}
  *{box-sizing:border-box}
  body{background:var(--ground);color:var(--ink);margin:0;padding:0 20px 72px;line-height:1.65;
       font-family:"IBM Plex Sans","Noto Sans TC",system-ui,sans-serif;-webkit-font-smoothing:antialiased}
  .wrap{max-width:1000px;margin:0 auto;display:flex;flex-direction:column;gap:32px}
  header{padding-top:44px;display:flex;flex-direction:column;gap:10px}
  .eyebrow{font-family:"IBM Plex Mono",monospace;font-size:11.5px;letter-spacing:.14em;
           text-transform:uppercase;color:var(--faint)}
  h1{font-family:"IBM Plex Sans Condensed","Noto Sans TC",sans-serif;font-weight:600;
     font-size:clamp(28px,4.4vw,42px);line-height:1.12;margin:0;text-wrap:balance}
  h2{font-family:"IBM Plex Sans Condensed","Noto Sans TC",sans-serif;font-weight:600;
     font-size:21px;margin:0 0 4px}
  .lede{font-size:17px;color:var(--dim);max-width:64ch;margin:0}
  p{margin:0 0 12px;max-width:68ch}
  section{display:flex;flex-direction:column;gap:14px}
  hr.rule{height:1px;background:var(--line);border:0;margin:0}
  .verdict{display:flex;flex-wrap:wrap;gap:1px;background:var(--line);border:1px solid var(--line);
           border-radius:3px;overflow:hidden}
  .vc{background:var(--panel);padding:14px 16px;flex:1 1 150px}
  .vc .k{font-size:11.5px;letter-spacing:.08em;text-transform:uppercase;color:var(--faint);
         font-family:"IBM Plex Mono",monospace}
  .vc .v{font-family:"IBM Plex Mono",monospace;font-size:23px;font-weight:500;
         font-variant-numeric:tabular-nums;margin-top:3px}
  .vc .s{font-size:12.5px;color:var(--dim);margin-top:2px}
  .ok{color:var(--good)}.no{color:var(--bad)}.wn{color:var(--warn)}
  figure{margin:0;display:flex;flex-direction:column;gap:9px}
  .cw{background:var(--panel);border:1px solid var(--line);border-radius:3px;padding:12px 12px 6px;
      overflow-x:auto}
  canvas{display:block;width:100%;height:auto}
  figcaption{font-size:13px;color:var(--dim);max-width:70ch}
  .legend{display:flex;flex-wrap:wrap;gap:16px;font-size:12.5px;color:var(--dim);
          font-family:"IBM Plex Mono",monospace}
  .legend i{display:inline-block;width:15px;height:3px;vertical-align:middle;margin-right:6px;border-radius:2px}
  code,.mono{font-family:"IBM Plex Mono",monospace;font-size:.9em}
  code{background:var(--panel2);padding:1px 5px;border-radius:2px}
  .note{background:var(--panel);border-left:2px solid var(--accent);padding:13px 16px;border-radius:0 3px 3px 0}
  .note.bad{border-left-color:var(--bad)}
  footer{color:var(--faint);font-size:12.5px;font-family:"IBM Plex Mono",monospace;
         border-top:1px solid var(--line);padding-top:16px}
  @media (prefers-reduced-motion:reduce){*{animation:none!important;transition:none!important}}
</style>
<div class="wrap">
<header>
  <div class="eyebrow">uInspESP32 &middot; cam_mode auto + proc_mode auto &middot; __STAMP__</div>
  <h1>八小時,兩層都交給機器自己訂</h1>
  <p class="lede">第一層跟著相機的 <code>ResultingFrameRate</code> 走,第二層跟著主機的服務時間走。整段實跑沒有人填任何一個速率。這頁每小時重建一次。</p>
</header>
<div class="verdict">__VERDICT__</div>
<hr class="rule">
<section>
  <h2>兩層的上限,和實際進料</h2>
  <p>青色是第一層(相機)算出的上限,深青是第二層(主機)算出的,暖色是實際放行。兩層取較慢者生效,所以實際進料應該貼著兩條線裡較低的那一條。</p>
  <figure><div class="cw"><canvas id="c1" width="1880" height="620"></canvas></div>
  <div class="legend"><span><i style="background:var(--accent)"></i>相機上限</span>
  <span><i style="background:var(--accent2)"></i>主機上限</span>
  <span><i style="background:var(--warm)"></i>實際放行</span></div>
  <figcaption>顆/秒。第二層若一直在較低的位置,代表主機是瓶頸而不是相機。</figcaption></figure>
</section>
<section>
  <h2>沒有被判定的料</h2>
  <p>這是「不可檢錯 &gt; best effort &gt; 盡量不停機」裡第一條的證據面:判不到的料不致動、回流,而連續累積才會停機。</p>
  <figure><div class="cw"><canvas id="c2" width="1880" height="480"></canvas></div>
  <div class="legend"><span><i style="background:var(--bad)"></i>無判決/分</span>
  <span><i style="background:var(--warn)"></i>SKIP/分</span>
  <span><i style="background:var(--faint)"></i>連續無判決</span></div>
  <figcaption>連續值是即時的,不是累計 &mdash; 它歸零代表機器又答上來了。</figcaption></figure>
</section>
<section>
  <h2>主機的利用率與服務時間</h2>
  <figure><div class="cw"><canvas id="c3" width="1880" height="480"></canvas></div>
  <div class="legend"><span><i style="background:var(--accent)"></i>利用率 %</span>
  <span><i style="background:var(--warm)"></i>服務時間 ms</span>
  <span><i style="background:var(--faint)"></i>佇列</span></div>
  <figcaption>利用率若長期貼近目標而佇列不成長,就是這個迴路在做它該做的事。</figcaption></figure>
</section>
<section>
  <h2>時鐘</h2>
  <figure><div class="cw"><canvas id="c4" width="1880" height="480"></canvas></div>
  <div class="legend"><span><i style="background:var(--accent)"></i>殘差 us</span>
  <span><i style="background:var(--warm)"></i>漂移 us/s</span></div>
  <figcaption>殘差是每次補償的原始差值,不會累積;它的包絡線應該是平的。</figcaption></figure>
</section>
<footer>__FOOTER__</footer>
</div>
<script>
const D = __DATA__;
const cs=getComputedStyle(document.documentElement), C=k=>cs.getPropertyValue('--'+k).trim();
function prep(id){const c=document.getElementById(id),x=c.getContext('2d');
  x.clearRect(0,0,c.width,c.height);x.lineJoin='round';x.lineCap='round';return {x,W:c.width,H:c.height};}
function axes(x,W,H,P,xr,yr,yt,ylab){
  const px=v=>P.l+(v-xr[0])/((xr[1]-xr[0])||1)*(W-P.l-P.r);
  const py=v=>H-P.b-(v-yr[0])/((yr[1]-yr[0])||1)*(H-P.t-P.b);
  x.strokeStyle=C('grid');x.lineWidth=2;x.font='500 24px "IBM Plex Mono", monospace';x.fillStyle=C('faint');
  yt.forEach(v=>{x.beginPath();x.moveTo(P.l,py(v));x.lineTo(W-P.r,py(v));x.stroke();
    x.textAlign='right';x.textBaseline='middle';x.fillText(String(v),P.l-12,py(v));});
  const hrs=Math.ceil(xr[1]/60);
  for(let h=0;h<=hrs;h++){const v=h*60;if(v>xr[1])break;
    x.textAlign='center';x.textBaseline='top';x.fillText(h+'h',px(v),H-P.b+14);}
  x.strokeStyle=C('line');x.lineWidth=2;x.beginPath();
  x.moveTo(P.l,P.t);x.lineTo(P.l,H-P.b);x.lineTo(W-P.r,H-P.b);x.stroke();
  x.textAlign='left';x.textBaseline='top';if(ylab)x.fillText(ylab,P.l-4,P.t-32);
  return {px,py};}
function line(x,pts,col,w,dash){if(!pts.length)return;x.save();x.strokeStyle=col;x.lineWidth=w;
  if(dash)x.setLineDash(dash);x.beginPath();pts.forEach((p,i)=>i?x.lineTo(p[0],p[1]):x.moveTo(p[0],p[1]));
  x.stroke();x.restore();}
const T=D.length?D[D.length-1].t:1;
const hz=v=>v>0?1e6/v:null;
(function(){const {x,W,H}=prep('c1');const P={l:96,r:26,t:44,b:66};
  const A=axes(x,W,H,P,[0,T],[0,80],[0,20,40,60,80],'顆/秒');
  const S=(f,col,w)=>line(x,D.map(d=>[A.px(d.t),A.py(f(d))]).filter(p=>isFinite(p[1])),col,w);
  S(d=>hz(d.sepeff)||0,C('accent'),4);
  S(d=>hz(d.proceff)||0,C('accent2'),4);
  S(d=>d.accept/60,C('warm'),3);})();
(function(){const {x,W,H}=prep('c2');const P={l:96,r:26,t:44,b:66};
  const mx=Math.max(6,...D.map(d=>Math.max(d.unans,d.skip,d.consec)));
  const A=axes(x,W,H,P,[0,T],[0,mx],[0,Math.round(mx/2),mx],'/分');
  line(x,D.map(d=>[A.px(d.t),A.py(d.consec)]),C('faint'),2.5,[7,6]);
  line(x,D.map(d=>[A.px(d.t),A.py(d.skip)]),C('warn'),3);
  line(x,D.map(d=>[A.px(d.t),A.py(d.unans)]),C('bad'),3.5);})();
(function(){const {x,W,H}=prep('c3');const P={l:96,r:26,t:44,b:66};
  const A=axes(x,W,H,P,[0,T],[0,120],[0,40,80,120],'% / ms / 顆');
  line(x,D.map(d=>[A.px(d.t),A.py(d.rho||0)]),C('accent'),3.5);
  line(x,D.map(d=>[A.px(d.t),A.py((d.svc||0)/1000)]),C('warm'),3);
  line(x,D.map(d=>[A.px(d.t),A.py(d.wait||0)]),C('faint'),2.5);})();
(function(){const {x,W,H}=prep('c4');const P={l:96,r:26,t:44,b:66};
  const rs=D.map(d=>Math.abs(d.resid||0));const mx=Math.max(100,...rs);
  const A=axes(x,W,H,P,[0,T],[-mx,mx],[-Math.round(mx),0,Math.round(mx)],'us | us/s');
  line(x,D.map(d=>[A.px(d.t),A.py(d.resid||0)]),C('accent'),2.5);
  line(x,D.map(d=>[A.px(d.t),A.py((d.drift||0)*(mx/100))]),C('warm'),2.5);})();
</script>
'''

vhtml = ''.join(
    f'<div class="vc"><div class="k">{k}</div><div class="v">{v}</div><div class="s">{s}</div></div>'
    for k, v, s in verdict)
foot = ('每分鐘取樣 · 轉盤 10000 · '
        + (f'(捨棄了 {dropped} 段更早的執行) · ' if dropped else '')
        + f'cam_mode auto (相機 {fmt(last.get("cam_fps"))} fps) · proc_mode auto · '
        + f'目標利用率 80% · 相機上限過期 {stale_n} 次 · '
        + f'heap {heap0} -> {heapmin} · 產生於 ' + time.strftime('%Y-%m-%d %H:%M'))
html = (html.replace('__DATA__', DATA)
            .replace('__VERDICT__', vhtml)
            .replace('__STAMP__', time.strftime('%Y-%m-%d') + f' · {hours:.1f}h')
            .replace('__FOOTER__', foot))
io.open(OUT, 'w', encoding='utf-8').write(html)
print(f'{len(series)} samples, {hours:.2f} h -> {OUT}')
