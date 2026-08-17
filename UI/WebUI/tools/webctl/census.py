#!/usr/bin/env python3
"""Census a core log dump by (level, file:line).

The dump prints EVERY line twice -- once in the ring section, once in the
ephemeral section -- so counting the whole file doubles everything. Read only
the ring section.
"""
import re, sys, collections
def read(path):
    lines=open(path,errors='replace').read().split('\n')
    i0=next(i for i,l in enumerate(lines) if l.startswith('--- Ring'))
    i1=next((i for i,l in enumerate(lines) if l.startswith('--- Ephemeral')), len(lines))
    c=collections.Counter();lv=collections.Counter();samp={}
    for l in lines[i0+1:i1]:
        m=re.match(r'\[\s*([\d.]+)\]\[(\w)\]\[([^\]]+)\]\[([^\]]+):(\d+)\s+([^\]]+)\]\s*(.*)',l)
        if not m: continue
        lv[m.group(2)]+=1;k=(m.group(2),m.group(4).strip(),m.group(5))
        c[k]+=1; samp.setdefault(k,m.group(7)[:100])
    return c,lv,samp
if __name__=='__main__':
    c,lv,s=read(sys.argv[1])
    if len(sys.argv)>2:                       # diff mode: census(B) - census(A)
        c0,_,_=read(sys.argv[2]); c=collections.Counter({k:c[k]-c0.get(k,0) for k in c if c[k]-c0.get(k,0)>0})
    tot=sum(c.values()); print(f"TOTAL {tot} lines {dict(lv)}"); acc=0
    for k,n in c.most_common(20):
        acc+=n; print(f"{n:6d} {100*n/tot:5.1f}% cum{100*acc/tot:5.1f}% [{k[0]}] {k[1]}:{k[2]}  {s.get(k,'')}")
