#!/usr/bin/env python3
"""v548 thin-wrapper fraud deleter. Targets 17 THIN_SHAPE functions whose Python
does real work but C returns a canned/const/void (-> honest REAL_GAP). All
verified unreferenced. Same safe mechanism as the other deleters.
Usage: python3 tests/v548_delete_thin.py [--apply]
"""
import json, re, sys
from pathlib import Path

SLERMES = Path("/home/wubu/hermes-agent-dev/slermes")
rows = json.load(open(SLERMES / "tests/.v548_thin_real.json"))
TARGET = {}
for r in rows:
    if r["decision"] == "DELETE":
        TARGET.setdefault(r["file"], []).append(r["cname"])

POP_RE = re.compile(r"/\* PoP:\s*(\w+)\s*@\s*([^:*]+?):(\w+)\s*\*/")
defname_re = re.compile(r"(?<![A-Za-z0-9_.>])([A-Za-z_]\w*)\s*\(")
APPLY = "--apply" in sys.argv

def find_func_end(text, start):
    assert text[start] == "{"
    depth=1; i=start+1; n=len(text)
    while i<n and depth>0:
        c=text[i]
        if c=="'":
            i+=1
            while i<n and text[i]!="'":
                if text[i]=="\\": i+=2
                else: i+=1
            i+=1; continue
        if c=='"':
            i+=1
            while i<n and text[i]!='"':
                if text[i]=="\\": i+=2
                else: i+=1
            i+=1; continue
        if c=="/" and i+1<n and text[i+1]=="*":
            i+=2
            while i+1<n and not (text[i]=="*" and text[i+1]=="/"): i+=1
            i+=2; continue
        if c=="/" and i+1<n and text[i+1]=="/":
            while i<n and text[i]!="\n": i+=1
            continue
        if c=="{": depth+=1
        elif c=="}": depth-=1
        i+=1
    return i

total=0
for frel, names in TARGET.items():
    fpath=SLERMES/frel
    text=fpath.read_text()
    pops=[(m.start(),m.group(1)) for m in POP_RE.finditer(text)]
    ranges=[]
    for pos,name in pops:
        if name not in names: continue
        rest=text[pos:]
        mf=defname_re.search(rest)
        if not mf: continue
        def_start=pos+mf.start()
        p=def_start+len(mf.group(1))
        while p<len(text) and text[p]!="(": p+=1
        if p>=len(text): continue
        depth=0;i=p
        while i<len(text):
            if text[i]=="(": depth+=1
            elif text[i]==")":
                depth-=1
                if depth==0: break
            i+=1
        j=i+1
        while j<len(text) and text[j] in " \t\r\n": j+=1
        if j>=len(text) or text[j]!="{": continue
        ranges.append((pos, find_func_end(text,j)))
    new=text
    for a,b in sorted(ranges,key=lambda t:-t[0]):
        new=new[:a]+new[b:]
    if ranges:
        if APPLY: fpath.write_text(new)
        print(f"[{'APPLY' if APPLY else 'DRY'}] {frel}: removed {len(ranges)} fn(s)")
        total+=len(ranges)
print(f"\nTotal thin-wrapper frauds removed (plan): {total}")
