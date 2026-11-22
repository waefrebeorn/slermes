#!/usr/bin/env python3
"""Per-harness content keys for the batch harness builder.

Computes, in ONE Python process, the set of archive members each harness can
actually reach (harness source includes + symbol reachability from the archive
index), then hashes only those inputs. A change to an unrelated object changes
the archive but leaves unaffected harness keys alone, so the next `make` only
relinks the gates whose inputs really changed.

Usage: harness_key.py <specs-file> <archive> <workdir>
Writes <workdir>/<harness>.key (only when the digest changes) and
<workdir>/inputs.tsv (harness<TAB>obj1 obj2 ...).
"""
import hashlib
import json
import os
import re
import subprocess
import sys
from collections import defaultdict, deque

specs_file, archive, work = sys.argv[1:4]
os.makedirs(work, exist_ok=True)

# Registered objects: basename -> path. Same rule as make_harness_archive.sh.
registered = {}
mk = ""
for f in os.listdir("build"):
    if f.endswith(".mk"):
        mk += open(os.path.join("build", f), encoding="utf-8", errors="replace").read()
# Deterministic resolution: sorted order, and prefer real src/lib objects over
# any fixture-copy with the same basename. An unordered set here made the
# cache key flip between objects and silently reuse stale harness binaries.
objs = sorted(set(re.findall(r"[A-Za-z0-9_./\\-]+\.o", mk)),
              key=lambda o: (0 if o.replace("\\", "/").startswith(("src/", "lib/")) else 1,
                             o))
for obj in objs:
    p = obj.replace("\\", "/")
    while p.startswith("./"):
        p = p[2:]
    if os.path.isfile(p):
        registered.setdefault(os.path.basename(p), p)
for extra in (".testbin/hash_O0.o", ".testbin/hermes_regex_O0.o"):
    if os.path.isfile(extra):
        registered[os.path.basename(extra)] = extra

# One nm pass over the archive gives defined and undefined symbols per member.
idx_cache = os.path.join(work, "archive.idx.json")
idx_mtime = os.path.join(work, "archive.idx.mtime")
stamp = str(os.path.getmtime(archive))
if not (os.path.isfile(idx_cache) and os.path.isfile(idx_mtime)
        and open(idx_mtime).read().strip() == stamp):
    defined = defaultdict(set)
    undef = defaultdict(set)
    out = subprocess.run(["nm", "-A", archive], capture_output=True, text=True,
                         errors="replace").stdout
    for line in out.splitlines():
        parts = line.split(":", 2)
        if len(parts) < 3:
            continue
        member = os.path.basename(parts[1])
        rest = parts[2].strip().split()
        if len(rest) < 2:
            continue
        kind, sym = rest[-2], rest[-1]
        if kind == "U":
            undef[member].add(sym)
        else:
            defined[sym].add(member)
    json.dump({"defined": {k: sorted(v) for k, v in defined.items()},
               "undef": {k: sorted(v) for k, v in undef.items()}},
              open(idx_cache, "w"))
    open(idx_mtime, "w").write(stamp)
else:
    d = json.load(open(idx_cache))
    defined = {k: set(v) for k, v in d["defined"].items()}
    undef = {k: set(v) for k, v in d["undef"].items()}

# Include directives name the .c whose .o is the harness's port object.
include_re = re.compile(r'#\s*include\s+"([^"]+)\.c"')
lines = []
for row in open(specs_file, encoding="utf-8"):
    row = row.rstrip("\n")
    if not row:
        continue
    parts = row.split("\t")
    name = parts[0]
    harness = parts[1] if len(parts) > 1 and parts[1] else name
    objs = parts[2] if len(parts) > 2 else ""
    if not os.path.isfile(f"tests/t_port_{harness}.c"):
        continue
    src = open(f"tests/t_port_{harness}.c", encoding="utf-8", errors="replace").read()
    seeds = set()
    for inc in include_re.findall(src):
        o = os.path.basename(inc) + ".o"
        if o in registered:
            seeds.add(o)
    if objs != "FULL":
        for o in objs.split():
            o = os.path.basename(o)
            if o in registered:
                seeds.add(o)
    # Symbol reachability, bounded: transitive closure through the archive.
    seen = set(seeds)
    q = deque(seeds)
    while q:
        m = q.popleft()
        for sym in undef.get(m, ()):
            for dep in defined.get(sym, ()):
                if dep not in seen and len(seen) < 400:
                    seen.add(dep)
                    q.append(dep)
    paths = sorted({registered[m] for m in seen if m in registered})
    h = hashlib.sha256()
    h.update(src.encode("utf-8", "replace"))
    for p in paths:
        h.update(p.encode())
        with open(p, "rb") as fh:
            for chunk in iter(lambda: fh.read(1 << 20), b""):
                h.update(chunk)
    digest = h.hexdigest()
    key = os.path.join(work, f"{harness}.key")
    if not (os.path.isfile(key) and open(key).read().strip() == digest):
        open(key, "w").write(digest + "\n")
    lines.append(f"{harness}\t{' '.join(paths)}")

open(os.path.join(work, "inputs.tsv"), "w").write("\n".join(lines) + "\n")
print(f"keys: {len(lines)} harnesses, {len(defined)} symbols indexed")
