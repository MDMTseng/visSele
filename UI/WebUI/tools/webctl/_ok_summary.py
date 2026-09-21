import json, glob, os, sys
sys.stdout.reconfigure(encoding='utf-8')
M={m['name']:m for m in json.load(open('../../../../InspectionCore/Core0_1/data/_ok_manifest.json',encoding='utf-8'))}
J={}
for f in sorted(glob.glob('_ok_stab_*.json')): J.update(json.load(open(f,encoding='utf-8')))
fails=set(x.strip() for x in open('_ok_fail.txt',encoding='utf-8') if x.strip()) if os.path.exists('_ok_fail.txt') else set()
rows=[]; cats={'migrate_failed':[], 'sig360_base_not_located':[], 'sbm_base_not_located':[], 'sbm_locate_miss':[], 'sbm_judges_regressed':[], 'sbm_judges_improved':[], 'sig360_locate_miss':[], 'judges_worse_range':[], 'base_NA_both':[], 'base_NG_both':[], 'sbm_flip_or_note':[]}
def base_counts(v):
    r=J[v]['runs'][0]; j=r['judges']; return (sum(1 for x in j.values() if x['st']==0), sum(1 for x in j.values() if x['st']==-1), sum(1 for x in j.values() if x['st']==-128), r['located'])
for name,m in M.items():
    a=J.get(name); s=J.get(name+'_sbm')
    if name in fails or not s: cats['migrate_failed'].append(name); 
    if not a: continue
    aOK,aNG,aNA,aL=base_counts(name)
    row={'name':name,'src':m['src'],'sig_base':aL,'sig_judges':(aOK,aNG,aNA),'sig_loc':a['located'],'sig_rot':a['rotResMaxDeg'],'sig_shift':a['shiftResMaxPx']}
    if not aL: cats['sig360_base_not_located'].append(name)
    if aL and a['located']!='8/8': cats['sig360_locate_miss'].append('%s %s'%(name,a['located']))
    if s:
        sOK,sNG,sNA,sL=base_counts(name+'_sbm'); row.update({'sbm_base':sL,'sbm_judges':(sOK,sNG,sNA),'sbm_loc':s['located'],'sbm_rot':s['rotResMaxDeg'],'sbm_shift':s['shiftResMaxPx'],'note':s.get('note'),'ms':(a['msAvg'],s['msAvg'])})
        if not sL: cats['sbm_base_not_located'].append(name)
        if sL and s['located']!='8/8': cats['sbm_locate_miss'].append('%s %s'%(name,s['located']))
        if sL and (s['rotResMaxDeg'] or 0)>1.0 or (s['shiftResMaxPx'] or 0)>3.0: cats['sbm_locate_miss'].append('%s rot%.2f shift%.1f'%(name,s['rotResMaxDeg'] or 0,s['shiftResMaxPx'] or 0))
        if aL and sL:
            if sOK<aOK: cats['sbm_judges_regressed'].append('%s %d/%d/%d -> %d/%d/%d'%(name,aOK,aNG,aNA,sOK,sNG,sNA))
            elif sOK>aOK: cats['sbm_judges_improved'].append('%s %d/%d/%d -> %d/%d/%d'%(name,aOK,aNG,aNA,sOK,sNG,sNA))
            sj={j['name']:j for j in s['judges']}; worse=[]
            for j in a['judges']:
                k=sj.get(j['name'],{})
                if j['range'] is None or k.get('range') is None: continue
                if k['range']>2*j['range'] and k['range']>0.01: worse.append('%s %.3f->%.3f'%(j['name'][:10],j['range'],k['range']))
            if worse: cats['judges_worse_range'].append('%s: %s'%(name,'; '.join(worse[:4])))
        if s.get('note'): cats['sbm_flip_or_note'].append('%s %s'%(name,s['note']))
    if aL:
        ra=J[name]['runs'][0]['judges']; rs=J[name+'_sbm']['runs'][0]['judges'] if s and J[name+'_sbm']['runs'][0]['located'] else {}
        na=[k for k,v in ra.items() if v['st']==-128 and rs.get(k,{}).get('st')==-128]; ng=[k for k,v in ra.items() if v['st']==-1 and rs.get(k,{}).get('st')==-1]
        if na: cats['base_NA_both'].append('%s: %s'%(name,', '.join(na)[:80]))
        if ng: cats['base_NG_both'].append('%s: %s'%(name,', '.join(ng)[:80]))
    rows.append(row)
n=len(rows); print('recipes swept:',n,' migrated:',sum(1 for r in rows if 'sbm_base' in r))
print('sig360 base located: %d   SBM base located: %d'%(sum(1 for r in rows if r['sig_base']),sum(1 for r in rows if r.get('sbm_base'))))
print('sweep all-located: sig360 %d, SBM %d'%(sum(1 for r in rows if r['sig_loc']=='8/8'),sum(1 for r in rows if r.get('sbm_loc')=='8/8')))
import statistics
sr=[r['sbm_rot'] for r in rows if r.get('sbm_rot') is not None]; ss=[r['sbm_shift'] for r in rows if r.get('sbm_shift') is not None]
if sr: print('SBM rot residual: median %.3f  p90 %.3f  max %.3f deg'%(statistics.median(sr),sorted(sr)[int(len(sr)*0.9)],max(sr)))
if ss: print('SBM shift residual: median %.2f  p90 %.2f  max %.2f px'%(statistics.median(ss),sorted(ss)[int(len(ss)*0.9)],max(ss)))
ms=[r['ms'] for r in rows if r.get('ms')]
if ms: print('ms sig360 median %d / SBM median %d (max SBM %d)'%(statistics.median([x[0] for x in ms]),statistics.median([x[1] for x in ms]),max(x[1] for x in ms)))
for k,v in cats.items():
    print('\n== %s (%d)'%(k,len(v)))
    for x in v[:60]: print('   ',x)
json.dump({'rows':rows,'cats':cats},open('_ok_summary.json','w',encoding='utf-8'),ensure_ascii=False)
