"""A practical crunch mod on the 303 VCF: a trim voltage (re the 5.33 V bias rail) through 100k into Q12a's base (inL)
offsets the input pair, and a smaller R15 (stock 10k) raises the loop gain. For each pair: the measured base offset,
the output with a 1 mV input (does it rest, or oscillate by itself?) and, with a 3.25 V saw (the real VCO level), the
energy between the note's harmonics. See DUFFING.md (superfreak) / docs/diode-fidelity.md.

  python3 bend.py      (run from this directory; a couple of minutes on 10 cores)"""
import os, sys, subprocess, hashlib, numpy as np
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, '.')
from tb303_vcf import netlist
from native_targets import If_for
import tempfile
C=os.path.join(tempfile.gettempdir(),"diode_bend_cache"); os.makedirs(C,exist_ok=True); F0=65.41
def build(dv, r15, amp, cut=1000.0, tstop=0.8, dt=4e-6):
    n=netlist(1.0, If_for(cut), mode="tran", amp=amp, freq=F0, tran=f".tran {dt} {tstop} 0 {dt}\n.option reltol=1e-4 abstol=1e-13\n")
    n=n.replace("R15 fbr inR 10k", f"R15 fbr inR {r15}")
    # the mod: a trim voltage dv (re the 5.33 V bias rail) through 100k into Q12a's base (inL)
    n=n.replace("Itail tail 0", f"Vtrim vt v5 DC {dv}\nRtrim vt inL 100k\nItail tail 0")
    T=1/F0; vin=[l for l in n.splitlines() if l.startswith("Vin in 0")][0]
    n=n.replace(vin, f"Vin in 0 DC 0 PULSE({-amp} {amp} 0 {T-1e-6} 1e-6 0 {T})")
    return n
def run(n, outs="v(vout) v(inL) v(inR)"):
    k=hashlib.md5((n+outs).encode()).hexdigest()[:14]; d=f"{C}/{k}.dat"
    if not os.path.exists(d):
        m=n.replace(".end\n",f".control\nrun\nwrdata {C}/{k}.tmp {outs}\nquit\n.endc\n.end\n"); open(f"{C}/{k}.cir","w").write(m)
        subprocess.run(["ngspice","-b",f"{C}/{k}.cir"],capture_output=True); os.replace(f"{C}/{k}.tmp",d)
    x=np.loadtxt(d); return x[:,0],x[:,1],x[:,3]-x[:,5]   # t, vout, inL - inR
def win(t,v,a,b): ts=np.arange(a,b,1/44100.); x=np.interp(ts,t,v); return x-x.mean()
def inharm(t,v):
    x=win(t,v,0.3,0.8); X=np.abs(np.fft.rfft(x*np.hanning(len(x))))**2; f=np.fft.rfftfreq(len(x),1/44100.)
    d=np.abs(f/F0-np.round(f/F0))*F0; m=f>20; return 10*np.log10(np.sum(X[m&(d>.25*F0)])/np.sum(X[m])+1e-30)
def job(j):
    dv,r15=j
    t,v,off=run(build(dv,r15,1e-3)); rest=20*np.log10(np.sqrt(np.mean(win(t,v,0.6,0.8)**2))+1e-30)
    m=(t>0.6)&(t<0.8); offmV=1e3*np.mean(off[m])
    t,v,_=run(build(dv,r15,3.25)); return j,offmV,rest,inharm(t,v)
jobs=[(dv,r15) for dv in (0.0,0.6,1.2,1.8) for r15 in ("10k","8.2k","7.5k","6.8k","6.2k")]
with ThreadPoolExecutor(10) as ex: res=list(ex.map(job,jobs))
print("trim V   R15  | Q12 base offset (inL-inR, no note) | 1 mV input: output (rest?) | saw 3.25 V: between harmonics")
for (dv,r15),o,r,ih in res:
    print(f"{dv:5.2f} {r15:>6} | {o:7.1f} mV | {r:7.1f} dBV {'(oscillates)' if r>-40 else ''} | {ih:6.1f} dB")
