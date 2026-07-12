import re, json
from collections import defaultdict
src=open('src/cli/commands.c').read()
entries=re.findall(r'\.name="([^"]+)"[^}]*?\.category="([^"]+)"[^}]*?\.handler=([a-zA-Z_][\w]*)\s*}', src, re.S)
entries2=re.findall(r'\.name="([^"]+)"[^}]*?\.handler=([a-zA-Z_][\w]*)\s*}(?![^}]*\.category)', src, re.S)
cats=defaultdict(list); handler_cat={}
for name,cat,h in entries:
    cats[cat].append(h); handler_cat[h]=cat
for name,h in entries2:
    if h not in handler_cat:
        cats['Misc'].append(h); handler_cat[h]='Misc'
def_re=re.compile(r'^(?:static\s+)?(?:const\s+)?[a-zA-Z_][\w\s\*]*?\b([a-z_][\w]*)\s*\(([^)]*)\)\s*\{?\s*$')
all_handlers=[m.group(1) for l in src.splitlines() if (m:=def_re.match(l)) and m.group(1).startswith('cmd_')]
final=defaultdict(list)
for h in all_handlers:
    if h in handler_cat:
        final[handler_cat[h]].append(h); continue
    c='Misc'
    for kw in ['session','save','load','new','undo','history','conv','recap','clear','sessions','stats','branch','snapshot','title','resume','topic','memory','search','export','import']:
        if kw in h: c='Session'; break
    for kw in ['config','setup','model','personality','reasoning','uninstall','backup','dashboard','yolo','fast','footer']:
        if kw in h: c='Config'; break
    for kw in ['skill','bundles','curator','reload_skills']:
        if kw in h: c='Skills'; break
    for kw in ['gateway','platforms','webhook','mcp','restart']:
        if kw in h: c='Gateway'; break
    for kw in ['auth','secrets','key','logs','doctor','deps','debug','update','voice','pet','dump','whoami','profile','goal','agents','toolsets','reload','queue','subgoal','sethome','handoff','platform','indicator','statusbar','busy','steer','copy','paste','image','insights','redraw','background','verbose','skin','rollback','cron','send','exit','stop','clear','commands','tools','help','kanban','completions','approve','deny','browser','compress','reset','retry','status']:
        if kw in h: c='System'; break
    final[c].append(h)
json.dump({k:sorted(set(v)) for k,v in final.items()}, open('scripts/cmd_categories.json','w'), indent=1)
print("categories:", {k:len(v) for k,v in final.items()})
