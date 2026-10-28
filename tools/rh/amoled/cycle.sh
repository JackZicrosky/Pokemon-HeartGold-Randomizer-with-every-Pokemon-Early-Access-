# one fix cycle: learn from the captures, build, battle transition check, proof recordings
cd /root/amw
bash relearn.sh
rm -f bpre_morning.dst vf_cmd.dst vf_bag.dst; rm -rf rec/am_vfix_*
bash vfix.sh
bash proofrun.sh
echo CYCLE DONE
