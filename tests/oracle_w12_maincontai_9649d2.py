#!/usr/bin/env python3
"""Oracle for hermes_cli/main_container.py exec_in_container (deterministic seam)."""
import importlib.util, io, json, os, sys, types
from unittest import mock

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, Q)

def _load():
    pdir = os.path.join(Q, "hermes_cli")
    sp = importlib.util.spec_from_file_location(
        "hermes_cli", os.path.join(pdir, "__init__.py"), submodule_search_locations=[pdir])
    pm = importlib.util.module_from_spec(sp)
    sys.modules["hermes_cli"] = pm
    try:
        sp.loader.exec_module(pm)
    except Exception:
        pass
    target = os.path.join(Q, "hermes_cli", "main_container.py")
    sp2 = importlib.util.spec_from_file_location("hermes_cli.main_container", target)
    live = importlib.util.module_from_spec(sp2)
    sys.modules["hermes_cli.main_container"] = live
    sp2.loader.exec_module(live)
    return live

live = _load()

class Stop(Exception):
    pass

class CP:
    def __init__(self, rc):
        self.returncode = rc

def compute(r):
    backend = r.get("backend", "docker")
    info = {
        "backend": backend,
        "container_name": r.get("container_name", "hermes"),
        "exec_user": r.get("exec_user", "hermes"),
        "hermes_bin": r.get("hermes_bin", "/usr/bin/hermes"),
    }
    args = r.get("cli_args", [])
    env = dict(r.get("env", {}))
    tty = bool(r.get("stdin_isatty", False))
    p1 = int(r.get("probe1_rc", 0))
    p2 = int(r.get("probe2_rc", 0))
    runtime = env.get("runtime", "")
    sudo = env.get("sudo_path", "")

    def which(name):
        return runtime if name == backend else (sudo if name == "sudo" else "")

    probes = iter([p1, p2])
    def run(*a, **k):
        return CP(next(probes, 0))

    err = io.StringIO()
    captured = {}
    def execvp(path, argv):
        captured["argv"] = argv
        raise Stop()
    def sysexit(code):
        raise Stop()

    patches = [
        mock.patch.object(live.shutil, "which", which),
        mock.patch.object(live.subprocess, "run", run),
        mock.patch.object(live.os, "environ", env),
        mock.patch.object(live.os, "getenv", lambda k, d=None: env.get(k, d)),
        mock.patch.object(live.sys, "stdin", types.SimpleNamespace(isatty=lambda: tty)),
        mock.patch.object(live.sys, "stderr", err),
        mock.patch.object(live.sys, "exit", sysexit),
        mock.patch.object(live.os, "execvp", execvp),
    ]
    for p in patches:
        p.start()
    try:
        try:
            live.exec_in_container(info, args)
        except Stop:
            pass
    finally:
        for p in reversed(patches):
            p.stop()
    if "argv" in captured:
        return {"kind": "exec", "argv": captured["argv"]}
    return {"kind": "error", "message": err.getvalue()}

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "exec_plan":
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w12_maincontai_9649d2: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
