#!/usr/bin/env python3
"""Differential oracle for hermes_cli/doctor_platform.py (w13_doctorplat_e064d4)."""
import importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, path=None, **attrs):
    m = types.ModuleType(name)
    if path is not None:
        m.__path__ = [path]
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_pkg = _mod("hermes_cli", Q + "/hermes_cli")
_mod("hermes_cli.colors", Colors=types.SimpleNamespace(YELLOW="yellow"), color=lambda s, c=None: s)
_mod("hermes_cli.config",
     is_nix_install_method=lambda m: False,
     recommended_update_command_for_method=lambda m: "hermes update",
     detect_install_method=lambda p: "pip")
_mod("hermes_cli.doctor", PROJECT_ROOT=Q)
_mod("hermes_cli.doctor_report",
     Finding=type("Finding", (), {}), doctor_check=lambda *a, **k: (lambda f: f),
     _fail_and_issue=lambda *a, **k: None, _section=lambda *a, **k: None,
     check_bool=lambda *a, **k: None, check_fail=lambda *a, **k: None,
     check_info=lambda *a, **k: None, check_ok=lambda *a, **k: None,
     check_warn=lambda *a, **k: None, warn_on_error=lambda *a, **k: (lambda f: f))
_mod("hermes_cli._startup_fast", project_root_str=lambda: Q)
_mod("hermes_constants", is_termux=lambda: False)

SRC = Q + "/hermes_cli/doctor_platform.py"
spec = importlib.util.spec_from_file_location("w13_doctorplat_e064d4_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w13_doctorplat_e064d4_live"] = live
spec.loader.exec_module(live)


def compat(v):
    if isinstance(v, (set, frozenset)):
        return sorted((compat(x) for x in v), key=repr)
    if isinstance(v, (list, tuple)):
        return [compat(x) for x in v]
    if isinstance(v, dict):
        return {str(k): compat(x) for k, x in v.items()}
    return v


def _tcc(dr):
    rows = []
    live.sys = types.SimpleNamespace(platform="darwin")
    live._desktop_app_bundle = lambda: "APP"
    live._macos_desktop_dr = lambda app: dr
    live.check_warn = lambda msg, detail=None: rows.append(["warn", msg, detail])
    live.check_ok = lambda msg, detail=None: rows.append(["ok", msg, detail])
    live.check_info = lambda msg, detail=None: rows.append(["info", msg, detail])
    live.check_macos_tcc_grants()
    return compat(rows)


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func, nix = r.get("func"), bool(r.get("nix", False))
    try:
        if func == "install_cmd":
            live.sys = types.SimpleNamespace(platform=r.get("platform", ""))
            live._is_termux = lambda: bool(r.get("termux", False))
            py = live._system_package_install_cmd(r.get("pkg", ""))
        elif func == "repair_hint":
            cfg = sys.modules["hermes_cli.config"]
            cfg.detect_install_method = lambda p: r.get("method", "")
            live.is_nix_install_method = lambda m: nix
            live.recommended_update_command_for_method = lambda m: r.get("cmd", "")
            py = live._python_repair_hint()
        elif func == "sqlite_hint":
            live.is_nix_install_method = lambda m: nix
            live.recommended_update_command_for_method = lambda m: r.get("cmd", "")
            py = live._sqlite_upgrade_hint(r.get("method", ""))
        elif func == "tcc":
            py = _tcc(r.get("dr", ""))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = compat(r.get("out"))
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w13_doctorplat_e064d4: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
