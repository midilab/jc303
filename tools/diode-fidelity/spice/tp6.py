import subprocess,numpy as np,sys,os,hashlib
sys.path.insert(0,'.'); from tb303_vcf import netlist
def run(a,If,wave,amp,freq=130.8,tstop=0.5):
    n=netlist(a,If,mode="tran",amp=amp,freq=freq,tran=f".tran 8e-6 {tstop} 0 8e-6\n.option reltol=1e-4 abstol=1e-13\n")
    T=1/freq
    if wave=="square": src=f"PULSE({-amp} {amp} 0 1u 1u {T/2} {T})"
    else: src=f"PULSE({-amp} {amp} 0 {T-2e-6} 1u 1u {T})"   # ideal saw: ramp up over the period then reset
    n=n.replace(f"SIN(0 {amp} {freq})",src)
    if wave=="saw":
        n=n.replace(f"PULSE({-amp} {amp} 0 {T-2e-6} 1u 1u {T})",f"PWL(0 {-amp} {T-1e-6} {amp} {T} {-amp} REPEAT)") if False else n
    key=hashlib.md5(n.encode()).hexdigest()[:10]; base=f"/tmp/_p_{key}"
    if not os.path.exists(base+".dat"):
        n=n.replace(".end\n",f".control\nrun\nwrdata {base}.dat v(vout) v(e19)\nquit\n.endc\n.end\n"); open(base+".cir","w").write(n)
        subprocess.run(["ngspice","-b",base+".cir"],capture_output=True,text=True)
    d=np.loadtxt(base+".dat"); t=d[:,0]; v=d[:,1]; m=t>tstop-0.15; return v[m].max()-v[m].min(), d[:,3][m].mean()
print("TP6 (Q19 emitter) peak-to-peak in volts, last 150 ms; real scope: ~0.6 Vpp, 5.5 V centre")
print(" input ampl  | wave    | a=0.6   a=0.8   a=1.0 | (If=21uA ~ 1150 Hz cutoff; 130.8 Hz note)")
for amp,w in ((1.5,"square"),(3.0,"square"),(3.25,"square")):
    r=[run(a,21e-6,w,amp) for a in (0.6,0.8,1.0)]
    print(f" +-{amp:4.2f} V   | {w:7s} | "+"  ".join(f"{x[0]:6.3f}" for x in r)+f"   (DC at e19 {r[0][1]:.2f} V)")
