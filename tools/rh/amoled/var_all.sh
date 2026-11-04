# owner's situations beyond the pictures (2026-11-04 list): run after a build; MODES "def am"
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
cd /root/amw
P=/root/dsvenv/bin/python
export RTC="2026-10-24 03:23"
F='^end|WARN|Error|Trace|party6'
B=$(cat bagfull.tok)
V6="party6:152/0/4;16/155/3/20;74/17/2;92/234/1/0;129/0/11/3;25/236/12"
VT="pkset:A,00,9e00 pkset:A,02,9b00 pkset:B,00,37000a002b006300 pkset:B,08,19231e14"
WILD="poke:022ADC70,040404040404040404040404 poke:022ADCAC,a300a300a300a300a300a300a300a300a300a300a300a300 poke:0227CF72,0400"
for M in ${MODES:-def am}; do
# field menu: cursor down the buttons (continue -> X -> down)
$P cap.py $M r30.sav W60 theme X W100 rec:px_c_menu0:30 D W30 rec:px_c_menu1:30 D W30 rec:px_c_menu2:30 D W30 rec:px_c_menu3:30 r W30 rec:px_c_menu4:30 2>&1 | grep -E "$F"
# full party: cursor moves, party menu, summaries of slots 1-2 (3 pages)
$P cap.py $M r30.sav W60 $V6 theme X W100 T43,81 W200 rec:px_v6_party:30 D W30 rec:px_v6_party_d1:30 D W30 rec:px_v6_party_d2:30 r W30 rec:px_v6_party_r:30 U W30 U W30 rec:px_v6_party_u:30 A W100 rec:px_v6_pmenu:30 A W200 rec:px_v6_sum1:30 D W80 rec:px_v6_sum1b:30 R W80 rec:px_v6_sum2:30 D W80 rec:px_v6_sum2b:30 R W80 rec:px_v6_sum3:30 B W150 B W100 B W100 B W150 2>&1 | grep -E "$F"
# full bag: every pocket, cursor moves
seq=""
for X in 16 48 80 112 144 176 208 240; do
  seq="$seq T$X,16 W60 rec:px_bv${X}_0:20 D W20 rec:px_bv${X}_d:20 r W20 rec:px_bv${X}_r:20"
done
$P cap.py $M r30.sav W60 $B poke:0227CC3C,d4010100800a01007d0a01000000000000000000 theme X W100 T43,121 W200 $seq 2>&1 | grep -E "$F"
# save prompt, trainer card front / flip / back, options menu values
$P cap.py $M r30.sav W60 theme X W100 T123,81 W100 rec:px_v_save:30 B W100 B W100 X W100 T123,41 W200 rec:px_v_tc:30 rec:px_v_tcflip:60:T128_100 W60 rec:px_v_tcback:30 B W150 X W100 T123,121 W200 rec:px_v_opt0:30 r W20 rec:px_v_opt1:20 D W20 rec:px_v_opt2:20 r W20 rec:px_v_opt3:20 2>&1 | grep -E "$F"
# battle with Totodile (4 moves): command, moves, party, summary pages, Check Moves, then run away
$P cap.py $M r30.sav W60 $WILD $VT W21 battle theme W700 rec:px_v_cmd3:30 P128,83 rec:px_v_fp0:30 rel W300 rec:px_v_moves:30 B W20 B W400 P216,168 rec:px_v_bparty_press:30 rel W400 rec:px_v_bparty:30 T64,40 W400 rec:px_v_bparty_menu:30 T50,172 W500 rec:px_v_bsum:30 R W200 rec:px_v_bsum2:30 B W500 T155,172 W500 rec:px_v_checkmoves:30 B W300 B W300 B W400 rec:px_v_cmd4:30 rec:px_v_run:400:T128_180 2>&1 | grep -E "$F"
# battle bag: open, pocket press, items, item press (white flash?) - full bag
$P cap.py $M r30.sav W60 $WILD $B W21 battle theme W700 rec:px_v_bb_press:40:T40_168 W300 rec:px_v_bb:30 rec:px_v_bb_pocket:60:T64_42 W300 rec:px_v_bb_items:30 rec:px_v_bb_item:90:T64_30 W200 rec:px_v_bb_use:30 2>&1 | grep -E "$F"
# Check Moves with every type (5 battles)
i=0
for MV in 2100020010002800 1c00580051007a00 e800480234003700 160054005d00b500 52002c0021002100; do
  i=$((i+1))
  $P cap.py $M r30.sav W60 $WILD pkset:B,00,$MV pkset:B,08,14141414 W21 battle theme W700 P216,168 rel W400 T64,40 W400 T155,172 W500 rec:px_v_cmt$i:20 2>&1 | grep -E "$F"
done
done
echo VARALL DONE
