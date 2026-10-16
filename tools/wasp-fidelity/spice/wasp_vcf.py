"""SPICE netlist generator for the EDP Wasp VCF (and the Doepfer A-124), with the same device equations as the DSP.

Values from Koper, Holters, Esqueda, Parker, "A Virtual Analog Model of the EDP Wasp VCF" (DAFx 2022), Table 1 and
Fig. 2 (the A-124: +12 V, R13 1M; the EDP original: +5 V, R13 100k):

- IC1 (CD4069 inverter) sums the input (R1 27k, C1 0.22u), LP (R2 27k || C2 100p) and its output HP (R3 27k).
- OTA1 / OTA2 (CA3080): + input on R7 / R8 (R11 / R12) 27k across the supply, R6 (R10) 1k between the inputs, the
  previous output through R5 (R9) 47k into the - input; outputs into IC3 / IC5 with C3 / C4 330p across them.
- Resonance: BP through R14 1k, the RES pot 50k (rho from that end to the wiper, the rest to the C7 node) and R4 27k
  into IC1's input; C7 0.22u to ground; BP through R13 to the D1 / D2 node, R15 100k on to the C7 node; D1 / D2
  (1N4148) back to IC1's input.

Devices as the DSP (dfl_WaspFilter.h): the CD4069's MOSFETs as square-law B-sources with the paper's gate-dependent
alpha / VT (eq. 33-34, Table 2), protection diodes to the rails on every inverter pin; the CA3080 as the paper's tanh
with its output stage near the rails (eq. 35-41, Tables 3-4); D1 / D2 with the paper's IS / N, no series resistance.
This checks the DSP's solver (the node equations, the RES star, the trapezoidal integration) against SPICE, not the
device models against a Wasp.

Run the scripts in this directory from this directory. Requires ngspice (brew install ngspice)."""
import math, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "ms20-fidelity", "spice"))

VT = 0.025865
R1 = R2 = R3 = R4 = R7 = R8 = R11 = R12 = 27e3
R5 = R9 = 47e3; R6 = R10 = R14 = 1e3; R15 = 100e3
C1 = C7 = 0.22e-6; C2 = 100e-12; C3 = C4 = 330e-12; RRES = 50e3
OTA_ALPHA, OTA_BETA = 0.8635, 0.9408
K1 = R6 / (R5 + R6 + R7 * R8 / (R7 + R8))
REV = {"edp": dict(vdd=5.0, r13=100e3), "a124": dict(vdd=12.0, r13=1e6)}

def iabc_for(fc):
    """OTA bias for an integrator crossover at fc (dfl::WaspFilter::otaIabc without the prewarp)."""
    return 2 * math.pi * fc * 2 * C3 * VT / (K1 * OTA_ALPHA * OTA_BETA)

SUBCKTS = """.model DPROT D (IS=1e-14 N=1)
.model D1N4148W D (IS=2.52n N=1.752)
.subckt INV in out vdd
Bn out 0 I = mos(max(0.0022 - 1.0471e-4*V(in) + 3.1252e-6*V(in)*V(in), 1e-6), 0.8655 + 0.2212*V(in) + 0.0149*V(in)*V(in), V(in), V(out), 1e-3)
Bp vdd out I = mos(max(-4.9942e-4 - 5.1393e-4*(V(in)-V(vdd)) - 6.7622e-5*(V(in)-V(vdd))*(V(in)-V(vdd)) - 2.7258e-6*(V(in)-V(vdd))*(V(in)-V(vdd))*(V(in)-V(vdd)), 1e-6), 0.6239 - 0.1941*(V(in)-V(vdd)) + 0.0135*(V(in)-V(vdd))*(V(in)-V(vdd)), V(vdd)-V(in), V(vdd)-V(out), 0.06)
D1 in vdd DPROT
D2 0 in DPROT
D3 out vdd DPROT
D4 0 out DPROT
.ends
.subckt OTA inp inn out vdd ib=1u
B1 0 out I = fm(V(inp)-V(inn), {ib}) + (ll(V(inp)-V(inn), {ib}) - fm(V(inp)-V(inn), {ib}))*0.5*(1 - tanh(30.14*(V(out) - 0.6093))) + (hh(V(inp)-V(inn), {ib}) - fm(V(inp)-V(inn), {ib}))*0.5*(1 + tanh(16.0255*(V(out) - (V(vdd) - 1.1895))))
D1 out vdd DPROT
D2 0 out DPROT
.ends
.func mos(a, t, g, d, lam) = { (g - t <= 0) ? 0 : ((d < g - t) ? a*(g - t - 0.5*d)*d*(1 + lam*d) : 0.5*a*(g - t)*(g - t)*(1 + lam*d)) }
.func fm(v, ib) = { 0.8635*ib*tanh(0.9408*v/(2*0.025865)) }
.func ll(v, ib) = { 1e-9 + 0.67938*ib + 1.27987e-7*v + 101.18857*ib*ib + 2.59819*ib*v }
.func hh(v, ib) = { 91.7e-9 - 1.13642*ib - 72e-9*v - 170.11701*ib*ib - 5.4144*ib*v }
"""

def netlist(rev="edp", pot=0.0, fc=1000.0, analysis="ac", amp=0.1, freq=100.0, tstop=0.1, tstep=2e-6, kick=0.0,
            fstart=5, fstop=80e3, ppd=100, bias=0.0, rail=1.0):
    """The circuit; input 'vin' at R1. analysis 'ac' (1 V AC at R1) or 'tran' (sine amp V at freq, plus a 100 us kick
    pulse of `kick` V at 1 ms). bias / rail as WaspFilter::setSaturationBias / setRail (0.15 / 0.3 of the supply)."""
    p = REV[rev]; vdd = p["vdd"] * (1 - 0.3 * (1 - rail)); ib = iabc_for(fc)
    ra = max(pot * RRES, 1e-3); rb = max((1 - pot) * RRES, 1.0)
    # bias: the R7 / R8 Thevenin voltage moves to vdd (0.5 + 0.15 b), its resistance kept
    r78 = R7 * R8 / (R7 + R8); vb1 = vdd * (0.5 + 0.15 * bias)
    if analysis == "ac":
        src = f"Vin vin 0 DC 0 AC 1\n.ac dec {ppd} {fstart} {fstop}\n"
    else:
        k = f" PULSE(0 {kick} 1m 1n 1n 100u 10)" if kick else ""
        src = (f"Vs vs 0 SIN(0 {amp} {freq})\nVk vin vs DC 0{k}\n"
               f".tran {tstep} {tstop} 0 {tstep}\n.options method=trap reltol=1e-5 abstol=1e-12 vntol=1e-7\n")
    return f"""* EDP Wasp VCF ({rev})
{SUBCKTS}
Vdd vdd 0 DC {vdd}
Vb1 vb1 0 DC {vb1}
{src}R1 vin n1 {R1}
C1 n1 s {C1}
R2 lp s {R2}
C2 lp s {C2}
R3 hp s {R3}
X1 s hp vdd INV
R5 hp m1 {R5}
R6 m1 p1 {R6}
R78 vb1 p1 {r78}
XO1 p1 m1 n3 vdd OTA ib={ib}
C3 n3 bp {C3}
X3 n3 bp vdd INV
R9 bp m2 {R9}
R10 m2 p2 {R10}
R11 vdd p2 {R11}
R12 p2 0 {R12}
XO2 p2 m2 n5 vdd OTA ib={ib}
C4 n5 lp {C4}
X5 n5 lp vdd INV
R14 bp e {R14}
Rpa e w {ra}
Rpb w b {rb}
R4 w s {R4}
C7 b 0 {C7}
R13 bp x {p['r13']}
R15 x b {R15}
D1 x s D1N4148W
D2 s x D1N4148W
.end
"""
