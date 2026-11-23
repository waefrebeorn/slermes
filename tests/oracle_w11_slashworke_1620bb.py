#!/usr/bin/env python3
"""Differential oracle for w11_slashworke_1620bb (tui_gateway/slash_worker.py).

LIVE module loaded with sys.path.insert(0, quarry), parent-package bootstrap
and sys.modules registration. The pure protocol helpers (_slash_base, the
main-loop line parse, SkillSlashRefused, _reply) are driven from the live
module; heavy process-launch collaborators are stubbed.
"""
import contextlib
import importlib.util
import io
import json
import os
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

Q = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, Q)
MOD = "tui_gateway/slash_worker"


def _load_live():
    # Stub the import-path hardener and the stdin-recovery helper; the tested
    # helpers do not touch them.
    hb = types.ModuleType("hermes_bootstrap")
    hb.harden_import_path = lambda: None
    sys.modules["hermes_bootstrap"] = hb
    parts = MOD.split("/")
    for i in range(1, len(parts)):
        pname = ".".join(parts[:i])
        if pname in sys.modules:
            continue
        pdir = os.path.join(Q, *parts[:i])
        init = os.path.join(pdir, "__init__.py")
        sp = importlib.util.spec_from_file_location(
            pname, init, submodule_search_locations=[pdir])
        pm = importlib.util.module_from_spec(sp)
        sys.modules[pname] = pm
        try:
            sp.loader.exec_module(pm)
        except Exception:
            pass
    rec = "tui_gateway._stdin_recovery"
    if rec not in sys.modules:
        m = types.ModuleType(rec)
        m.handle_spurious_eof = lambda *a, **k: False
        sys.modules[rec] = m
    name = ".".join(parts)
    sp = importlib.util.spec_from_file_location(name, os.path.join(Q, MOD + ".py"))
    live = importlib.util.module_from_spec(sp)
    sys.modules[name] = live
    sp.loader.exec_module(live)
    return live


live = _load_live()


def _canon(v):
    return json.loads(json.dumps(v, ensure_ascii=False))


def drive(rec):
    fn = rec.get("func")
    if fn == "slash_base":
        return live._slash_base(rec.get("command") or "")
    if fn == "parse_line":
        line = (rec.get("raw") or "").strip()
        if not line:
            return None
        req = json.loads(line)
        return _canon({"id": req.get("id"), "command": req.get("command", "")})
    if fn == "skill_refused":
        return str(live.SkillSlashRefused(rec.get("base") or ""))
    if fn == "reply":
        fields = rec.get("fields") or {}
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            live._reply(**fields)
        return buf.getvalue()
    return {"error": "unknown func"}


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except Exception:
            continue
        cases += 1
        got = rec.get("out")
        try:
            want = drive(rec)
        except BaseException as exc:
            mism += 1
            print("MISMATCH oracle-drive %s: %s" % (type(exc).__name__, exc), file=sys.stderr)
            continue
        if got != want:
            mism += 1
            print("MISMATCH w11_slashworke_1620bb case=%d func=%s\n  py=%r\n  c =%r" %
                  (cases, rec.get("func"), want, got), file=sys.stderr)
    print("ORACLE w11_slashworke_1620bb: %d cases, %d mismatches" % (cases, mism))
    if cases == 0:
        sys.exit(2)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
