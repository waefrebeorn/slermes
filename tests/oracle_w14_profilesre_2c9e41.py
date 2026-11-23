#!/usr/bin/env python3
"""Differential oracle for w14_profilesre_2c9e41 (hermes_cli/profiles_remove.py).

The live module imports only stdlib at module scope; the two scripted leaves
are driven with recording doubles injected onto the live module globals.
"""
import importlib.util
import json
import posixpath
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = _mod(name)
    if not hasattr(m, "__path__"):
        m.__path__ = []
    return m


_pkg("hermes_cli")  # parent-package bootstrap

SRC = Q + "/hermes_cli/profiles_remove.py"
spec = importlib.util.spec_from_file_location("hermes_cli.profiles_remove_w14_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def canon(v):
    def conv(x):
        if isinstance(x, float) and x.is_integer():
            return int(x)
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


def compute(r):
    f = r.get("func")

    if f == "make_writable":
        path = r.get("path", "")
        error = r.get("error", "")
        exc = {
            "PermissionError": PermissionError("x"),
            "FileNotFoundError": FileNotFoundError("x"),
            "RuntimeError": RuntimeError("x"),
            "OSError": OSError("x"),
            "ValueError": ValueError("x"),
        }.get(error, OSError("x"))
        if r.get("as_tuple"):
            exc = ("ctx", exc)  # onexc tuple form -> exc[1] is inspected

        calls = {"chmod": [], "stat": [], "func": []}
        fake_os = types.ModuleType("os")

        fake_os.path = types.SimpleNamespace(dirname=posixpath.dirname)

        def fchmod(p, mode):
            calls["chmod"].append(p)

        def fstat(p):
            calls["stat"].append(p)
            return types.SimpleNamespace(st_mode=0o444)

        fake_os.chmod = fchmod
        fake_os.stat = fstat
        old = (live.os, live.stat)
        live.os = fake_os
        live.stat = types.SimpleNamespace(S_IWUSR=0o200)

        def ffunc(p):
            calls["func"].append(p)

        try:
            try:
                live._rmtree_make_writable(ffunc, path, exc)
                reraise = False
            except Exception:
                reraise = True
        finally:
            live.os, live.stat = old
        return {"reraise": reraise, "chmod": calls["chmod"],
                "call_func": bool(calls["func"])}

    if f == "retry":
        ok_list = r.get("rmtree_ok", [])
        ex_list = r.get("exists", [])
        ri, ei, sleeps, gone = [0], [0], [0], [False]

        class RmtreeErr(OSError):
            pass

        def faker(p, **kw):
            ok = ok_list[ri[0]] if ri[0] < len(ok_list) else True
            ri[0] += 1
            if not ok:
                raise RmtreeErr()

        def fexists():
            v = ex_list[ei[0]] if ei[0] < len(ex_list) else True
            ei[0] += 1
            if not v:
                gone[0] = True
            return v

        old = (live.shutil, live.time)
        live.shutil = types.SimpleNamespace(rmtree=faker)
        live.time = types.SimpleNamespace(
            sleep=lambda s: sleeps.__setitem__(0, sleeps[0] + 1))
        path = types.SimpleNamespace(exists=fexists)
        handler = lambda *a: None
        try:
            try:
                live._rmtree_with_retry(path, handler)
                result = "gone" if gone[0] else "success"
            except OSError:
                result = "raised"
        finally:
            live.shutil, live.time = old
        return {"result": result, "rmtree_calls": ri[0],
                "exists_calls": ei[0], "sleeps": sleeps[0]}

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
        py = canon(compute(r))
    except Exception as exc:
        print(f"LIVE ERROR {r.get('func')} case={r.get('case')}: {exc!r}",
              file=sys.stderr)
        mism += 1
        continue
    got = canon(r.get("out"))
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}",
              file=sys.stderr)

print(f"ORACLE w14_profilesre_2c9e41: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
