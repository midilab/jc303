"""Varactor Duffing on a 303 ladder: the ladder scaled to 1/10 (capacitors and current, so the cutoff and voltages
stay), then a back-to-back varactor pair across each capacitor (reverse biased from vdd through 10M), abrupt (M 0.5)
or hyperabrupt (M 1.5). Steady sines at 0.03-6.5 V, pot 0.95, cutoff 1350 Hz: the resonant peak vs level, against
the stock and the scaled ladder without varactors.

  python3 varactor.py      (run from this directory; several minutes on 10 cores)"""
import os, sys, subprocess, hashlib, numpy as np
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, '.')
from tb303_vcf import netlist
from native_targets import If_for
import tempfile
C=os.path.join(tempfile.gettempdir(),"diode_varactor_cache"); os.makedirs(C,exist_ok=True)
CAPS=[("C26","L0","R0",33e-9),("C24","L1","R1",33e-9),("C19","L2","R2",33e-9),("C18","L3","R3",18e-9)]
def build(cfg, f, amp, pot=0.95, cut=1350.0, N=10.0, dt=None):
    dt=dt or min(4e-6,1/f/200)
    scale = N if cfg else 1.0
    n=netlist(pot, If_for(cut)/scale, mode="tran", amp=amp, freq=f, tran=f".tran {dt} 0.4 0 {dt}\n.option reltol=1e-4 abstol=1e-13\n")
    for name,a,b,c in CAPS:
        old=f"{name} {a} {b} {int(round(c*1e9))}n"; assert old in n, old
        new=f"{name} {a} {b} {c/scale:.4g}"
        if cfg and cfg!="none":
            cjo,m=cfg
            new+=f"\nDVa{name} {a} K{name} DV\nDVb{name} {b} K{name} DV\nRV{name} K{name} vdd 10meg"
        n=n.replace(old,new)
    if cfg and cfg!="none":
        n=n.replace(".model QN NPN", f".model DV D(IS=1e-15 CJO={cfg[0]} VJ=0.7 M={cfg[1]} RS=1)\n.model QN NPN",1)
    return n
def run(n):
    k=hashlib.md5(n.encode()).hexdigest()[:14]; d=f"{C}/{k}.dat"
    if not os.path.exists(d):
        m=n.replace(".end\n",f".control\nrun\nwrdata {C}/{k}.tmp v(vout)\nquit\n.endc\n.end\n"); open(f"{C}/{k}.cir","w").write(m)
        r=subprocess.run(["ngspice","-b",f"{C}/{k}.cir"],capture_output=True,text=True)
        if not os.path.exists(f"{C}/{k}.tmp"): print(r.stdout[-600:]); return None
        os.replace(f"{C}/{k}.tmp",d)
    x=np.loadtxt(d); return x[:,0],x[:,1]
def point(cfg,f,amp):
    r=run(build(cfg,f,amp))
    if r is None: return np.nan
    t,v=r; nc=int(0.1*f); T=nc/f; ts=np.linspace(0.4-T,0.4,1<<14,endpoint=False); x=np.interp(ts,t,v); x-=x.mean()
    return abs(np.fft.rfft(x)[nc])*2/len(x)/amp
cut=1350.0; fs=np.geomspace(cut*0.55,cut*1.4,45); amps=(0.03,1.0,3.25,6.5)
CFGS=[("stock ladder",None),("scaled 1/10, no varactors","none"),("abrupt varactors M=0.5, CJO 2n","a"),("hyperabrupt M=1.5, CJO 2n","h")]
P={"a":(2e-9,0.5),"h":(2e-9,1.5),"none":"none",None:None}
jobs=[(c,f,a) for _,c in CFGS for a in amps for f in fs]
with ThreadPoolExecutor(10) as ex: g=list(ex.map(lambda j: point(P[j[0]],j[1],j[2]), jobs))
g=np.array(g).reshape(len(CFGS),len(amps),len(fs))
def peak(row):
    i=int(np.nanargmax(row)); 
    if 0<i<len(row)-1:
        y=20*np.log10(row[i-1:i+2]); x=np.log(fs[i-1:i+2]); d=(y[0]-y[2])/(2*(y[0]-2*y[1]+y[2])); return float(np.exp(x[1]+d*(x[2]-x[1])))
    return fs[i]
print("pot 0.95, cutoff 1350 Hz: resonant peak (Hz) vs sine amplitude")
print("config".ljust(34)+"".join(f"{a:>9.2f} V" for a in amps)+"   shift 0.03->6.5 V")
for k,(name,_) in enumerate(CFGS):
    pk=[peak(g[k,j]) for j in range(len(amps))]
    print(name.ljust(34)+"".join(f"{p:11.0f}" for p in pk)+f"   {1200*np.log2(pk[-1]/pk[0]):+7.0f} cents")
