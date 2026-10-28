# fresh learning captures: build, battle states, every owner screen in def / id / am (AMOLED without screen pages)
cd /root/amw
bash mk.sh | grep -E "exit=|rror"
NOPAGES=1 MKSTATES=1 MODES="def id am" bash caps_all.sh 2>&1 | grep -E "Error|Trace|done"
echo RECAP DONE
