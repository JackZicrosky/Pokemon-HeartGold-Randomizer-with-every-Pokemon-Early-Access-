import ast
rows=[ast.literal_eval(l.split(' ',1)[1]) for l in open('timing.txt') if l.startswith('sigdbg')]
def lines(v): return (v-192)%263
st=[lines(r[3]) for r in rows]; sg=[lines(r[1]) for r in rows]; en=[lines(r[2]) for r in rows]
print('frames',len(rows),'start max',max(st))
print('sig cost (lines) max',max(b-a for a,b in zip(st,sg)),'avg %.1f'%(sum(b-a for a,b in zip(st,sg))/len(rows)))
print('task cost max',max(c-a for a,c in zip(st,en)),'task end max',max(en),'frames ending after vblank (>70):',sum(1 for e in en if e>70))
