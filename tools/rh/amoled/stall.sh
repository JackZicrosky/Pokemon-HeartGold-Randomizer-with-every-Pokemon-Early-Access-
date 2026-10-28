# the battle intro, recorded every frame (fresh battle state for this build): where AMOLED stalls against Default
cd /root/amw
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy RTC="2026-10-24 03:23"
P=/root/dsvenv/bin/python
$P cap.py def r30.sav W60 poke:022ADC70,030303030303030303030303 poke:022ADCAC,130013001300130013001300130013001300130013001300 poke:0227CF72,0400 setmon:10,10,14,11,13,14 W21 poke:021D15A8,b179379e battle ssave:bpre_r3 2>&1 | grep -E "Error|Trace"
$P cap.py am none sload:bpre_r3 theme idreset rec:st_am:600 2>&1 | grep -E "Error|Trace"
$P cap.py def none sload:bpre_r3 theme idreset rec:st_def:600 2>&1 | grep -E "Error|Trace"
echo STALL DONE
