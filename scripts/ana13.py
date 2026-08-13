import json
d=json.load(open('/tmp/scan60.json'))
rows=[]
for name,m in d['modules'].items():
    t=m.get('total',0); p=m.get('pinned',0) if 'pinned' in m else m.get('ported',0)
    r=t-p
    if r==0 or r>3: continue
    g=[x['python_feature']['name'] for x in m.get('gaps',[]) if x.get('classification')=='REAL_GAP']
    rows.append((r,name,g[:4]))
rows.sort()
for r,name,g in rows[:20]: print(f'{r} gaps | {name} | {g}')
