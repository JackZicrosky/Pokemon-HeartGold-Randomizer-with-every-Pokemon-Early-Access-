import json, sys
for d in sys.argv[1:]:
    fr=json.load(open(d+'/index.json'))['frames']; runs=[]; i=0
    while i<len(fr):
        j=i
        while j+1<len(fr) and fr[j+1]==fr[i]: j+=1
        if j-i>=3: runs.append((i,j-i+1))
        i=j+1
    print(d,len(set(fr)),runs[:14])
