export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
cd /root/amw
P=/root/dsvenv/bin/python
for T in morning:07 day:13 evening:18 night:22; do
  N=${T%%:*}; H=${T##*:}
  export RTC="2026-10-24 $H:00"
  [ -f bpre_$N.dst ] || $P cap.py def r30.sav theme battle ssave:bpre_$N 2>&1 | grep -E 'Error|Trace'
  for M in def am; do
    # the field state: the theme on, then the bag opened and closed (the game reloads the field's palettes, as when
    # a save made with the theme on boots)
    [ -f fs_${N}_$M.dst ] || $P cap.py $M r30.sav theme X W100 T43,121 W200 B W60 B W200 ssave:fs_${N}_$M 2>&1 | grep -E 'Error|Trace'
    $P runplan.py $M batplan.py $N
    $P runplan.py $M fieldplan.py $N
    echo "$N $M done"
  done
done
