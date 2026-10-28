# the owner's AMOLED TEST patch: a release build (RH_TEST 0), patched against the clean ROM, verified; then the test
# build again (RH_TEST 1) for the emulator tests
set -e
S="/mnt/d/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/source"
H="/mnt/d/AI shit/Claude/Pokemon Romhack Stuff/HeartGold"
OUT="$H/UI screenshots - AMOLED/TEST/HeartGold AMOLED theme TEST.xdelta"
cd "$S"
sed -i 's/^#define RH_TEST 1/#define RH_TEST 0/' include/rh/rh_test.h
bash /root/amw/mk.sh | grep -E "exit=|rror"
cp test.nds /tmp/rel_test.nds
sed -i 's/^#define RH_TEST 0/#define RH_TEST 1/' include/rh/rh_test.h
xdelta3 -e -f -S none -s "$H/Pokemon HeartGold (USA) v1.0.nds" /tmp/rel_test.nds /tmp/rel_test.xdelta
xdelta3 -d -f -s "$H/Pokemon HeartGold (USA) v1.0.nds" /tmp/rel_test.xdelta /tmp/rel_check.nds
cmp /tmp/rel_test.nds /tmp/rel_check.nds && echo "patch verified"
cp /tmp/rel_test.xdelta "$OUT"
ls -l "$OUT"
bash /root/amw/mk.sh | grep -E "exit=|rror"
grep -n "define RH_TEST" include/rh/rh_test.h
