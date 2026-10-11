"""SPICE model of the TB-303 VCA + mixer + line output (everything after the VCF output), and a comparison with
Open303's post-filter coupling filters (allpass 14.008 Hz, highpass2 24.167 Hz, notch 7.5164 Hz / 4.7 oct).

Run from this directory (imports tb303_vcf). Requires ngspice (brew install ngspice) and numpy.
Usage: python3 tb303_vca.py [--op] [--thd] [--rload 47e3] [--vol 1.0]

Values from the Roland TB-303 Service Notes (Feb 19 1982), main-board schematic page 5 (sections VCA, MIXER,
HEADPHONE, VCF), read off the scan at full resolution.

VERIFIED (read off the service-notes schematic):
  VCA input (IC15 pin 3, +in) is a passive sum of TWO taps of the VCF output:
    R121 220k + C21 0.01u in series from the VCF output (C14 side of the resonance pot, node `vout`)
    R122 100k + C22 0.01u in series from the RESONANCE pot wiper (VR4 gang 1, node `wip`, the same wiper that
    drives the resonance feedback via C15/R67 on the schematic = C6/R11 in tb303_vcf.py)
  Input bias: R126 2.2k from 5.333V to node B, C37 10u/16 B->GND, R124 2.2k B->pin3, R125 2.2k B->pin2 (-in).
    So pin 2 is AC-grounded through 2.2k; the 220k/100k vs 2.2k is a ~-40 dB attenuator into the OTA, and the
    0.01u caps make two high-passes (~72 Hz on the vout tap, ~156 Hz on the wiper tap).
  IC15 BA662A "vari-conductance amp" (parts list): OTA (pins 2/3 in, 1 = Iabc, 6 = out) + buffer (7 in, 8 out).
  OTA load at pin 6/7: R127 47k in series with R129 22k, in parallel with R128 220k, both to node T;
    T = R162 2.2k from 5.333V, C40 10u/16 T->GND.
  Output: pin 8 -> C38 1u/50 -> VR8 VOLUME 50k(A) pin 3 (pin 1 GND); wiper -> C59 1u/50 -> R155 47k -> mixer.
  Mixer (inverting transimpedance summer): Q34 NPN, base at 5.333V via R157 10k with C52 10u/16 to GND, emitter =
    summing node; collector R154 100k to the filtered rail (R136 100 + C53 47u/16); R158 10k to the base of Q33
    PNP (emitter on the filtered rail, collector = mixer output, R160 10k to GND); feedback R153 100k output->Q34
    emitter. MIX IN jack J1 -> C58 1u/50 -> R156 100k -> summing node (J1 switch grounds the tip when unplugged).
    Gain from volume wiper = -R153/R155 = -2.13.
  Line output: mixer out -> C56 1u/50 -> R161 1k -> J6 OUTPUT tip, R165 100k tip->GND.
  Headphone amp IC14 LA4140 fed from the mixer output via R159 33k + C51 2.2u/50 (pin 2).
  Cross-checked against the x0xb0x (faithful clone) VCA and HP/MIX build-page parts lists: C21/C22 .01uF film,
  R121 220k, R122 100k, R124/R125/R126/R162 2.2k, R127 47k, R128 220k, R129 22k (fixed), C37/C40 10u, C38 1u,
  R153/R154/R156/R165 100k, R155 47k, R157/R158/R160 10k, R161 1k, C56/C58/C59 1u, VR8 50k log, Q33 2SA733P,
  Q34 2SC536F, IC15 BA662A (or BA6110 on the x0xb0x).
  VCA control: Iabc into pin 1 from Q31 (PNP) collector; Q31 emitter via R131 220k + R133 2.2k from the
    gate/accent node (D35/R137 1k/D34), base on C42 1u/35 which Q32 (R130 100, R132 100) discharges at gate on
    (driven via D36/R134 22k/C41 0.1u) and R123 1.5M recharges -> ~1.5 s exponential decay of Iabc. A second
    current enters pin 1 from the accent sweep circuit via R119 47k / C36 0.033u / R120 22k / D27.
GUESSED / ASSUMED:
  - BA662A internals: modelled as a plain CA3080-style OTA (NPN diff pair, tail = Iabc, ideal current mirrors,
    no linearising diodes) + ideal unity buffer with 100 ohm output resistance. No datasheet at hand.
  - Iabc levels (static): ~45 uA max (11 V across 222k), 20 uA "sustained", 2 uA "low". The response is linear in
    the external network, so the frequency response barely depends on Iabc (checked below).
  - The rail arrow on R136/R154 is taken as +12 V (same symbol as R95 in the VCF, modelled as 12 V there).
  - Volume pot VR8 at full (wiper at top) by default; --vol sets the wiper fraction (linear here, log in reality);
    below full the C38/C59 corners move by < 0.5 dB at 20 Hz.
  - Line-out load RLOAD (default 47k, a typical mixer line input); R165 100k alone (unloaded) gives ~1.6 Hz HP.
  - LA4140 input modelled as 30k to AC ground behind R159/C51 (only loads the mixer output, which is ~0 ohm).
  - Transistor cards: QN = the VCF script's NPN; Q33 uses the 2SA733 card from tools/osc-fidelity (CJC 13p
    sets the mixer's HF rolloff, so treat the HF end as approximate).
  - Wiper load in the standalone netlist: C6 1u + R11 2.2k + ~94k (Q18 follower input), as in tb303_vcf.py.
"""
import argparse, os, subprocess, sys, tempfile
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tb303_vcf import netlist as vcf_netlist  # noqa: E402

OPTS = ".option reltol=1e-6 abstol=1e-15 vntol=1e-9 gmin=1e-15\n"
QN = ".model QN NPN (IS=1e-14 BF=300 VAF=80 RB=30 RE=1 CJE=2p CJC=1p TF=0.3n)\n"
QP = (".model Q2SA733 PNP (IS=55.9f BF=205 NF=1.01201 VAF=135 IKF=462.6m ISE=180f NE=1.65 BR=2.048 NR=1.012 "
      "RB=20 RE=10m RC=1.7 CJE=35p VJE=900m MJE=500m TF=700p XTF=10 VTF=10 ITF=700m CJC=13p VJC=750m MJC=400m "
      "XCJC=500m TR=10n XTB=2.00849 EG=1.11 XTI=3 FC=500m)\n")


def vca_block(iabc=20e-6, vol=1.0, rload=47e3):
    """VCA + mixer + line out, hanging off nodes vout / wip (VCF), vdd (12 V), v5 (5.333 V). Output node: jack."""
    vt, vb = (1 - vol) * 50e3 + 1, vol * 50e3 + 1
    return f"""* ---- VCA (IC15 BA662A) ----
{QP}R121 vout x21 220k
C21  x21 p3 0.01u
R122 wip x22 100k
C22  x22 p3 0.01u
R126 v5 vcab 2.2k
C37  vcab 0 10u
R124 vcab p3 2.2k
R125 vcab p2 2.2k
* OTA: diff pair, tail = Iabc, ideal mirrors: Iout(pin6) = Ic(+in) - Ic(-in)
Vc1  vdd c1 DC 0
Vc2  vdd c2 DC 0
Qo1  c1 p3 tl QN
Qo2  c2 p2 tl QN
Iabc tl 0 DC {iabc}
F1   0 p6 Vc1 1
F2   p6 0 Vc2 1
Cp6  p6 0 5p
R127 p6 x127 47k
R129 x127 vcat 22k
R128 p6 vcat 220k
R162 v5 vcat 2.2k
C40  vcat 0 10u
* buffer pin7 -> pin8
Eb   b8 0 p6 0 1
Rbo  b8 p8 100
C38  p8 volt 1u
RV8t volt volw {vt}
RV8b volw 0 {vb}
C59  volw x155 1u
R155 x155 sE 47k
* ---- MIXER (Q34 NPN common-base input, Q33 PNP, feedback R153) ----
R136 vdd sR 100
C53  sR 0 47u
R154 sR c34 100k
Q34  c34 b34 sE QN
R157 v5 b34 10k
C52  b34 0 10u
R158 c34 b33 10k
Q33  mo b33 sR Q2SA733
R160 mo 0 10k
R153 mo sE 100k
C58  sE x156 1u
R156 x156 0 100k
* headphone amp input (LA4140) load
R159 mo x159 33k
C51  x159 x51 2.2u
Rhp  x51 0 30k
* ---- line out J6 ----
C56  mo x56 1u
R161 x56 jack 1k
R165 jack 0 100k
Rld  jack 0 {rload}
"""


def standalone(a=0.0, iabc=20e-6, vol=1.0, rload=47e3, src="AC 1"):
    """Ideal source at the VCF output (vout); resonance pot + approximate wiper load; then the VCA chain."""
    return f"""* TB-303 VCA + output, driven at vout  a={a} Iabc={iabc}
{QN}V12 vdd 0 DC 12
V5  v5  0 DC 5.3333
Vs  vout 0 DC 0 {src}
RVt vout wip {(1 - a) * 50e3 + 1}
RVb wip 0 {a * 50e3 + 1}
C6w wip xw 1u
R11w xw xw2 2.2k
Rq18 xw2 0 94k
{vca_block(iabc, vol, rload)}{OPTS}"""


def chained(a=0.0, If=21e-6, iabc=20e-6, vol=1.0, rload=47e3):
    """Full VCF (tb303_vcf.py) + this chain; the VCF's Vin has AC 1."""
    blk = vca_block(iabc, vol, rload)
    return vcf_netlist(a=a, If=If, extra=blk + OPTS)


def run(net, analysis, vecs):
    with tempfile.TemporaryDirectory() as d:
        dat = os.path.join(d, "o.dat")
        net = net.replace(".end\n", "")
        net += f"{analysis}\n.control\nrun\nwrdata {dat} {' '.join(vecs)}\nquit\n.endc\n.end\n"
        cir = os.path.join(d, "x.cir")
        open(cir, "w").write(net)
        r = subprocess.run(["ngspice", "-b", cir], capture_output=True, text=True)
        if not os.path.exists(dat):
            print(r.stdout[-3000:], r.stderr[-3000:])
            raise SystemExit("ngspice failed")
        return np.loadtxt(dat), r.stdout


def ac(net, nodes, f0=1, f1=20e3, ppd=60):
    """Complex node voltages. wrdata writes (x, re, im) per vector."""
    d, _ = run(net, f".ac dec {ppd} {f0} {f1}", [f"v({n})" for n in nodes])
    f = d[:, 0]
    return f, [d[:, 3 * i + 1] + 1j * d[:, 3 * i + 2] for i in range(len(nodes))]


def op(net, nodes):
    net2 = net.replace(".end\n", "") + ".control\nop\n" + "".join(f"print v({n})\n" for n in nodes) + \
        "print @qo1[ic] @qo2[ic] @q34[ic] @q33[ic] i(vc1) i(vc2)\nquit\n.endc\n.end\n"
    with tempfile.TemporaryDirectory() as d:
        cir = os.path.join(d, "op.cir")
        open(cir, "w").write(net2)
        r = subprocess.run(["ngspice", "-b", cir], capture_output=True, text=True)
    return "\n".join(l for l in r.stdout.splitlines() if "=" in l)


# ---- Open303 digital chain (exact coefficients from rosic_OnePoleFilter / rosic_BiquadFilter) ----
def _z(f, fs):
    return np.exp(-1j * 2 * np.pi * f / fs)


def o3_hp(f, fc, fs):
    x = np.exp(-2 * np.pi * fc / fs)
    z = _z(f, fs)
    return 0.5 * (1 + x) * (1 - z) / (1 - x * z)


def o3_ap(f, fc, fs):
    t = np.tan(np.pi * fc / fs)
    x = (t - 1) / (t + 1)
    z = _z(f, fs)
    return (x + z) / (1 + x * z)


def o3_notch(f, fc, bw, fs):
    w = 2 * np.pi * fc / fs
    s, c = np.sin(w), np.cos(w)
    al = s * np.sinh(0.5 * np.log(2) * bw * w / s)
    sc = 1 / (1 + al)
    b0, b1, b2, a1, a2 = sc, -2 * c * sc, sc, 2 * c * sc, (al - 1) * sc
    z = _z(f, fs)
    return (b0 + b1 * z + b2 * z * z) / (1 - a1 * z - a2 * z * z)


def o3_post(f, fs=44100.0):
    return o3_ap(f, 14.008, fs) * o3_hp(f, 24.167, fs) * o3_notch(f, 7.5164, 4.7, fs)


def o3_pre(f, fs=44100.0):
    return o3_hp(f, 44.486, 4 * fs)


# Electrolytic coupling/bypass caps of tb303_vcf.py; the ladder caps (C26/C24/C19/C18) are left alone.
VCF_BIG_CAPS = ("C15", "C16", "C27", "C25", "C29", "C30", "C14", "C28", "C6", "C7", "C8")


def big_caps(net):
    out = []
    for line in net.splitlines():
        t = line.split()
        if t and t[0] in VCF_BIG_CAPS:
            line = " ".join(t[:-1] + ["1"])
        out.append(line)
    return "\n".join(out) + "\n"


def proposed_post(f, a=0.0, fs=44100.0):
    """Proposed replacement for allpass*highpass2*notch: three one-pole HPs (Open303's HIGHPASS form).
    88 Hz + 55 Hz * resonance-pot position (the VCA input taps vout via 220k/10n and the pot wiper via 100k/10n),
    plus two at 9.7 Hz (VCF output coupling, C38/C59/C56 and the line-out load). Fit in 10 Hz - 20 kHz."""
    return o3_hp(f, 88.0 + 55.0 * a, fs) * o3_hp(f, 9.7, fs) * o3_hp(f, 9.7, fs)


def norm(f, H, fref=1000.0):
    i = np.argmin(abs(f - fref))
    return H / H[i]


def dbph(H):
    return 20 * np.log10(abs(H)), np.degrees(np.angle(H))


def wrap(d):
    return (d + 180) % 360 - 180


FREQS = (2, 5, 10, 15, 20, 30, 50, 100, 200, 500, 1000, 5000, 10000, 20000)


def interp_c(f, H, fr):
    return np.interp(fr, f, H.real) + 1j * np.interp(fr, f, H.imag)


def table(f, cols, names):
    print("   Hz  " + "".join(f"| {n:>17s} " for n in names))
    for fr in FREQS:
        row = f"{fr:6g} "
        for H in cols:
            h = interp_c(f, H, fr)
            row += f"| {20*np.log10(abs(h)):7.2f} dB {np.degrees(np.angle(h)):6.1f} "
        print(row)


def err(f, A, B, lo=20, hi=20e3):
    m = (f >= lo) & (f <= hi)
    r = A[m] / B[m]
    return np.max(abs(20 * np.log10(abs(r)))), np.max(abs(np.degrees(np.angle(r))))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--op", action="store_true", help="print operating point")
    ap.add_argument("--thd", action="store_true", help="transient THD at the VCA")
    ap.add_argument("--rload", type=float, default=47e3)
    ap.add_argument("--vol", type=float, default=1.0)
    ap.add_argument("--If", type=float, default=400e-6,
                    help="VCF tail current for the chained run (~4 kHz cutoff; >=500uA the netlist's bias collapses)")
    args = ap.parse_args()

    if args.op:
        print(op(standalone(iabc=20e-6, rload=args.rload),
                 ["p3", "p2", "vcab", "tl", "p6", "vcat", "sE", "b34", "c34", "b33", "sR", "mo", "jack"]))

    # 1) post-VCF chain, vout -> jack, Iabc dependence and resonance-pot dependence
    res = {}
    for a in (0.0, 0.5, 1.0):
        for ia in (45e-6, 20e-6, 2e-6):
            f, (vo, j) = ac(standalone(a=a, iabc=ia, vol=args.vol, rload=args.rload), ["vout", "jack"], 1, 200e3)
            res[(a, ia)] = (f, j / vo)
    f = res[(0.0, 20e-6)][0]
    print("\n== vout -> jack, absolute gain at 1 kHz (dB) ==")
    for k, (ff, H) in res.items():
        rel = 20 * np.log10(abs(H / interp_c(ff, H, 1000)))
        hf = ff[(ff > 1000) & (rel < -3)]
        print(f"  a={k[0]:.1f} Iabc={k[1]*1e6:5.1f}uA: {20*np.log10(abs(interp_c(ff, H, 1000))):7.2f} dB"
              f"   at 20 kHz {np.interp(20e3, ff, rel):5.2f} dB, -3 dB HF corner "
              + (f"{hf[0]/1e3:.0f} kHz" if len(hf) else "> 200 kHz"))
    base = norm(f, res[(0.0, 20e-6)][1])
    for ia in (45e-6, 2e-6):
        e = err(f, norm(f, res[(0.0, ia)][1]), base, 1, 20e3)
        print(f"  shape change vs 20uA at Iabc={ia*1e6:.0f}uA (1-20k Hz): {e[0]:.3f} dB, {e[1]:.2f} deg")

    o3 = o3_post(f)
    print("\n== post chain normalised at 1 kHz: SPICE (a=0, 0.5, 1) vs Open303 allpass*hp2*notch ==")
    cols = [norm(f, res[(a, 20e-6)][1]) for a in (0.0, 0.5, 1.0)] + [norm(f, o3)]
    table(f, cols, ["SPICE a=0", "SPICE a=0.5", "SPICE a=1", "Open303 post"])
    for a in (0.0, 0.5, 1.0):
        e = err(f, cols[int(a * 2)], cols[3])
        print(f"  max |err| 20 Hz-20 kHz, a={a}: {e[0]:.2f} dB, {e[1]:.1f} deg")

    # 2) whole chain, coupling only: (VCF in -> vout with real caps) / (same with every coupling/bypass
    #    electrolytic at 1 F) isolates the VCF's own coupling network from its lowpass; times vout -> jack.
    vcfn = vcf_netlist(a=0.0, If=args.If, extra=OPTS)
    fc, (vi, vo) = ac(vcfn, ["in", "vout"], 1, 20e3)
    _, (vi2, vo2) = ac(big_caps(vcfn), ["in", "vout"], 1, 20e3)
    hc = norm(fc, (vo / vi) / (vo2 / vi2))
    _, (svo, sjk) = ac(standalone(a=0.0, iabc=20e-6, vol=args.vol, rload=args.rload), ["vout", "jack"], 1, 20e3)
    hv = norm(fc, sjk / svo)
    whole = hc * hv
    o3w = norm(fc, o3_pre(fc) * o3_post(fc))
    prop = norm(fc, o3_pre(fc) * proposed_post(fc, 0.0))
    print(f"\n== whole chain, coupling only (a=0, If={args.If*1e6:.0f}uA), normalised at 1 kHz ==")
    table(fc, [hc, whole, o3w, prop], ["SPICE VCF coupl.", "SPICE VCF+VCA", "Open303 hp1*post", "proposed"])
    for name, H in (("Open303", o3w), ("proposed", prop)):
        e, e10 = err(fc, H, whole), err(fc, H, whole, 10)
        print(f"  {name:9s} max |err| 20 Hz-20 kHz: {e[0]:.2f} dB, {e[1]:.1f} deg;  10 Hz-20 kHz: {e10[0]:.2f} dB, {e10[1]:.1f} deg")
    print("  proposed post chain vs SPICE at other resonance settings (VCF coupling taken at a=0):")
    for a in (0.5, 1.0):
        _, (svo, sjk) = ac(standalone(a=a, iabc=20e-6, vol=args.vol, rload=args.rload), ["vout", "jack"], 1, 20e3)
        w = hc * norm(fc, sjk / svo)
        for name, H in (("Open303", o3w), ("proposed", norm(fc, o3_pre(fc) * proposed_post(fc, a)))):
            e = err(fc, H, w)
            print(f"    a={a}: {name:9s} max |err| 20 Hz-20 kHz: {e[0]:.2f} dB, {e[1]:.1f} deg")

    if args.thd:
        print("\n== VCA distortion, 200 Hz sine at vout (OTA modelled without linearising diodes) ==")
        for a, amp in ((0.0, 0.5), (0.0, 1.0), (0.0, 2.5), (1.0, 0.5), (1.0, 1.0), (1.0, 2.5)):
            for ia in (20e-6,):
                net = standalone(a=a, iabc=ia, vol=args.vol, rload=args.rload, src=f"SIN(0 {amp} 200)")
                d, _ = run(net, ".tran 2u 0.3 0.2 2u", ["v(jack)", "v(p3)", "v(p2)"])
                t, y = d[:, 0], d[:, 1]
                tt = np.linspace(0.2, 0.3, 20000, endpoint=False)
                yy = np.interp(tt, t, y) - np.mean(np.interp(tt, t, y))
                sp = abs(np.fft.rfft(yy * np.hanning(len(yy))))
                k = 20  # 200 Hz bin at 0.1 s window
                h = [sp[k * n - 2:k * n + 3].max() for n in range(1, 6)]
                vin = np.interp(tt, t, d[:, 3] - d[:, 5])
                print(f"  a={a:.0f} vout {amp:4.2f} Vpk, Iabc {ia*1e6:.0f}uA: OTA diff in {np.ptp(vin)/2*1e3:6.2f} mVpk, "
                      f"THD {100*np.sqrt(sum(x*x for x in h[1:]))/h[0]:.3f} %  out {np.ptp(yy)/2:.3f} Vpk")


if __name__ == "__main__":
    main()
