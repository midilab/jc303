import numpy as np,subprocess,os,re,sys,uuid
sys.path.insert(0,'.'); from tb303_vcf import netlist
from multiprocessing import Pool
SUF={'k':1e3,'u':1e-6,'n':1e-9,'m':1e-3,'p':1e-12,'meg':1e6}
def parse(v):
    m=re.fullmatch(r"([0-9.]+(?:e[-+]?[0-9]+)?)(meg|k|u|n|m|p)?",v)
    return float(m.group(1))*SUF.get(m.group(2),1.0) if m else None
FILM={"C26","C24","C19","C18","C27","C25"}                    # ladder + .1u film: +-10%
PAIRS=[("Q22a","Q22b"),("Q12a","Q12b"),("Q21a","Q21b")]       # matched duals share parameters
def perturb(n,rng,tolR=0.05,tolF=0.10,tolE=0.20,sigIS=0.2,sigBF=0.3):
    out=[]; models=[]; shared={}
    for line in n.split("\n"):
        t=line.split()
        if not t or line.startswith(("*",".")) or t[0] in("V12","V5","Vin","Itail","RVt","RVb"): out.append(line); continue
        nm=t[0]
        if nm[0] in "RC" and len(t)==4:
            v=parse(t[3])
            if v is not None:
                tol=tolR if nm[0]=="R" else (tolF if nm in FILM else tolE)
                t[3]="%.6g"%(v*(1+rng.uniform(-tol,tol))); line=" ".join(t)
        elif nm[0]=="Q":
            key=next((p[0] for p in PAIRS if nm in p),nm)
            if key not in shared: shared[key]=(np.exp(rng.normal(0,sigIS)),np.exp(rng.normal(0,sigBF)))
            isf,bff=shared[key]; mname="QN_"+nm; models.append(f".model {mname} NPN (IS={1e-14*isf:.4g} BF={300*bff:.4g} VAF=80 RB=30 RE=1 CJE=2p CJC=1p TF=0.3n)")
            t[-1]=mname; line=" ".join(t)
        out.append(line)
    return "\n".join(out+models)+"\n"
def run_ac(args):
    seed,a,If,tol=args
    rng=np.random.default_rng(seed) if seed is not None else None
    n=netlist(a,If) 
    if rng is not None: n=perturb(n,rng,**tol)
    base="/tmp/_mc_"+uuid.uuid4().hex[:12]
    n=n.replace(".end\n",f".control\nrun\nwrdata {base}.dat vdb(vout) vp(vout)\nquit\n.endc\n.end\n"); open(base+".cir","w").write(n)
    subprocess.run(["ngspice","-b",base+".cir"],capture_output=True,text=True)
    try: d=np.loadtxt(base+".dat"); r=(d[:,0],d[:,1])
    except Exception: r=None
    for ext in (".cir",".dat"):
        try: os.remove(base+ext)
        except OSError: pass
    return r
def metrics(f,db,db0,cut):
    sel=(f>0.4*cut)&(f<2.4*cut); i=np.argmax(np.where(sel,db,-1e9)); fp=f[i]
    boost=db[i]-np.interp(fp,f,db0)
    ref=np.interp(300,f,db0); i10=np.argmax((f>300)&(db0<ref-10)); f10=f[i10]
    return boost,fp/cut,f10
