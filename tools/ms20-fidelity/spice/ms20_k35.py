"""SPICE netlist generator for the Korg MS-20 (early, KLM-127 old production) Korg35 low-pass filter.

Korg35 chip internals from Stinchcombe, "A Study of the Korg MS10 & MS20 Filters" (2006), Figure 1 (Korg's own
disclosure of the hybrid's circuit). External values from the MS-20 service manual, KLM-127 (old production), IC10 (LPF):
C29 3.3n, C28 1n, R111 220, R113 5.6k || C30 1n, R117 270k, R116 4.7k, D20/D21, VR22 10kB with R127 2.2k across its top
half, input pad R115 15k / R112 220. (The MS-10 values Stinchcombe uses are identical except the input pad, 12k / 47.)

Unverified guesses (no board measured): the 2SC1623 / 2SK30A / 2SA812 models (2SC1623 BF / BR from the PSpice model
Stinchcombe quotes; the rest generic), Q1's emitter left open (only its base-collector junction is used), an ideal op amp
with 3 MHz GBW and +-13.5 V swing for the 4558, the cutoff set by V4 into R1 (47k) instead of the IC12 exponential
converter, and R127 = 2.2k (unreadable on the scan; the MS-10's R62).

Response modes are injection points, as the real circuit does it: LP drives Q2's collector through the input pad; HP
drives the bottom of C28 (the MS-20 HPF lifts that cap off ground and drives it through 20k / 47); BP drives the bottom of
R111 (where the resonance feedback arrives at C29).

Run the scripts in this directory from this directory. Requires ngspice (brew install ngspice)."""

from spice_util import MODELS

def netlist(a=0.5, vctl=5.0, mode="lp", analysis="ac", fstart=10, fstop=40e3, ppd=100,
            amp=0.1, freq=200.0, tstop=0.3, tstep=2e-6, r127=2.2e3, extra="", bias=0.0, clip=0.5, swap=False):
    """a: VR22 wiper position 0..1 (linear pot, 1 = max resonance); vctl: V4 at R1 (sets the cutoff); mode: lp / bp / hp
    (injection point); amp: input amplitude at the input pad (V, tran). clip: the limiter diodes' IS scaled by
    10^(4 (0.5 - clip)); bias: D21's IS raised by 10^(2 bias), the DSP's diodeIa (MS20Filter::setClipThreshold /
    setSaturationBias; swap: raise D20 instead, the opposite polarity)."""
    isa = 2.52e-9 * 10 ** (4.0 * (0.5 - clip)); isb = isa * 10 ** (2.0 * bias)
    if swap: isa, isb = isb, isa
    src = "DC 0 AC 1" if analysis == "ac" else f"DC 0 SIN(0 {amp} {freq})"
    lp, bp, hp = ("vin", "0", "0") if mode == "lp" else ("0", "vin", "0") if mode == "bp" else ("0", "0", "vin")
    n = f"""* MS-20 Korg35 LPF  a={a} vctl={vctl} mode={mode}
{MODELS}
.model DLA D (IS={isa} N=1.752 RS=0.568 CJO=4p)
.model DLB D (IS={isb} N=1.752 RS=0.568 CJO=4p)
Vcc vcc 0 DC 15
Vee vee 0 DC -15
Vin vin 0 {src}
* exponential control: Q1 base-collector junction steals current from Q2 / Q3 bases (node nA)
R3  vcc nA 470k
Q1  nB nA q1e Q2SC1623
Rq1 q1e 0 1G
Vctl vctl 0 DC {vctl}
R1  vctl nB 47k
R2  nB nC 680
R58 nC 0 220
R55 nC nD 15k
C14 nD 0 10u
R89 nD vee 1k
* LP input pad (R115 / R112)
R115 {lp} nin 15k
R112 nin 0 220
* Sallen-Key: Q2 / Q3 reverse-saturated as current-controlled resistors
Q2  nin nA n1 Q2SC1623
C29 n1 nG 3.3n
Q3  n1 nA n2 Q2SC1623
C28 n2 nH 1n
* HP injection: bottom of C28 through the MS-20 HPF pad (20k / 47)
R100 {hp} nH 20k
R104 nH 0 47
* JFET + PNP compound buffer
J4  nDr n2 nS J2SK30
R4  vcc nDr 10k
Q5  nP nDr vcc Q2SA812
R5  nS nP 820
R6  nS nD 10k
* gain stage x58 with the D20/D21 limiter
XA  nP nN nO OPA
R117 nO nN 270k
D20 nN nO DLA
D21 nO nN DLB
C31 nN nR 10u
R116 nR 0 4.7k
* output coupling and resonance pot VR22 (R127 across the top half)
C32 nO nE 33u
RVt nE nF {{(1-{a})*10k + 1}}
RVb nF 0 {{{a}*10k + 1}}
R127 nE nF {r127}
R113 nF nG 5.6k
C30 nF nG 1n
* R111: feedback arrives at C29; BP injection lifts its bottom
R111 nG nbp 220
Rbp {bp} nbp 1m
.option reltol={'1e-5' if analysis != 'tran' else '1e-4'} abstol=1e-12 vntol=1e-7{' method=gear maxord=2' if analysis == 'tran' else ''}
"""
    if analysis == "ac":
        n += f".ac dec {ppd} {fstart} {fstop}\n"
    elif analysis == "op":
        n += ".op\n"
    else:
        n += f".tran {tstep} {tstop} 0 {tstep}\n"
    return n + extra + ".end\n"

if __name__ == "__main__":
    print(netlist())
