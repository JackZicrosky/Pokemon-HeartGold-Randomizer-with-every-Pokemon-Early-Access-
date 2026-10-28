export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy RTC="2026-10-24 07:00"
cd /root/amw
P=/root/dsvenv/bin/python
[ -f bpre_morning.dst ] || $P cap.py def r30.sav theme battle ssave:bpre_morning 2>&1 | grep -E 'Error|Trace'
$P cap.py am none sload:bpre_morning theme W800 ssave:vf_cmd rec:vfix_cmd_bag:300:T40_170 sload:vf_cmd T40,170 W400 ssave:vf_bag rec:vfix_bag_balls:300:T192_40 sload:vf_bag rec:vfix_bag_hpp:300:T64_40 sload:vf_bag rec:vfix_bag_back:300:T240_176 sload:vf_cmd T216,168 W400 T64,40 W400 T155,172 W500 rec:vfix_checkmoves:300 sload:vf_cmd rec:vfix_cmd_fight:300:T128_83 sload:vf_cmd rec:vfix_cmd_pokemon:300:T216_168 2>&1 | grep -E 'Error|Trace|^rec'
