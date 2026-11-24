#!/usr/bin/env python3
"""Differential oracle for w18_pluginstra_7bc196 / hermes_cli/plugins_transaction.py."""
import importlib.util, json, sys, types, tempfile
from pathlib import Path
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/plugins_transaction.py"
sys.path.insert(0, QUARRY)
if "hermes_cli" not in sys.modules:
    pkg = types.ModuleType("hermes_cli")
    pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = pkg

spec = importlib.util.spec_from_file_location("tx_live_w18", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["tx_live_w18"] = live
spec.loader.exec_module(live)

# _refresh_declared_dependencies late-binds all of these; fake them so only
# the pure "added specs" + refusal text branch varies.
class FakeDecl:
    def __init__(self, reqs, installs):
        self.install_requirements = tuple(reqs)
        self.installs_packages = bool(installs)

_decls = {}
_enabled = []

pm = types.ModuleType("pm"); pm.__path__ = []; sys.modules["pm"] = pm
pmpd = types.ModuleType("pm.plugin_declarations")
pmpd.read_python_declaration = lambda path: _decls[str(path)]
pmpd.manifest_version_error = lambda manifest, name: None
sys.modules["pm.plugin_declarations"] = pmpd
pmws = types.ModuleType("pm.workspace")
pmws.enabled_plugin_dirs = lambda installing=None: _enabled
pmws.install_node_sidecar = lambda *a, **k: None
sys.modules["pm.workspace"] = pmws
rs = types.ModuleType("hermes_cli.runtime_state")
rs._bytes = lambda p: b""
sys.modules["hermes_cli.runtime_state"] = rs
pc = types.ModuleType("hermes_cli.plugins_cmd")
class PluginOperationError(Exception): pass
pc.PluginOperationError = PluginOperationError
pc._console = lambda: None
sys.modules["hermes_cli.plugins_cmd"] = pc

class NamedPath:
    """Real path for filesystem ops, but .name is exactly the fixture value."""
    def __init__(self, real, name):
        self._real = real
        self.name = name
    def __truediv__(self, other):
        return self._real / other
    def resolve(self):
        return self._real.resolve()
    def __str__(self):
        return str(self._real)

def compute(r):
    before = r.get("before_reqs", [])
    after = r.get("after_reqs", [])
    with tempfile.TemporaryDirectory() as d:
        real_target = Path(d) / "t"; real_target.mkdir()
        staged = Path(d) / "s"; staged.mkdir()
        target = NamedPath(real_target, r.get("target_name", ""))
        _decls.clear()
        _decls[str(target)] = FakeDecl(before, r.get("before_installs", False))
        _decls[str(staged)] = FakeDecl(after, r.get("after_installs", False))
        _enabled[:] = [target.resolve()]
        try:
            live._refresh_declared_dependencies(
                target, staged, {"name": "plug"}, interactive=False)
            return None
        except PluginOperationError as exc:
            return str(exc)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None: continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE w18_pluginstra_7bc196: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w18_pluginstra_7bc196 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w18_pluginstra_7bc196: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
