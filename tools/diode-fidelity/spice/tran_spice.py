import subprocess,numpy as np,sys,os,hashlib
sys.path.insert(0,'.'); from tb303_vcf import netlist
def tran(a,If,freq,amp,tstop=0.7,tmax=8e-6,node="vout"):
    key=hashlib.md5(repr((a,If,freq,amp,tstop,tmax)).encode()).hexdigest()[:10]; base=f"/tmp/_t_{key}"
    if not os.path.exists(base+".dat"):
        n=netlist(a,If,mode="tran",amp=amp,freq=freq,tran=f".tran {tmax} {tstop} 0 {tmax}\n.option reltol=1e-4 abstol=1e-13\n")
        n=n.replace(".end\n",f".control\nrun\nwrdata {base}.dat v({node})\nquit\n.endc\n.end\n")
        open(base+".cir","w").write(n); r=subprocess.run(["ngspice","-b",base+".cir"],capture_output=True,text=True)
        if not os.path.exists(base+".dat"): print(r.stdout[-1500:]); raise SystemExit
    d=np.loadtxt(base+".dat"); return d[:,0],d[:,1]
def analyze(t,v,freq,tail=0.1,nh=6):
    m=t>t[-1]-tail; tt=t[m]; vv=v[m]-v[m].mean()
    # resample uniformly, integer cycles
    nc=int(tail*freq); T=nc/freq; ts=np.linspace(tt[0],tt[0]+T,20000,endpoint=False); vs=np.interp(ts,tt,vv)
    h=[abs(np.fft.rfft(vs)[k*nc])*2/len(vs) for k in range(1,nh+1)]
    return h
if __name__=="__main__":
    If=21e-6
    for a,freq,lab in ((0.0,200.0,"res off, 200 Hz (passband)"),(0.9,1450.0,"a=0.9, near resonant peak (1450 Hz)")):
        print("\n"+lab+": input amp (V at R20) | fundamental out (mV) | gain dB re smallest | THD % | H2 H3 (% of fund)")
        ref=None
        for amp in (0.03,0.1,0.3,1.0,3.0):
            t,v=tran(a,If,freq,amp); h=analyze(t,v,freq)
            g=20*np.log10(h[0]/amp); ref=g if ref is None else ref
            thd=100*np.sqrt(sum(x*x for x in h[1:]))/h[0]
            print(f"   {amp:5.2f} V | {h[0]*1e3:9.3f} | {g-ref:+6.2f} | {thd:6.2f} | {100*h[1]/h[0]:5.2f} {100*h[2]/h[0]:5.2f}")
