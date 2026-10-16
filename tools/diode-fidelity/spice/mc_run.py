import numpy as np,sys,json; sys.path.insert(0,'.')
from mc import *
WC21=1150.0; CUTS=(500,1500,3000); AS=(0.0,0.8,0.93,0.99,1.0); N=int(sys.argv[1]) if len(sys.argv)>1 else 100
def If_for(c): return 21e-6*c/WC21
jobs=[(s if s>=0 else None,a,If_for(c),{}) for s in range(-1,N) for c in CUTS for a in AS]
res=[run_ac(j) for j in jobs]
out={}; k=0
for s in range(-1,N):
    for c in CUTS:
        data={a:res[k+i] for i,a in enumerate(AS)}; k+=len(AS)
        if any(v is None for v in data.values()): continue
        f,db0=data[0.0]; row={}
        for a in AS[1:]:
            b,fr,f10=metrics(f,data[a][1],db0,c); row[a]=(b,fr)
        # bass lift at 65 Hz re 500 Hz for a=0.93 (loop-open shape vs closed)
        bass=np.interp(65,f,data[0.93][1])-np.interp(500,f,data[0.93][1])
        out[(s,c)]={"boost":{a:row[a][0] for a in row},"fr":{a:row[a][1] for a in row},"f10":metrics(f,data[0.93][1],db0,c)[2],"bass":bass}
json.dump({f"{s}|{c}":v for (s,c),v in out.items()},open("mc_out.json","w"))
nom={c:out[(-1,c)] for c in CUTS}
print(f"N={N} units. nominal | mean +- std | 5th..95th percentile (dB or ratio)")
for c in CUTS:
    print(f"\n cutoff ~{c} Hz  (If={If_for(c)*1e6:.1f} uA)")
    f10=np.array([out[(s,c)]["f10"] for s in range(N) if (s,c) in out]); print(f"  -10dB frequency (cutoff scale):  nom {nom[c]['f10']:6.0f}  mean {f10.mean():6.0f} +-{f10.std():4.0f}  5-95%: {np.percentile(f10,5):.0f}..{np.percentile(f10,95):.0f}  (spread {100*f10.std()/f10.mean():.1f}%)")
    for a in AS[1:]:
        b=np.array([out[(s,c)]["boost"][a] for s in range(N) if (s,c) in out]); fr=np.array([out[(s,c)]["fr"][a] for s in range(N) if (s,c) in out])
        print(f"  a={a:4.2f} boost dB: nom {nom[c]['boost'][a]:5.1f}  mean {b.mean():5.1f} +-{b.std():4.1f}  5-95%: {np.percentile(b,5):5.1f}..{np.percentile(b,95):5.1f} | peak f/cut nom {nom[c]['fr'][a]:.2f} mean {fr.mean():.2f} +-{fr.std():.2f}")
    bs=np.array([out[(s,c)]["bass"] for s in range(N) if (s,c) in out]); print(f"  bass (65 Hz re 500 Hz, a=0.93): nom {nom[c]['bass']:+5.1f} mean {bs.mean():+5.1f} +-{bs.std():.1f} dB")
