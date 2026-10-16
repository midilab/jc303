import numpy as np,sys,json
sys.path.insert(0,'.'); from run_ac import ac
WC21=1150.0   # effective cutoff (Hz) at If=21uA
def If_for(cut): return 21e-6*cut/WC21
cache={}
def resp(a,cut):
    key=(round(a,4),cut)
    if key not in cache: cache[key]=ac(a,If_for(cut))
    return cache[key]
def boost(a,cut):
    f,db,ph=resp(a,cut); f0,db0,_=resp(0.0,cut)
    sel=(f>0.4*cut)&(f<2.4*cut); i=np.argmax(np.where(sel,db,-1e9)); return db[i]-np.interp(f[i],f0,db0), f[i]/cut
if __name__=="__main__":
    tb820={30:12.1,50:17.2,70:20.5,85:21.9,100:23.2}
    As=np.array([0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,1.0]); b=np.array([boost(a,820)[0] for a in As])
    print("SPICE boost at 820 Hz vs pot a:"," ".join(f"{a:.1f}:{x:.1f}" for a,x in zip(As,b)))
    amap={r:float(np.interp(v,b,As)) for r,v in tb820.items()}
    print("res -> pot position a (match TB boost at 820):",{r:round(a,3) for r,a in amap.items()})
    cuts=(300,500,820,1500,3000); out={}
    print("\nSPICE boost dB (and peak-freq ratio), rows res, cols cutoff",cuts)
    for r,a in amap.items():
        row=[boost(a,c) for c in cuts]; out[r]=[x[0] for x in row]
        print(f" res {r:3d} (a={a:.2f}): "+"  ".join(f"{x[0]:5.1f}({x[1]:.2f})" for x in row))
    with open('targets_spice.txt','w') as f:
        for r in out:
            for c,v in zip(cuts,out[r]): f.write(f"{r} {c} {min(v,32.0):.3f}\n")
