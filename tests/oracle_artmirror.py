#!/usr/bin/env python3
"""Oracle for upstream Hermes pm/artifact_mirror.py.

The npm registry helpers and Source are injected from the fixture; the gate
compares object_key/mirror_url validation and pinned_source's decision logic.
"""
import importlib.util, json, sys
from pathlib import Path
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path: sys.path.insert(0, Q)
SRC = Q + "/pm/artifact_mirror.py"
spec = importlib.util.spec_from_file_location("artmirror_live", SRC)
live = importlib.util.module_from_spec(spec); spec.loader.exec_module(live)

class Source:
    def __init__(self, url, dest, sha256, fallbacks=(), http_mirror=""):
        self.url, self.dest, self.sha256 = url, str(dest), sha256
        self.fallbacks, self.http_mirror = tuple(fallbacks), http_mirror
live.Source = Source

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func"); sha = r.get("sha256", "")
    if func == "object_key":
        try: py = live.object_key(sha)
        except ValueError: py = None
    elif func == "mirror_url":
        try: py = live.mirror_url(sha)
        except ValueError: py = None
    elif func == "pinned":
        mirrored = r.get("mirrored", ""); registry = r.get("registry", "")
        live.npm_registry_url = lambda url, env, m=mirrored: m
        live.npm_registry = lambda env, rg=registry: rg
        try:
            s = live.pinned_source(r.get("url", ""), Path(r.get("dest", "")), sha)
            py = {"url": s.url, "dest": s.dest, "sha256": s.sha256,
                  "fallbacks": list(s.fallbacks), "http_mirror": s.http_mirror}
        except ValueError:
            py = None
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE artmirror: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
