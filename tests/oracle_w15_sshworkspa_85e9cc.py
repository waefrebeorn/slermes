#!/usr/bin/env python3
"""Differential oracle for hermes_cli/ssh_workspace_fs.py (gate w15_sshworkspa_85e9cc)."""
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

if "hermes_cli" not in sys.modules:
    _pkg = types.ModuleType("hermes_cli")
    _pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = _pkg


def _stub(name, **attrs):
    m = types.ModuleType(name)
    if "." in name:
        parent = name.rsplit(".", 1)[0]
        if parent not in sys.modules:
            pm = types.ModuleType(parent)
            pm.__path__ = [QUARRY + "/" + parent.replace(".", "/")]
            sys.modules[parent] = pm
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_stub("hermes_cli._subprocess_compat", windows_hide_flags=lambda: 0)
_stub("tools")
_stub("tools.environments")
_stub("tools.environments.ssh", SSHEnvironment=object)


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m


live = load("w15_sshworkspa_85e9cc_live", QUARRY + "/hermes_cli/ssh_workspace_fs.py")


class _FakeEnv:
    def __init__(self, cwd, remote_home):
        self.cwd = cwd
        self._remote_home = remote_home


def compute(r):
    f = r.get("func")
    if f == "error_code":
        return live.SshWorkspaceFs._error_code(r.get("output", ""), r.get("fallback", "EIO"))
    if f == "normalize":
        env = _FakeEnv(r.get("cwd", ""), r.get("remote_home", ""))
        return live.SshWorkspaceFs(env).normalize(r.get("path", ""))
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
        print("ORACLE w15_sshworkspa_85e9cc: live raised %r case %s" % (exc, r.get("case")), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH w15_sshworkspa_85e9cc %s case=%s py=%r c=%r" %
              (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w15_sshworkspa_85e9cc: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
