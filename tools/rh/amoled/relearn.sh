cd /root/amw
S="/mnt/d/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/source"
python3 learn.py learned.json 2>&1 | grep -E "tile patches|index patches|Error|Trace|entries"
cp learned.json screens.json "$S/tools/rh/amoled/"
bash mk.sh | grep -E "exit=|rror"
grep -E "pages|filled|HP-box" "$S/build/mk.log"
