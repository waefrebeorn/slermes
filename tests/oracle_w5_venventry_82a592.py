#!/usr/bin/env python3
"""Oracle for upstream Hermes pm/_venv_entry.py (w5_venventry_82a592 gate).

main() is pure argv dispatch plus process execution. The oracle instruments
the LIVE main() with a fake sys/os/site/runpy and a patched builtins.exec to
observe its deterministic plan: usage vs code/module/script, the resulting
sys.argv, the SITE passed to addsitedir, and the front sys.path entry inserted
when not safe. os.getcwd()/abspath/dirname are stubbed with fixture values.
"""
import importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

spec = importlib.util.spec_from_file_location("w5_venventry_live", Q + "/pm/_venv_entry.py")
live = importlib.util.module_from_spec(spec)
sys.modules["w5_venventry_live"] = live
spec.loader.exec_module(live)

SCRIPT = Q + "/pm/_venv_entry.py"
HERE = "C:\\pm"  # dirname(abspath(SCRIPT)) under our FakePath


class Flags:
    def __init__(self, safe):
        self.safe_path = bool(safe)
        self.isolated = bool(safe)


class FakeSys:
    def __init__(self, argv, path, flags):
        self.argv, self.path, self.flags = argv, path, flags
        self.modules = {}


class FakePath:
    def __init__(self, cwd):
        self.cwd = cwd

    def abspath(self, p):
        q = p.replace("/", "\\")
        if q.startswith("\\\\") or (len(q) >= 2 and q[1] == ":"):
            return q
        return self.cwd.rstrip("\\") + "\\" + q if q else self.cwd

    def dirname(self, p):
        i = max(p.rfind("\\"), p.rfind("/"))
        return p[:i] if i >= 0 else ""


def observe(argv, safe, cwd):
    fs = FakeSys([SCRIPT] + list(argv), ["P0", "C:/base", Q + "/pm"], Flags(safe))
    rec = {"site": None, "action": None, "err": None}
    old = (live.sys, live.os, live.site, live.runpy, live.__file__)
    live.sys = fs
    live.__file__ = SCRIPT
    live.os = types.SimpleNamespace(
        path=FakePath(cwd), getcwd=lambda: cwd, fspath=lambda p: p)
    live.site = types.SimpleNamespace(addsitedir=lambda d: rec.__setitem__("site", d))
    live.runpy = types.SimpleNamespace(
        run_module=lambda m, **k: rec.__setitem__("action", "module:" + m),
        run_path=lambda p, **k: rec.__setitem__("action", "script:" + p))
    import builtins
    old_exec = builtins.exec
    def fake_exec(code, ns):
        rec["action"] = "code"
    builtins.exec = fake_exec
    try:
        live.main()
    except SystemExit as exc:
        rec["err"] = str(exc)
    finally:
        builtins.exec = old_exec
        (live.sys, live.os, live.site, live.runpy, live.__file__) = old

    # "P0" is the sentinel first entry: the filter never removes it, so any
    # other leading value means main() inserted a front sys.path entry.
    inserted = bool(fs.path) and fs.path[0] != "P0"
    insert_path = fs.path[0] if inserted else ""
    site = rec["site"] if rec["site"] is not None else ""
    a = rec["action"] or ""
    if rec["err"] is not None:
        action, new_argv = "usage", []
    elif a == "code":
        action, new_argv = "code", list(fs.argv)
    elif a.startswith("module:"):
        action, new_argv = "module", list(fs.argv)
    elif a.startswith("script:"):
        action, new_argv = "script", list(fs.argv)
    else:
        action, new_argv = "usage", []
    return {"action": action, "site": site, "inserted": inserted,
            "insert_path": insert_path, "argv": new_argv}


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
    if func != "plan":
        continue
    try:
        py = observe(r.get("argv", []), bool(r.get("safe", False)), r.get("cwd", ""))
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w5_venventry_82a592: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
