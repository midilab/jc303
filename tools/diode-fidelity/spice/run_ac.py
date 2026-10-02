import subprocess,numpy as np,sys,os
sys.path.insert(0,'.'); from tb303_vcf import netlist
def ac(a,If,node="vout"):
    base=f"/tmp/_s_{abs(hash((a,If)))%99999}"
    open(base+".cir","w").write(netlist(a,If)+"") 
    ctl=f".control\nrun\nwrdata {base}.dat vdb({node}) vp({node})\nquit\n.endc\n"
    s=netlist(a,If).replace(".end\n",ctl+".end\n"); open(base+".cir","w").write(s)
    r=subprocess.run(["ngspice","-b",base+".cir"],capture_output=True,text=True)
    if not os.path.exists(base+".dat"): print(r.stdout[-2000:],r.stderr[-2000:]); raise SystemExit
    d=np.loadtxt(base+".dat"); return d[:,0],d[:,1],d[:,3] if d.shape[1]>3 else d[:,2]
if __name__=="__main__":
    f,db,ph=ac(0.0,21e-6)
    print("a=0 (no resonance): gain dB at some freqs"); 
    for fr in (10,30,100,300,600,800,1000,2000,4000,8000): print(" %5d Hz %7.2f dB"%(fr,np.interp(fr,f,db)))
