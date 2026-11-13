"""SPICE netlist generator for the TB-303 VCF (Roland service-manual VCF page + Stinchcombe analysis).

Run the scripts in this directory from this directory (they import each other). Requires ngspice (brew install ngspice).
The nominal netlist is also checked in as docs/tb303-vcf.cir. Assumptions are listed in its header."""
# TB-303 VCF netlist generator (Roland service-manual VCF + Stinchcombe's analysis). Values from the schematics.
def netlist(a=0.5, If=21e-6, mode="ac", fstart=1, fstop=20e3, tran=None, amp=0.1, freq=110.0, extra="",
            bite=0.0, sharp=0.5, warm=0.0):
    """bite / sharp / warm: the diode filter's character controls (DiodeLadderFilter::setBite etc.; defaults = the
    circuit, the netlist unchanged). bite: Q12a / Q21a saturation currents x exp(0.4 bite) / exp(-0.5 bite); sharp > 0.5:
    Q12 emitters degenerated, RE = 2 Vt rho / If with rho = 4 (2 sharp - 1), the input and the feedback raised by 1 + rho
    (the feedback through an ideal gain, R15's load kept by a dummy); warm: Q21 with NF = 1 / w, BF x w (its input
    impedance kept) and R1 / R2 = 22k / w, w = 4^warm (input x w, output / w). Clip needs no netlist change: the model
    is homogeneous (y(x; clip) = s y(x / s)), so it is checked by that identity."""
    import math
    rho = 4.0 * max(0.0, 2.0 * sharp - 1.0); cdeg = 1.0 + rho; w = 4.0 ** warm
    if mode != "ac": amp = amp * cdeg
    n = f"""* TB-303 VCF  a={a} If={If}
.model QN NPN (IS=1e-14 BF=300 VAF=80 RB=30 RE=1 CJE=2p CJC=1p TF=0.3n)
.model QN12A NPN (IS={1e-14 * math.exp(0.4 * bite)} BF=300 VAF=80 RB=30 RE=1 CJE=2p CJC=1p TF=0.3n)
.model QN21A NPN (IS={1e-14 * math.exp(-0.5 * bite)} NF={1 / w} BF={300 * w} VAF=80 RB=30 RE=1 CJE=2p CJC=1p TF=0.3n)
.model QN21B NPN (IS=1e-14 NF={1 / w} BF={300 * w} VAF=80 RB=30 RE=1 CJE=2p CJC=1p TF=0.3n)
V12 vdd 0 DC 12
V5  v5  0 DC 5.3333
R95 vdd vcc 100
C28 vcc 0 47u
* ladder top bias + top pair (diode-connected pair, common bias)
R115 vcc nb 10k
C30  nb 0 10u
Q22a nb nb L0 QN
Q22b nb nb R0 QN
* ladder: diode-connected NPNs, caps between arms
Q16 L0 L0 L1 QN
Q23 R0 R0 R1 QN
Q14 L1 L1 L2 QN
Q17 R1 R1 R2 QN
Q13 L2 L2 L3 QN
Q15 R2 R2 R3 QN
C26 L0 R0 33n
C24 L1 R1 33n
C19 L2 R2 33n
C18 L3 R3 18n
* input pair Q12 + tail current (replaces the exponential converter)
{"Q12a L3 inL tail QN12A" if rho == 0 else f"Q12a L3 inL e12a QN12A\nRe12a e12a tail {2 * 0.025865 * rho / If}"}
{"Q12b R3 inR tail QN" if rho == 0 else f"Q12b R3 inR e12b QN\nRe12b e12b tail {2 * 0.025865 * rho / If}"}
Itail tail 0 DC {If}
* input bias / coupling
R17 v5 nin 2.2k
C16 nin 0 10u
R19 nin inL 2.2k
R16 nin inR 2.2k
C15 inL src 1u
R20 src in 220k
* output diff pair Q21 (taps the two top rung nodes), tail Q20, bias
C27 L0 b21a 0.1u
C25 R0 b21b 0.1u
R113 b21a nbq 100k
R114 b21b nbq 100k
R9  v5 nbq 10k
C29 nbq 0 1u
R6  nbq nt 10k
R7  nt 0 10k
Q20 e21 nt e20 QN
R8  e20 0 2.2k
Q21a c21a b21a e21 QN21A
Q21b c21b b21b e21 QN21B
R1  vcc c21a {22e3 / w}
R2  vcc c21b {22e3 / w}
Q19 vcc c21a e19 QN
R96 e19 0 10k
C14 e19 vout 1u
* resonance pot (linear, wiper position a), loaded by C6/R11/Q18
RVt vout wip {{(1-{a})*50k + 1}}
RVb wip 0 {{{a}*50k + 1}}
C6  wip qb0 1u
R11 qb0 qb 2.2k
Q18 vcc qb e18 QN
R12 qb qbias 100k
R13 v5 qbias 100k
C7  qbias 0 1u
R14 e18 0 10k
C8  e18 fbr 1u
{"R15 fbr inR 10k" if rho == 0 else f"R15d fbr nin 12.2k\nE15 fbx nin fbr nin {cdeg}\nR15 fbx inR 10k"}
* tight tolerances: with the defaults the operating point of the input pair Q12 is sometimes wrong (the boost
* jumped by ~3 dB between neighbouring tail currents, e.g. 22.3 -> 22.8 uA), see docs/diode-fidelity.md
.option reltol=1e-6 abstol=1e-15 vntol=1e-9 gmin=1e-15
.nodeset v(nb)=11.5 v(L0)=10.95 v(R0)=10.95 v(L1)=10.4 v(R1)=10.4 v(L2)=9.85 v(R2)=9.85 v(L3)=9.3 v(R3)=9.3 v(tail)=4.7 v(inL)=5.3 v(inR)=5.3
"""
    if mode == "ac":
        n += f"Vin in 0 DC 0 AC {cdeg}\n.ac dec 60 %g %g\n" % (fstart, fstop)
    else:
        n += f"Vin in 0 DC 0 SIN(0 {amp} {freq})\n"
        n += tran
    return n + extra + ".end\n"
