#!/usr/bin/env python3
"""Differential oracle for tui_gateway/pet_sheet.py (gate w8_petsheet_21b5c1).

stat() is injected through a fake spritesheet object; clone_payload is pure.
"""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m


live = load("w8_petsheet_21b5c1_live", QUARRY + "/tui_gateway/pet_sheet.py")


class _SS:
    def __init__(self, mtime_ns, size, fail):
        self.mtime_ns, self.size, self.fail = mtime_ns, size, fail

    def stat(self):
        if self.fail:
            raise OSError("stat failed")
        return types.SimpleNamespace(st_mtime_ns=self.mtime_ns, st_size=self.size)


def compute(r):
    func = r.get("func")
    if func == "sheet_revision":
        return live._pet_sheet_revision(_SS(int(r.get("mtime_ns", "0")),
                                            int(r.get("size", "0")),
                                            bool(r.get("fail", False))))
    if func == "clone_payload":
        return live._clone_pet_payload(r.get("payload") or {})
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print("ORACLE w8_petsheet_21b5c1: live raised %r case %s" % (exc, r.get("case")), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH w8_petsheet_21b5c1 %s case=%s py=%r c=%r" %
              (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w8_petsheet_21b5c1: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
