#!/usr/bin/env python3
"""v548 surgical deleter: removes fake PoP: comment + no-op function body for
every FACADE_SHAPE function the honest audit decided to DELETE (-> honest REAL_GAP).

Safety:
  - Only operates on functions whose PoP comment we decided to DELETE.
  - For web_load_web_config (referenced by 3 internal callers), also removes the
    3 `char *config = web_load_web_config(); ... free(config);` lines so callers
    stay valid.
  - Does NOT touch any function not in the decision set.
  - Writes nothing until DRY_RUN=False; prints per-file deletions.
Usage: python3 tests/v548_delete_facades.py [--apply]
"""
import json, re, sys
from pathlib import Path

SLERMES = Path("/home/wubu/hermes-agent-dev/slermes")
rows = json.load(open(SLERMES / "tests/.v548_facade_real.json"))
DELETE = [r for r in rows if r["decision"] == "DELETE"]
APPLY = "--apply" in sys.argv

# group by file
by_file = {}
for r in DELETE:
    by_file.setdefault(r["file"], []).append(r["cname"])

POP_RE = re.compile(r"/\* PoP:\s*(\w+)\s*@\s*([^:*]+?):(\w+)\s*\*/")
defname_re = re.compile(r"(?<![A-Za-z0-9_.>])([A-Za-z_]\w*)\s*\(")

def find_func_end(text, start):
    """Given index of '{' starting a function body, return index just after matching '}'."""
    assert text[start] == "{"
    depth = 1
    i = start + 1
    n = len(text)
    while i < n and depth > 0:
        c = text[i]
        if c == "'":
            i += 1
            while i < n and text[i] != "'":
                if text[i] == "\\": i += 2
                else: i += 1
            i += 1; continue
        if c == '"':
            i += 1
            while i < n and text[i] != '"':
                if text[i] == "\\": i += 2
                else: i += 1
            i += 1; continue
        if c == "/" and i+1 < n and text[i+1] == "*":
            i += 2
            while i+1 < n and not (text[i] == "*" and text[i+1] == "/"): i += 1
            i += 2; continue
        if c == "/" and i+1 < n and text[i+1] == "/":
            while i < n and text[i] != "\n": i += 1
            continue
        if c == "{": depth += 1
        elif c == "}": depth -= 1
        i += 1
    return i

total_removed = 0
for frel, names in by_file.items():
    fpath = SLERMES / frel
    text = fpath.read_text()
    # collect all deletion ranges [start,end) from the ORIGINAL text at once
    pops = [(m.start(), m.group(1)) for m in POP_RE.finditer(text)]
    ranges = []
    for pos, name in pops:
        if name not in names:
            continue
        # The function to remove is the NEXT function definition after this PoP
        # comment (bridge convention). Its C name may differ from `name`
        # (e.g. cname="_browser_eval" but C fn is "browser_browser_eval").
        rest = text[pos:]
        mf = defname_re.search(rest)
        if not mf:
            continue
        def_start = pos + mf.start()
        p = def_start + len(mf.group(1))
        while p < len(text) and text[p] != "(":
            p += 1
        if p >= len(text):
            continue
        depth = 0; i = p
        while i < len(text):
            if text[i] == "(": depth += 1
            elif text[i] == ")":
                depth -= 1
                if depth == 0: break
            i += 1
        j = i + 1
        while j < len(text) and text[j] in " \t\r\n":
            j += 1
        if j >= len(text) or text[j] != "{":
            continue
        fn_end = find_func_end(text, j)
        ranges.append((pos, fn_end))  # delete [pos, fn_end)
    # sort descending and splice; also handle web_load_web_config call-sites
    new = text
    for (a, b) in sorted(ranges, key=lambda t: -t[0]):
        new = new[:a] + new[b:]
    removed_here = len(ranges)
    if "web_load_web_config" in names:
        new = re.sub(r"\n\s*char \*config = web_load_web_config\(\);\n\s*/\* Parse config[^\n]*\n\s*free\(config\);\n", "\n", new)
        new = re.sub(r"\nchar \*web_load_web_config\(void\);\n", "\n", new)
    if removed_here or "web_load_web_config" in names:
        if APPLY:
            fpath.write_text(new)
        print(f"[{'APPLY' if APPLY else 'DRY'}] {frel}: removed {removed_here} fn(s){' + web_load_web_config call-sites' if 'web_load_web_config' in names else ''}")
        total_removed += removed_here
print(f"\nTotal functions removed (plan): {total_removed}")
