import subprocess,numpy as np,sys,os,hashlib
sys.path.insert(0,'.'); import tb303_vcf; from tb303_vcf import netlist
from tran_spice import analyze
VT=0.02585
Q21='Q20 e21 nt e20 QN\nR8  e20 0 2.2k\nQ21a c21a b21a e21 QN\nQ21b c21b b21b e21 QN\n'
Q21LIN=f'IA c21a 0 DC 0.25m\nIB c21b 0 DC 0.25m\nGA c21a 0 b21a b21b {0.5e-3/(4*VT):.6g}\nGB c21b 0 b21b b21a {0.5e-3/(4*VT):.6g}\n'
Q12='Q12a L3 inL tail QN\nQ12b R3 inR tail QN\nItail tail 0 DC {If}\n'
def Q12B(kind,If):
    f="V(inL,inR)/(2*%g)"%VT; g=f"tanh({f})" if kind=="tanh" else f
    return f"B12a L3 0 I = {If/2:.6g}*(1 + {g})\nB12b R3 0 I = {If/2:.6g}*(1 - {g})\n"
def variant(name,a,If,amp,freq):
    n=netlist(a,If,mode="tran",amp=amp,freq=freq,tran=".tran 8e-6 0.7 0 8e-6\n.option reltol=1e-4 abstol=1e-13\n")
    if "lin21" in name: assert Q21 in n; n=n.replace(Q21,Q21LIN)
    if "lin12" in name: assert Q12.replace("{If}",str(If)) in n or "Itail tail 0 DC" in n; n=n.replace("Q12a L3 inL tail QN\nQ12b R3 inR tail QN\n"+f"Itail tail 0 DC {If}\n",Q12B("lin",If))
    if "tanh12" in name: n=n.replace("Q12a L3 inL tail QN\nQ12b R3 inR tail QN\n"+f"Itail tail 0 DC {If}\n",Q12B("tanh",If))
    key=hashlib.md5((name+n).encode()).hexdigest()[:10]; base=f"/tmp/_a_{key}"
    if not os.path.exists(base+".dat"):
        n=n.replace(".end\n",f".control\nrun\nwrdata {base}.dat v(vout)\nquit\n.endc\n.end\n"); open(base+".cir","w").write(n)
        r=subprocess.run(["ngspice","-b",base+".cir"],capture_output=True,text=True)
        if not os.path.exists(base+".dat"): print(name,"FAILED",r.stdout[-800:]); return None
    d=np.loadtxt(base+".dat"); return analyze(d[:,0],d[:,1],freq)
If=21e-6
variants=["base","lin21","lin12","lin21+lin12","tanh12"]
for cond,a,freq in (("passband a=0 @200Hz",0.0,200.0),("peak a=0.9 @1450Hz",0.9,1450.0)):
    print(f"\n{cond}:  comp dB re 0.1 V  /  THD %   at input 0.3, 1, 3 V")
    for v in variants:
        h0=variant(v,a,If,0.1,freq)
        if h0 is None: continue
        g0=20*np.log10(h0[0]/0.1); row=[]
        for amp in (0.3,1.0,3.0):
            h=variant(v,a,If,amp,freq); g=20*np.log10(h[0]/amp)-g0; thd=100*np.sqrt(sum(x*x for x in h[1:]))/h[0]; row.append(f"{g:+6.2f}/{thd:5.2f}")
        print(f"  {v:12s} "+"   ".join(row))
