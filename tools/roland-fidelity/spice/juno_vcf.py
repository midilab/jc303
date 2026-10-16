"""SPICE netlist generator for the Roland Juno-106 VCF (the 80017A module: IR3109 + BA662 resonance VCA).

The 80017A's internal values are from dksynth's reverse engineering (ModWiggler, "80017A" thread) as used by the
Open80017a replacement (T. Herpoel, https://github.com/ThomHPL/Open80017a, tested in a Juno-106); the topology is the
Juno-6 / 60 voice's (Juno-6 service notes, CPU board IC17 / IC18), with a hotter input and compensation and 270 pF
capacitors. The trim is from the HS-60 / Juno-106S service notes (Jul. 1985): RESO VR26 for 4.8 V p-p at the VCF
output (C4 key, RESO 10); the RES CV is 0..+10 V from the DAC, through R107 27k + VR26 into Tr18.

- IR3109 on +-15 V, its GND pin and the 560R shunts / 270 pF capacitors on ground. Four 68k stages (as in the
  SH-101 / Juno-6, which have 240 pF).
- Input: VCF in (after C56) through 4.7k into stage 1 (stage gain -14.5). (Juno-6: R14 10k, 240 pF.)
- Resonance VCA (BA662, OTA section): - input from Out4 through 100k / 1.5k; + input from VCF in through 24k / 1.5k
  (the resonance compensation; Juno-6: R5 47k); its output current goes into stage 1's input. Here the RES control
  is the BA662's Iabc directly, and RES_IABC_MAX is the trim's calibration (see calibrate()).
- Output: Out4 (VCF out) through C59 to the VCA.

IR3109 stage (AS3109 datasheet, Fig. 2): an undegenerated bipolar pair (Iout = Iabc tanh(Vd / 2 VT)), the other base
on an internal 560R to GND, into the capacitor node; a PMOS source follower (resistor load on stages 1 / 3, current
source on 2 / 4) to Out. Unverified guesses: the follower is an ideal +VSH level shift, the OTA output's compliance
(clamped VCLAMP from the rails), the follower's ceiling (VCC - 0.2 V), no stage offsets, an ideal Iabc (the
exponential converter is not modelled), and the BA662 as an ideal OTA (Iabc tanh(Vd / 2 VT)) with an ideal Iabc.

Run the scripts in this directory from this directory. Requires ngspice (brew install ngspice)."""
import math, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "ms20-fidelity", "spice"))

VT = 0.02585
R_FB, R_SH, C_ST = 68e3, 560.0, 270e-12
K_IN = 1.0 / (2.0 + R_FB / R_SH)   # OTA input node = (v_prev + v_out) * K_IN (relative to ground)
VSH = 1.6        # PMOS follower level shift (guess)
VCLAMP = 0.8     # OTA output compliance from either rail (guess)
R_IN = 4.7e3
R_COMP = 24e3

MODELS = """.model DCLMP D (IS=1e-14 N=1)
"""

def ir3109_subckt(iabc, c=C_ST):
    """One IR3109 stage (pins: in out ref vcc vee) at bias iabc, capacitor c."""
    return f""".subckt IR3109ST in out ref vcc vee m=0 gn=1 bo=0
B1  0 c I = -{iabc}*gn*((1+m/2)*(tanh((V(in)-V(ref))/(2*{VT}) + bo) - tanh(bo)) + m/2)
Dh  c ch DCLMP
Vh  ch vcc DC -{VCLAMP}
Dl  cl c DCLMP
Vl  cl vee DC {VCLAMP}
Cst c ref {c}
Bo  ob 0 V = V(c) + {VSH} - 0.05*ln(1 + exp((V(c) + {VSH} - (V(vcc) - 0.2))/0.05))
Ro  ob out 10
.ends
"""

PATTERN = (1.0, -0.7, 0.5, -0.85)   # Bite's per-stage mirror mismatch (RolandFilter::updateCharacter)

def stage_params(bite=0.0, bias=0.0):
    """Per-stage subcircuit parameters 'm=.. gn=.. bo=..' for Bite (mirror mismatch up to 0.1) and the saturation bias
    (stage 1's input offset bo, its resting current trimmed out), slope renormalised (RolandFilter::updateCharacter)."""
    out = []
    for k, p in enumerate(PATTERN):
        bo = bias if k == 0 else 0.0
        m = 0.1 * bite * p; h = m / 2; t0 = math.tanh(bo) - h / (1 + h)
        out.append(f"m={m} gn={1 / ((1 - t0 * t0) * (1 + h))} bo={bo}")
    return out

def vca_out(node_vd, k):
    """B source: the BA662's tanh on the VCA's input difference node_vd, rescaled to LP-output volts (gain 1 / k)."""
    return f"Bvca nout 0 V = {2 * VT / k}*tanh(({node_vd})/(2*{VT}))\n"

def iabc_for(fc, c=C_ST):
    """Iabc (A) for a stage pole fc (Hz): w = gm K_IN / C, gm = Iabc / (2 VT)."""
    return 2 * math.pi * fc * c / K_IN * 2 * VT

RES_IABC_MAX = 0.2326e-3   # from calibrate(): 4.8 V p-p at Out4 (RolandFilter::RES_IABC_MAX)

def juno_ires(pot):
    """The BA662's Iabc for a RES slider position (as RolandFilter::updatePot)."""
    return RES_IABC_MAX * max(0.0, pot - 0.06) / 0.94

def netlist(ires=0.0, iabc=None, fc=1000.0, analysis="ac", fstart=10, fstop=40e3, ppd=100,
            amp=0.1, freq=200.0, tstop=0.3, tstep=2e-6, kick=0.0, extra="", bite=0.0, clip=0.5, warmth=0.0, bias=0.0):
    """ires: the BA662's Iabc (A); iabc (or fc): the IR3109 stage bias; amp: input volts at the mixer node.
    bite / clip / warmth: RolandFilter's character controls (stage mirror mismatch; the BA662's input dividers
    scaled by 4^(1 - 2 clip), its Iabc inversely; the VCA input driven 4^warmth harder); bias: the saturation bias
    (stage 1's input offset)."""
    iabc = iabc_for(fc) if iabc is None else iabc
    sp = stage_params(bite, bias)
    cs = 4.0 ** (1.0 - 2.0 * clip)
    ires = ires / cs
    kv = 560.0 / (82e3 + 25e3 + 4.7e3 + 560.0)
    if analysis == "ac":
        src = "DC 0 AC 1"
    elif kick:
        src = f"DC 0 PULSE(0 {kick} 1m 1u 1u 100u 10)"
    else:
        src = f"DC 0 SIN(0 {amp} {freq})"
    n = f"""* Juno VCF  ires={ires} iabc={iabc}
{MODELS}
{ir3109_subckt(iabc, C_ST)}
Vp   vp 0 DC 15
Vn   vn 0 DC -15
Vin  na 0 {src}
* input: VCF in through 4.7k into stage 1
R14  na i1 {R_IN}
* stages
R6   i1 0 560
R7   i1 o1 68k
X1   i1 o1 0 vp vn IR3109ST {sp[0]}
R8   o1 i2 68k
R9   i2 0 560
R10  i2 o2 68k
X2   i2 o2 0 vp vn IR3109ST {sp[1]}
R11  o2 i3 68k
R18  i3 0 560
R17  i3 o3 68k
X3   i3 o3 0 vp vn IR3109ST {sp[2]}
R16  o3 i4 68k
R15  i4 0 560
R12  i4 o4 68k
X4   i4 o4 0 vp vn IR3109ST {sp[3]}
* resonance VCA (BA662): - from Out4 (R3 / R1), + from the input (R5 / R2), output current into pin 2
R3   o4 nm 100k
R1   nm 0 1.5k
R5   na np {R_COMP}
R2   np 0 1.5k
Bres 0 i1 I = {ires}*tanh({cs}*(V(np)-V(nm))/(2*{VT}))
* VCA input: C59, VR27 (mid) + R108 into the 80017A's 4.7k / 560R pad; Warmth scales the pad voltage; the BA662's
* tanh, rescaled to VCF-out volts (the DSP's output)
C59  o4 nc59 1u
R108 nc59 npv 107k
R27  npv npq 4.7k
R29  npq 0 560
{vca_out(f"{4.0 ** warmth}*V(npq)", kv)}.option reltol={'1e-5' if analysis != 'tran' else '1e-4'} abstol=1e-13 vntol=1e-7{' method=gear maxord=2' if analysis == 'tran' else ''}
"""
    if analysis == "ac":
        n += f".ac dec {ppd} {fstart} {fstop}\n"
    elif analysis == "op":
        n += ".op\n"
    else:
        n += f".tran {tstep} {tstop} 0 {tstep}\n"
    return n + extra + ".end\n"

def ring_pp(ires, fc=262.0, secs=0.4):
    """Out4 p-p (V) of the steady self-oscillation after a kick."""
    import numpy as np
    from spice_util import tran
    t, v = tran(netlist(ires=ires, fc=fc, analysis="tran", kick=0.1, tstop=secs, tstep=4e-6), "o4")
    return float(np.ptp(v[t > secs - 0.05]))

def calibrate(target=4.8):
    """The BA662 Iabc for a target p-p oscillation (VR26's adjustment: C4 key, 4.8 V p-p), by bisection."""
    lo, hi = 0.2e-3, 3e-3
    for _ in range(18):
        m = 0.5 * (lo + hi)
        (lo, hi) = (m, hi) if ring_pp(m) < target else (lo, m)
    return 0.5 * (lo + hi)

if __name__ == "__main__":
    print(netlist())
