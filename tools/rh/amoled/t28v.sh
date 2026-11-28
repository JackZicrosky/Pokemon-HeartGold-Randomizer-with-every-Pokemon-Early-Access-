# every owner-designed screen, captured in MODES (def id am), at the owner's time (Saturday 03:23, night)
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
cd /root/amw
P=/root/dsvenv/bin/python
export RTC="2026-10-24 03:23"
F='^end|WARN|Error|Trace|give4'
M=am
$P cap.py $M r30.sav W60 setmon:10,10,14,11,13,14 setnature:15 pkset:B,10,ead39729 pkset:D,13,1a0a05 pkset:A,04,c358 poke:0227C298,c358 poke:023B6B68,c358 poke:0227C258,49c25632 poke:0227C2AC,1a poke:0227CC3C,d4010100800a01007d0a01000000000000000000 poke:0227D110,04000500 theme X W100 T43,121 W200 B W60 idreset B W200 X W100 U W40 D W40 idreset T43,41 W200 A W120 rec:px_s28:60 rec:px_s28v0:2 B W60 A W120 rec:px_s28v1:2 B W61 A W120 rec:px_s28v2:2 B W62 A W120 rec:px_s28v3:2 B W63 A W120 rec:px_s28v4:2 B W64 A W120 rec:px_s28v5:2 B W65 A W120 rec:px_s28v6:2 B W66 A W120 rec:px_s28v7:2 B W67 A W120 2>&1 | grep -E "Error|Trace"
# 67: the FIGHT button pressed at each phase of its icon's bob (the owner's pose depends on the moment of the press)
for K in 1 2 3 4 5 6 7 8 9 10 11 12; do $P cap.py $M none sload:bpre theme idreset W150 W150 W150 W$((240 + K)) P128,83 rec:px_e_fpv$K:4 2>&1 | grep -E "Error|Trace"; done
echo T28V DONE
