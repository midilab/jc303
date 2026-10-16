import subprocess,numpy as np,sys,os,hashlib,re
sys.path.insert(0,'.'); from tb303_vcf import netlist
WC21=1150.0
def If_for(c): return 21e-6*c/WC21
def run(a,cLo,cHi,tau,ampV,f0,total=0.8,tOn=0.05):
    Ilo,Ihi=If_for(cLo),If_for(cHi); T=1/f0
    n=netlist(a,Ilo,mode="tran",amp=ampV,freq=f0,tran=f".tran 8e-6 {total} 0 8e-6\n.option reltol=1e-4 abstol=1e-13\n")
    n=re.sub(r"Itail tail 0 DC [^\n]+",f"Itail tail 0 EXP({Ilo:.6g} {Ihi:.6g} {tOn} 5e-5 {tOn+1e-3} {tau})",n)
    # EXP source decays toward V1 with tau2 after td2: matches cutoff decay (current ~ cutoff)
    n=n.replace(f"SIN(0 {ampV} {f0})",f"PULSE({-ampV} {ampV} 0 {T-1e-6:.9g} 1e-6 0 {T:.9g})")
    key=hashlib.md5(n.encode()).hexdigest()[:10]; base=f"/tmp/_w_{key}"
    if not os.path.exists(base+".dat"):
        n=n.replace(".end\n",f".control\nrun\nwrdata {base}.dat v(vout)\nquit\n.endc\n.end\n"); open(base+".cir","w").write(n)
        r=subprocess.run(["ngspice","-b",base+".cir"],capture_output=True,text=True)
        if not os.path.exists(base+".dat"): print(r.stdout[-1500:]); raise SystemExit
    d=np.loadtxt(base+".dat"); t=d[:,0]; v=d[:,1]; tt=np.arange(0,total,1/44100.0); return np.interp(tt,t,v)
