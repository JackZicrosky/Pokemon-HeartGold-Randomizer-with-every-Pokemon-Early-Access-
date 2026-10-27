# runplan.py MODE PLAN.py TIME : runs every line of the plan (one menu state = one fresh emulator process), skipping
# lines whose recordings all exist, killing a process that hangs (no progress for 600 s) and retrying it once.
import subprocess, sys, os, re
mode, plan, t = sys.argv[1:4]
P = '/root/dsvenv/bin/python'
lines = subprocess.run([P, plan, t, mode], capture_output=True, text=True).stdout.split('\n')
for ln in filter(None, lines):
    names = re.findall(r'rec:([^: ]+)', ln)
    if all(os.path.exists('rec/%s_%s/index.json' % (mode, n)) for n in names): continue
    for attempt in (1, 2):
        try:
            r = subprocess.run([P, 'cap.py', mode, 'none'] + ln.split(), capture_output=True, text=True, timeout=600)
            bad = [l for l in (r.stdout + r.stderr).split('\n') if re.search('Error|Trace', l)]
            if bad: print('ERR', names[0], bad[:3], flush=True)
            break
        except subprocess.TimeoutExpired:
            print('HANG', names[0], 'attempt', attempt, flush=True)
    print('done', mode, names[0], flush=True)
