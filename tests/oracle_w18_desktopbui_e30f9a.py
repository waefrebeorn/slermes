#!/usr/bin/env python3
"""Differential oracle for hermes_cli/desktop_build_reuse.py (gate w18_desktopbui_e30f9a)."""
import importlib.util, json, sys, types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _seed(name, path=None, **attrs):
    m = types.ModuleType(name)
    if path is not None:
        m.__path__ = [path]
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = ".".join(parts[:i])
        if parent in sys.modules and not hasattr(sys.modules[parent], parts[i]):
            setattr(sys.modules[parent], parts[i], m)
    return m


_seed("hermes_cli", Q + "/hermes_cli")
pm = _seed("pm", install_hint=lambda s: s,
           installed_package=lambda name: None,
           ensure=lambda *a, **k: types.SimpleNamespace(env={}))

spec = importlib.util.spec_from_file_location(
    "w18_desktopbui_live", Q + "/hermes_cli/desktop_build_reuse.py")
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


class _Inst:
    def __init__(self, binary):
        self.binary = binary


class _Resp:
    def __init__(self, stdout):
        self.stdout = stdout


class _FakeFile:
    def __init__(self, text, fail):
        self.text, self.fail = text, fail

    def read_text(self, encoding=None):
        if self.fail:
            raise OSError("missing stamp")
        return self.text


class _FakePath:
    def __init__(self, baked_text, prep_text, baked_fail, prep_fail):
        self._b = _FakeFile(baked_text, baked_fail)
        self._p = _FakeFile(prep_text, prep_fail)

    def __truediv__(self, other):
        s = str(other).replace("\\", "/")
        if s.endswith("build/install-stamp.json"):
            return self._p
        if s.endswith("install-stamp.json"):
            return self._b
        return self


def _canon(x):
    if isinstance(x, (list, tuple)):
        return [_canon(v) for v in x]
    if isinstance(x, dict):
        return {str(k): _canon(v) for k, v in x.items()}
    if isinstance(x, float) and x.is_integer():
        return int(x)
    return x


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
        if func == "stamp_drift":
            binary = r.get("binary")
            pm.installed_package = lambda name: _Inst(binary)
            fail = bool(r.get("fail", False))
            raw = r.get("stdout", "")

            def fake_run(*a, **k):
                if fail:
                    raise OSError("probe failed")
                return _Resp(raw)

            live.subprocess = types.SimpleNamespace(run=fake_run, SubprocessError=OSError)
            py = live._stamp_drift(Path("/proj"), Path("/proj/baked"), {})
        elif func == "packaged_desktop_reusable":
            baked = r.get("baked")
            prepared = r.get("prepared")
            present = bool(r.get("resources_present", False))
            fake = _FakePath(
                json.dumps(baked) if baked is not None else "",
                json.dumps(prepared) if prepared is not None else "",
                baked is None, prepared is None)
            _seed("hermes_cli.main_desktop",
                  _packaged_resources_dir=lambda d: fake if present else None,
                  _desktop_build_needed=lambda d, p, source_mode=False:
                      bool(r.get("build_needed", False)))
            live._stamp_drift = lambda project_root, baked_path, env: r.get("drift")
            py = live.packaged_desktop_reusable(fake, "projroot", {})
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if _canon(py) != _canon(got):
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w18_desktopbui_e30f9a: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
