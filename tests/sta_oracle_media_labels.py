#!/usr/bin/env python3
"""Oracle for the pure helpers in gateway/relay/media.py (media_base_url,
is_relay_media_url), hermes_cli/platforms.py (_plugin_label), and
hermes_cli/browser_credential.py (generation_of). All ast-extracted and run
live.
"""
import ast
import hashlib
import json
import sys
from types import SimpleNamespace

sources = [
    ("C:/Users/eman5/hermes-agent/gateway/relay/media.py",
     {"_fn": "media_base_url", "method": None}),
    ("C:/Users/eman5/hermes-agent/gateway/relay/media.py", None),
    ("C:/Users/eman5/hermes-agent/hermes_cli/platforms.py", None),
    ("C:/Users/eman5/hermes-agent/hermes_cli/browser_credential.py", None),
]

NS = {}


def _collect(path, names):
    code = open(path, encoding="utf-8").read()
    tree = ast.parse(code)
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name in names:
            seg = ast.get_source_segment(code, n)
            exec(compile(ast.Module(body=[ast.parse(seg).body[0]],
                                    type_ignores=[]), "x", "exec"), NS)
        elif isinstance(n, ast.ClassDef):
            for m in n.body:
                if isinstance(m, ast.FunctionDef) and m.name in names:
                    seg = ast.get_source_segment(code, m)
                    exec(compile(ast.Module(body=[ast.parse(seg).body[0]],
                                            type_ignores=[]), "x", "exec"), NS)


_collect(sources[0][0], {"media_base_url", "is_relay_media_url"})
_collect(sources[2][0], {"_plugin_label"})
_collect(sources[3][0], {"generation_of"})
NS["hashlib"] = hashlib

mbu = NS["media_base_url"]
irm = NS["is_relay_media_url"]
plg = NS["_plugin_label"]
gof = NS["generation_of"]


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE media_labels: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    for c in cases:
        fam = c["family"]
        if fam == "burl":
            want = mbu(c["in"])
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH burl {c['case']}: C={c['out']!r} PY={want!r} "
                      f"in={c['in']!r}")
        elif fam == "url":
            want = 1 if irm(None, c["in"]) else 0
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH url {c['case']}: C={c['out']} PY={want} "
                      f"in={c['in']!r}")
        elif fam == "label":
            e = SimpleNamespace(emoji=c["emoji"], label=c["label"])
            want = plg(e)
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH label {c['case']}: C={c['out']!r} PY={want!r}")
        elif fam == "gen":
            want = gof(c["in"])
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH gen {c['case']}: C={c['out']!r} PY={want!r}")
    print(f"ORACLE media_labels: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
