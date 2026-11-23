#!/usr/bin/env python3
"""Differential oracle for cron/bot_chat_legacy.py (gate w7_botchatleg_2b179a).

The live module is loaded with importlib.util.spec_from_file_location; its
filesystem/clock collaborators (get_hermes_home, make_user_dir, atomic_json_write,
time.time_ns) are injected so defer() can be observed without touching disk.
_malformed and defer()'s record construction run from the live file.
"""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_mod("hermes_dir_acl", make_user_dir=lambda *a, **k: None)
_mod("hermes_constants", get_hermes_home=lambda: None)
_mod("utils", atomic_json_write=lambda *a, **k: None, atomic_write_bytes=lambda *a, **k: None)

SRC = QUARRY + "/cron/bot_chat_legacy.py"
spec = importlib.util.spec_from_file_location("w7_botchatleg_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


class FakePath:
    def __init__(self, value):
        self._value = str(value)

    def resolve(self):
        return self

    def __truediv__(self, part):
        return FakePath(self._value + "/" + str(part))

    def exists(self):
        return False

    def __str__(self):
        return self._value


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    if func not in ("malformed", "defer_record"):
        continue
    cases += 1
    try:
        if func == "malformed":
            py = live._malformed(r.get("record") or {})
        else:
            captured = []
            live.atomic_json_write = lambda path, data, **kw: captured.append(data)
            live.get_hermes_home = lambda: FakePath("/home/test/.hermes")
            live.make_user_dir = lambda *a, **k: None
            timemod = types.ModuleType("time")
            timemod.time_ns = lambda: int(r.get("sequence", "0"))
            sys.modules["time"] = timemod
            live.defer(r.get("key", ""), r.get("job") or {}, r.get("content", ""),
                       FakePath(r.get("home", "")), for_failure=bool(r.get("for_failure", False)))
            py = captured[0]
    except Exception as exc:
        mism += 1
        print("LIVE ERROR case=%s %r" % (r.get("case"), exc), file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH case=%s\n  py=%r\n  c =%r" % (r.get("case"), py, got), file=sys.stderr)

print("ORACLE w7_botchatleg_2b179a: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
