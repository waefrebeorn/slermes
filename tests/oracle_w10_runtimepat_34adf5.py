#!/usr/bin/env python3
"""Differential oracle for hermes_cli/runtime_paths.py (gate w10_runtimepat_34adf5).

The live module re-exports pm.environments helpers; path/env probes are injected
as explicit seams, so only the deterministic layout logic is compared.
"""
import hashlib, importlib.util, json, posixpath, sys, types
from pathlib import PurePosixPath as PP

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
PKG = "w10_runtimepat_live_pkg"
_pkg = types.ModuleType(PKG); _pkg.__path__ = [Q + "/hermes_cli"]; sys.modules[PKG] = _pkg
spec = importlib.util.spec_from_file_location(PKG + ".runtime_paths", Q + "/hermes_cli/runtime_paths.py")
live = importlib.util.module_from_spec(spec); sys.modules[spec.name] = live; spec.loader.exec_module(live)
import pm.environments as pm_env  # noqa: E402  (the live re-export source)


def compute(r):
    f = r.get("func")
    if f == "install_key":
        return hashlib.sha256(r.get("canonical", "").encode("utf-8")).hexdigest()[:16]
    if f == "venv_bin_dir":
        return str(pm_env.venv_bin_dir(PP(r.get("venv", "")), windows=bool(r.get("windows", False)))).replace("\\", "/")
    if f == "venv_python":
        return str(pm_env.venv_python(PP(r.get("venv", "")), windows=bool(r.get("windows", False)))).replace("\\", "/")
    if f == "site_packages":
        v = r.get("venv", "")
        if r.get("windows", False):
            return posixpath.join(v, "Lib/site-packages")
        return posixpath.join(v, "lib/python%d.%d/site-packages" % (int(r.get("major", 3)), int(r.get("minor", 11))))
    if f == "install_state_dir":
        return posixpath.join(posixpath.join(r.get("home_root", ""), "installs"), r.get("key", ""))
    if f == "runtime_facts_path":
        return posixpath.join(posixpath.join(posixpath.join(r.get("home_root", ""), "installs"), r.get("key", "")), "facts.json")
    if f == "cli_command_name":
        c = r.get("command") or None
        return str(PP(c).name.removesuffix(".exe")) if c else "hermes"
    if f == "shell_exports":
        return pm_env.shell_exports(r.get("env", {}), r.get("dialect", "sh"))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s args=%r py=%r c=%r" % (r.get("func"), r, py, got), file=sys.stderr)
print("ORACLE w10_runtimepat_34adf5: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
