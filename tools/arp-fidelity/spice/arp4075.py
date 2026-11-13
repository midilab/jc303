"""SPICE netlist generator for the ARP 4075 VCF in the Odyssey Mk III's wiring, with real transistors.

Values from ARP's 4075 schematic (rev. E, ARPtech) and the Odyssey Mk III VCF page:

- Input: the mixer pot (a VCO) through R123 1M and C24 0.05u into pin 1 (R35 3k3); the limiter Q9 / Q10 (2N5087) on
  R24 56k from +15 V, collectors into stage 1's base nodes (Q9's into the one with stage 1's own feedback, so pin 1
  is non-inverting and pin 2's resonance feedback negative).
- Stages: 2N5087 pairs, base nodes on 220R to the -7 V reference, the previous output into one and its own into the
  other through 12k1;
  tails through 4k7 each from Q14's collector (a 2N3906, base on ground, its emitter fed 4 x the stage current: it
  saturates as its collector nears ground); the collectors into an LM3900 integrator (4700 pF).
- Resonance: the output after C6 10u across R144 100k lin (wiper from ground), R142 22k || C37 220p into pin 2 (R36 3k3).

Unverified guesses / simplifications: the LM3900 as an ideal Norton integrator (its inputs held at -13.4 V, the
integrated current (Ib - Ia) on its capacitor, a stiff output; no slew or sink limit), its swing as clamp diodes on
the capacitor at -1 V / -13.9 V; the -7 V
reference ideal (Q16); generic 2N5087 / 2N3906 models (the National Gummel-Poon parameters); the expo converter's
input side ideal (Q14's emitter current set directly).

Run the scripts in this directory from this directory. Requires ngspice (brew install ngspice)."""
import math, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "ms20-fidelity", "spice"))

VT = 0.025865
R_B, R_SUM, C_ST, R_TAIL = 220.0, 12.1e3, 4700e-12, 4.7e3
K_SUM = (R_B * R_SUM / (R_B + R_SUM)) / R_SUM
V_NEG = 14.0

MODELS = """.model Q2N5087 PNP(Is=6.734f Xti=3 Eg=1.11 Vaf=45.7 Bf=254.1 Ne=1.741 Ise=6.734f Ikf=.1962 Xtb=1.5 Br=5.879
+ Rc=1.67 Cjc=6.2p Mjc=.301 Vjc=.75 Fc=.5 Cje=7.5p Mje=.2593 Vje=.75 Tr=6.075n Tf=627.4p Itf=.7 Vtf=5 Xtf=.6 Rb=10)
.model Q2N3906 PNP(Is=1.41f Xti=3 Eg=1.11 Vaf=18.7 Bf=180.7 Ne=1.5 Ise=0 Ikf=80m Xtb=1.5 Br=4.977 Isc=0 Ikr=0 Rc=2.5
+ Cjc=9.728p Mjc=.5776 Vjc=.75 Fc=.5 Cje=8.063p Mje=.3677 Vje=.75 Tr=33.42n Tf=179.3p Itf=.4 Vtf=4 Xtf=6 Rb=10)
.model DCLMP D (IS=1e-14 N=1)
"""

def tail_for(fc):
    """Per-stage tail current for a stage pole at fc (dfl::ARP4075Filter::stageTail without the prewarp or the knee)."""
    return 2 * math.pi * fc * 2 * VT * C_ST / K_SUM

def netlist(pot=0.0, fc=1000.0, analysis="ac", amp=1.0, freq=100.0, tstop=0.1, tstep=2e-6, kick=0.0,
            fstart=5, fstop=80e3, ppd=100, rail=1.0, mod=False):
    """The circuit; input 'vin' at the mixer pot. analysis 'ac' (1 V AC) or 'tran' (sine amp V at freq, plus a 100 us
    kick pulse of `kick` V at 1 ms). rail as ARP4075Filter::setRail (the +15 V and -14 V supplies sagged together by
    up to 60 %, the reference half the -14 V); mod: the 2k2 mod (R2 / R7 / R12 / R17 4k7 -> 2k2)."""
    sag = 1 - 0.6 * (1 - rail); vneg = V_NEG * sag; vpos = 15.0 * sag; ref = -0.5 * vneg; rt = 2.2e3 if mod else R_TAIL
    it = tail_for(fc); rtop = max((1 - pot) * 100e3, 1e-3); rbot = max(pot * 100e3, 1e-3)
    if analysis == "ac":
        src = f"Vin vin 0 DC 0 AC 1\n.ac dec {ppd} {fstart} {fstop}\n"
    else:
        k = f" PULSE(0 {kick} 1m 1n 1n 100u 10)" if kick else ""
        src = (f"Vs vs 0 SIN(0 {amp} {freq})\nVk vin vs DC 0{k}\n"
               f".tran {tstep} {tstop} 0 {tstep}\n.options method=trap reltol=1e-5 abstol=1e-12 vntol=1e-7\n")
    stages = []
    for k in range(1, 5):
        prev = "" if k == 1 else f"R{k}in y{k-1} ba{k} {R_SUM}\n"
        stages.append(f"""* stage {k}
Rba{k} ba{k} ref {R_B}
Rbb{k} bb{k} ref {R_B}
{prev}R{k}fb y{k} bb{k} {R_SUM}
Rt{k} tc e{k} {rt}
Qa{k} na{k} ba{k} e{k} Q2N5087
Qb{k} nb{k} bb{k} e{k} Q2N5087
Vna{k} na{k} 0 DC {-vneg + 0.6}
Vnb{k} nb{k} 0 DC {-vneg + 0.6}
B{k} 0 s{k} I = I(Vnb{k}) - I(Vna{k})
C{k} s{k} ref {C_ST}
Dh{k} s{k} yh DCLMP
Dl{k} yl s{k} DCLMP
E{k} y{k} 0 s{k} 0 1
""")
    return f"""* ARP 4075 VCF (Odyssey Mk III)
{MODELS}
Vp vp 0 DC {vpos}
Vref ref 0 DC {ref}
Vyh yh 0 DC {-1.0}
Vyl yl 0 DC {-vneg + 0.1}
{src}R123 vin na {1e6}
C24 na p1 0.05u
R35 p1 0 3.3k
R24 vp ein 56k
Q9 q9c p1 ein Q2N5087
Q10 q10c p2 ein Q2N5087
Vq9 q9c bb1 DC 0
Vq10 q10c ba1 DC 0
Ie vp eq {4 * it}
Q14 tc 0 eq Q2N3906
{''.join(stages)}
C6 y4 out 10u
Rtop out w {rtop}
Rbot w 0 {rbot}
R142 w p2 22k
C37 w p2 220p
R36 p2 0 3.3k
.end
"""
