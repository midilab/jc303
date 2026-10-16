"""SPICE netlist generator for the Oberheim SEM VCF (SEM-1A), with the same device equations as the DSP.

Values from the SEM-1A schematic (VCF section):

- A12 (LM301A) sums the input (R155 220k, C15 0.15u), LP (R138 100k || C18 10p) and its output HP (R139 100k);
  its + input on R141 22k to ground.
- A13 / A15 (CA3080): + input on the R142 / R148 100k, R144 / R150 220R divider from HP / BP, - input on R145 / R151
  220R to ground (the offset trims at null); outputs into C20 / C21 300p to ground, buffered by Q39 / Q40 (2N4302
  source followers, R147 / R153 15k to -15 V): BP and LP.
- Resonance: BP across PS16 RES 50k to ground, the wiper through R157 33k to the C22 node; BP through R159 47k (R158
  10k to ground), D11 / D12 (1N4148) and R156 100k to the same node; C22 0.33u into A12's input.

Devices as the DSP (dfl_SEMFilter.h): A12 as a two-stage op amp (its input stage a tanh slewing the 30 pF
compensation node at 0.5 V / us with 1 MHz unity gain, a leak for the 1e5 DC gain, a softplus clamp 1.5 V inside the supply, a unity buffer);
the CA3080 as Iabc tanh(v / 2 VT) with clamp junctions 1.5 V inside either rail; the 2N4302 as SPICE's square-law
JFET (IDSS 2 mA, VTO -1.5 V: a guess); D11 / D12 with IS 2.52n, N 1.752. This checks the DSP's solver (the node
equations, the RES star, the integration) against SPICE, not the device models against an SEM.

Run the scripts in this directory from this directory. Requires ngspice (brew install ngspice)."""
import math, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "ms20-fidelity", "spice"))

VT = 0.025865
R155, C15, R138, C18, R139 = 220e3, 0.15e-6, 100e3, 10e-12, 100e3
RDIV, RDIVLO, C20 = 100e3, 220.0, 300e-12
R147 = 15e3; R157, C22, RRES = 33e3, 0.33e-6, 50e3
R159, R158, R156 = 47e3, 10e3, 100e3
KDIV = RDIVLO / (RDIV + RDIVLO)
IDSS, VTO = 2e-3, -1.5
OP_GAIN, OP_SLEW, OP_KNEE, OP_CC = 1e5, 0.5e6, 0.05, 30e-12
OP_VLIN = OP_SLEW / (2 * math.pi * 1e6)   # 1 MHz unity gain

def iabc_for(fc):
    """OTA bias for an integrator crossover at fc (dfl::SEMFilter::otaIabc without the prewarp)."""
    return 2 * math.pi * fc * C20 * 2 * VT / KDIV

def netlist(pot=0.0, fc=1000.0, analysis="ac", amp=0.1, freq=100.0, tstop=0.1, tstep=2e-6, kick=0.0,
            fstart=5, fstop=80e3, ppd=100, bias=0.0, rail=1.0):
    """The circuit; input 'vin' at R155. analysis 'ac' (1 V AC) or 'tran' (sine amp V at freq, plus a 100 us kick
    pulse of `kick` V at 1 ms). bias / rail as SEMFilter::setSaturationBias / setRail (5 mV at OTA1's input / 30 %)."""
    vcc = 15.0 * (1 - 0.3 * (1 - rail)); vsw = vcc - 1.5; ib = iabc_for(fc)
    a = 1.0 - pot   # the wiper's position from the bottom (RES up = toward ground)
    rup = max((1 - a) * RRES, 1.0); rdn = max(a * RRES, 1.0)
    off = -bias * 5e-3 / (2 * VT)   # OTA1's input offset in 2 VT units (BIAS_MAX_V = 5 mV, LP below ground)
    if analysis == "ac":
        src = f"Vin vin 0 DC 0 AC 1\n.ac dec {ppd} {fstart} {fstop}\n"
    else:
        k = f" PULSE(0 {kick} 1m 1n 1n 100u 10)" if kick else ""
        src = (f"Vs vs 0 SIN(0 {amp} {freq})\nVk vin vs DC 0{k}\n"
               f".tran {tstep} {tstop} 0 {tstep}\n.options method=trap reltol=1e-5 abstol=1e-12 vntol=1e-7\n")
    return f"""* Oberheim SEM VCF
.model J2N4302 NJF (VTO={VTO} BETA={IDSS / VTO ** 2} LAMBDA=0 IS=1e-16)
.model D1N4148W D (IS=2.52n N=1.752)
.model DCLMP D (IS=1e-14 N=1)
.func sp(x) {{x > 30 ? x : ln(1 + exp(x))}}
Vcc vcc 0 DC {vcc}
Vee vee 0 DC {-vcc}
Vhi vhi 0 DC {vcc - 1.5}
Vlo vlo 0 DC {-vcc + 1.5}
{src}R155 vin n1 {R155}
C15 n1 s {C15}
R138 lp s {R138}
C18 lp s {C18}
R139 hp s {R139}
Bc 0 cmp I = {OP_CC * OP_SLEW}*(tanh(-V(s)/{OP_VLIN}) - sp((V(cmp)-{vsw})/{OP_KNEE}) + sp((-V(cmp)-{vsw})/{OP_KNEE})) - {OP_CC * OP_SLEW / (OP_VLIN * OP_GAIN)}*V(cmp)
Ccmp cmp 0 {OP_CC}
Eop hp 0 cmp 0 1
R142 hp p1 {RDIV}
R144 p1 0 {RDIVLO}
Bo1 0 c1 I = {ib}*tanh(V(p1)/(2*{VT}) + {off})
Dh1 c1 vhi DCLMP
Dl1 vlo c1 DCLMP
C20 c1 0 {C20}
J39 vcc c1 bp J2N4302
R147 bp vee {R147}
R148 bp p2 {RDIV}
R150 p2 0 {RDIVLO}
Bo2 0 c2 I = {ib}*tanh(V(p2)/(2*{VT}))
Dh2 c2 vhi DCLMP
Dl2 vlo c2 DCLMP
C21 c2 0 {C20}
J40 vcc c2 lp J2N4302
R153 lp vee {R147}
Rup bp w {rup}
Rdn w 0 {rdn}
R157 w n {R157}
R159 bp d {R159}
R158 d 0 {R158}
D11 d m D1N4148W
D12 m d D1N4148W
R156 m n {R156}
C22 n s {C22}
.end
"""
