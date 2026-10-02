import numpy as np
# Stinchcombe's full TB-303 transfer function (poles/zeros in rad/s, s = j*omega), cutoff wc = 2*pi*820
wc=2*np.pi*820
def L(s): return (s/wc)**4+2**(11/4)*(s/wc)**3+10*2**.5*(s/wc)**2+2**(13/4)*(s/wc)+1
def P(s): return (s+97.5)*(s+38.5)*(s+4.45)*(s+578.1)*(s+20.0)*(s+7.41)
def Fnet(s): return s**4*(s+46.5)*(s+4.40)/P(s)          # feedback network, ->1 at HF (times 18.7k)
def H(s,k): return 1.06*s**3*(s+109.9)*(s+34.0)*(s+7.41)/(L(s)*P(s)+18.7*k*s**4*(s+46.5)*(s+4.40))
f=np.array([5,10,20,30,50,75,100,150,200,300,500,1000,3000.]); s=1j*2*np.pi*f
F=Fnet(s)
print("Real feedback network F(jw)/F(inf):  freq  |F| dB   phase deg   | 1-pole HP fc=92: dB, deg | fc=150: dB, deg")
for fi,Fi in zip(f,F):
    h=lambda fc: 1j*fi/(fc+1j*fi)
    print(f"  {fi:6.0f}  {20*np.log10(abs(Fi)):7.2f}  {np.degrees(np.angle(Fi)):7.1f}   | {20*np.log10(abs(h(92))):6.2f} {np.degrees(np.angle(h(92))):6.1f} | {20*np.log10(abs(h(150))):6.2f} {np.degrees(np.angle(h(150))):6.1f}")
# best single-pole HP fit (mag+phase) over 20..3000 Hz
ff=np.logspace(np.log10(20),np.log10(3000),200); ss=1j*2*np.pi*ff; Ff=Fnet(ss)
best=min(((np.sum((20*np.log10(abs(1j*ff/(fc+1j*ff)))-20*np.log10(abs(Ff)))**2)+np.sum((np.angle(1j*ff/(fc+1j*ff))-np.angle(Ff))**2*400),fc) for fc in np.arange(40,400,1)))
print("best single-pole HP corner (20-3000 Hz, mag+phase): %.0f Hz"%best[1])
# what phase lead does real network have at typical resonant freqs vs fc=150?
for fr in (300,500,1000,2000):
    sr_=1j*2*np.pi*fr; print(f"  at {fr:4d} Hz: real network phase lead {np.degrees(np.angle(Fnet(sr_))):5.1f} deg, gain {20*np.log10(abs(Fnet(sr_))):5.2f} dB | 1-pole150: {np.degrees(np.angle(1j*fr/(150+1j*fr))):5.1f} deg, {20*np.log10(abs(1j*fr/(150+1j*fr))):5.2f} dB")
# closed-loop real response at 820 Hz cutoff: resonant peaks and low peak vs k
ff2=np.logspace(0.5,3.9,600); H2={}
print("\nreal closed loop, cutoff 820 Hz: k -> main resonant peak (freq, dB re 300Hz level) and low peak (freq, dB re 300 Hz)")
for k in (0.2,0.4,0.6,0.8,0.9,0.99):
    h=20*np.log10(abs(H(1j*2*np.pi*ff2,k))); ref=20*np.log10(abs(H(1j*2*np.pi*300,k)))
    hi=ff2>400; lo=(ff2>3)&(ff2<150)
    ihi=np.argmax(np.where(hi,h,-1e9)); ilo=np.argmax(np.where(lo,h,-1e9))
    print(f"  k={k:4.2f}: main peak {ff2[ihi]:6.0f} Hz {h[ihi]-ref:+6.1f} dB | low peak {ff2[ilo]:5.1f} Hz {h[ilo]-ref:+6.1f} dB")
