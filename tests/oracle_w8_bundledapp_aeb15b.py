#!/usr/bin/env python3
"""Differential oracle for upstream Hermes hermes_cli/bundled_app.py
(w8_bundledapp_aeb15b gate).

The live module is loaded via spec. Its filesystem edges are injected with a
pure fake Path plus fixture-supplied directory/file listings: Path.resolve,
parent, name, suffix, is_dir, os.access and _files are all deterministic. The
live _only, _resolve_launcher and resolve_bundle_layout bodies do the real work.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

FILES = [[]]
DIRS = [set()]


class FP:
    def __init__(self, p, exec=False):
        self.p = p
        self.exec = exec

    @property
    def name(self):
        return self.p.rstrip("/").rsplit("/", 1)[-1] if self.p.rstrip("/") else ""

    @property
    def suffix(self):
        n = self.name
        i = n.rfind(".")
        return n[i:] if i > 0 and i < len(n) - 1 else ""

    @property
    def parent(self):
        segs = [s for s in self.p.split("/") if s]
        return FP("/" + "/".join(segs[:-1]) if len(segs) > 1 else "/")

    def resolve(self):
        return self

    def __truediv__(self, other):
        return FP(self.p.rstrip("/") + "/" + str(other))

    def is_dir(self):
        return self.p in DIRS[0]

    def is_file(self):
        return True

    def __str__(self):
        return self.p


SRC = Q + "/hermes_cli/bundled_app.py"
spec = importlib.util.spec_from_file_location("bundledapp_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["bundledapp_live"] = live
spec.loader.exec_module(live)

live.Path = FP
live._files = lambda directory: FILES[0]
live.os = types.SimpleNamespace(access=lambda p, mode: p.exec, X_OK=1)


def _files_from(raw):
    return [FP(f.get("path", ""), bool(f.get("exec", False))) for f in raw]


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "only":
            files = [FP(p) for p in r.get("candidates", [])]
            res = live._only(files)
            py = str(res) if res is not None else None
        elif func == "launcher":
            FILES[0] = _files_from(r.get("files", []))
            res = live._resolve_launcher(FP("/dummy"), r.get("platform", ""))
            py = str(res) if res is not None else None
        elif func == "layout":
            FILES[0] = _files_from(r.get("files", []))
            DIRS[0] = set(r.get("dirs", []))
            try:
                lay = live.resolve_bundle_layout(r.get("project_root", ""),
                                                 platform=r.get("platform", ""))
                py = {
                    "ok": True,
                    "app_root": str(lay.app_root),
                    "resources": str(lay.resources),
                    "payload": str(lay.payload),
                    "launcher": str(lay.launcher) if lay.launcher is not None else None,
                }
            except live.NotBundledApp as exc:
                code = "no_app_dir" if "no app directory above the payload" in str(exc) else "not_bundled"
                py = {"ok": False, "error": code}
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w8_bundledapp_aeb15b: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
