#!/usr/bin/env python3
"""Generate tests/fixtures/doctor_flow2.jsonl (>=5 cases per function).

Each line is {"func": <name>, "in": {...}}; "in.io" carries scripted seam
results ({"ret","raise","state"} or a queue LIST) consumed identically by the
C harness dispatcher and the oracle's recording stubs. AUTO cosmetic seams
(print/color/check_ok/check_warn/check_fail/check_info/_section) need no script.
Pure data - no quarry import.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "doctor_flow2.jsonl")
CASES = []


def case(func, **inp): CASES.append({"func": func, "in": inp})
def R(v): return {"ret": v}
def X(t, message="", **extra):
    d = {"type": t, "message": message}; d.update(extra); return {"raise": d}


# ── plural ────────────────────────────────────────────────────────────────
for n in (0, 1, 2, 3, 10, -1):
    case("plural", n=n)

# ── setup_gated ───────────────────────────────────────────────────────────
case("setup_gated", item={})
case("setup_gated", item={"name": "image_gen"})
case("setup_gated", item={"missing_vars": ["OPENAI_API_KEY"]})
case("setup_gated", item={"env_vars": []})
case("setup_gated", item={"name": "other", "env_vars": []})
case("setup_gated", item={"missing_vars": [], "env_vars": ["K"]})

# ── pm_tool_path ──────────────────────────────────────────────────────────
case("pm_tool_path", name="node", io={"installed_package": R({"binary": "/store/node"})})
case("pm_tool_path", name="node", io={"installed_package": R(None)})
case("pm_tool_path", name="rg", io={"installed_package": R({"binary": "/opt/ripgrep.exe"})})
case("pm_tool_path", name="rg", io={"installed_package": X("ValueError", "bad")})
case("pm_tool_path", name="rg", io={"installed_package": X("RuntimeError", "boom")})
case("pm_tool_path", name="rg", io={"installed_package": X("SystemExit", "1", code=1)})

# ── pm_package_for_command ────────────────────────────────────────────────
def ppkg(**io):
    d = {"current_target": R("linux"), "all_packages": R(["node", "ripgrep"]),
         "get_package": R(None), "_rel": R(None)}
    d.update(io); return d
case("pm_package_for_command", command="node", io=ppkg(
    get_package=[R({"binary": True}), R({"binary": True})],
    _rel=[R("/store/n/bin/node.exe"), R("/store/rg")]))
case("pm_package_for_command", command="rg", io=ppkg(
    get_package=[R({"binary": True}), R({"binary": True})],
    _rel=[R("/store/n/bin/node.exe"), R("/store/rg")]))
case("pm_package_for_command", command="npm", io=ppkg(
    get_package=[R({"binary": True}), R({"binary": True})],
    _rel=[R("/store/node"), R("/store/rg")]))
case("pm_package_for_command", command="node", io=ppkg(
    get_package=[R({"binary": False}), R({"binary": True})], _rel=[R("/store/rg")]))
case("pm_package_for_command", command="node", io=ppkg(all_packages=R([])))
case("pm_package_for_command", command="node", io=ppkg(current_target=X("RuntimeError", "t")))
case("pm_package_for_command", command="node", io=ppkg(get_package=X("OSError", "g")))
case("pm_package_for_command", command="node", io=ppkg(
    get_package=[R({"binary": True})], _rel=X("ValueError", "r")))
case("pm_package_for_command", command="node", io=ppkg(current_target=X("KeyboardInterrupt", "")))

# ── require ───────────────────────────────────────────────────────────────
case("require", cond=True, ok=["ok", "detail"], bad=["bad", "d"], issue="fix it",
     state={"issues": []}, io={"check_bool": R(True)})
case("require", cond=False, ok="ok", bad="bad", issue="fix it",
     state={"issues": []}, io={"check_bool": R(False)})
case("require", cond=False, ok="ok", bad="bad", issue="add issue",
     state={"issues": ["existing"]}, io={"check_bool": R(False)})
case("require", cond="x", ok=["a", "b"], bad=["c", "d"], issue="i2",
     state={"issues": []}, io={"check_bool": R(True)})
case("require", cond=False, ok="ok", bad="bad", issue="i3",
     state={"issues": []}, io={"check_bool": X("RuntimeError", "boom")})
case("require", cond=0, ok="ok", bad="bad", issue="i4",
     state={"issues": []}, io={"check_bool": R(False)})

# ── chromium_not_needed_reason ────────────────────────────────────────────
def chrio(cdp, cfg_q):
    d = {"load_config_readonly": R({}), "environ_get": R(cdp)}
    d["cfg_get"] = [R(v) for v in cfg_q]
    return d
case("chromium_not_needed_reason", io=chrio("http://cdp", []))
case("chromium_not_needed_reason", io=chrio(None, [True]))
case("chromium_not_needed_reason", io=chrio("", [False, "lightpanda"]))
case("chromium_not_needed_reason", io=chrio(None, [False, "  LIGHTPANDA  "]))
case("chromium_not_needed_reason", io=chrio(None, [None, "auto", " AWS "]))
case("chromium_not_needed_reason", io=chrio(None, [None, "auto", "local"]))
case("chromium_not_needed_reason", io={"load_config_readonly": R({}), "environ_get": X("RuntimeError", "e")})

# ── failing_boot_migration ────────────────────────────────────────────────
def fbm(**over):
    d = {"install_root": R("/inst"), "record_path": R("/inst/.last"),
         "read_last_known": R({"step": 3}), "owed_config_migration": R({"failed_warning": "boom"})}
    d.update(over); return d
case("failing_boot_migration", io=fbm())
case("failing_boot_migration", io=fbm(owed_config_migration=R({"error": "oops"})))
case("failing_boot_migration", io=fbm(owed_config_migration=R(None)))
case("failing_boot_migration", io=fbm(install_root=X("ImportError", "no")))
case("failing_boot_migration", io=fbm(record_path=X("FileNotFoundError", "gone")))
case("failing_boot_migration", io=fbm(read_last_known=X("ValueError", "v")))
case("failing_boot_migration", io=fbm(owed_config_migration=X("RuntimeError", "unexpected")))
case("failing_boot_migration", io=fbm(owed_config_migration=X("ModuleNotFoundError", "m")))

# ── drift_deprecations ────────────────────────────────────────────────────
case("drift_deprecations", config_path="/home/cfg.yaml", io={
    "read_user_config_raw": R({"old": 1}), "load_env": R({"X": "1"}),
    "report_deprecated_config_and_env": R(None)})
case("drift_deprecations", config_path=None, io={
    "load_env": R({}), "report_deprecated_config_and_env": R(None)})
case("drift_deprecations", config_path="/home/cfg.yaml", io={
    "read_user_config_raw": R({}), "load_env": X("ValueError", "bad"),
    "report_deprecated_config_and_env": R(None)})
case("drift_deprecations", config_path="/home/cfg.yaml", io={
    "read_user_config_raw": R({}), "load_env": X("SystemExit", "9", code=9)})
case("drift_deprecations", config_path="/home/cfg.yaml", io={
    "read_user_config_raw": X("OSError", "unreadable")})

# ── drift_structure ───────────────────────────────────────────────────────
case("drift_structure", state={"issues": []}, io={"validate_config_structure": R([])})
case("drift_structure", state={"issues": []}, io={"validate_config_structure": R([
    {"severity": "error", "message": "bad thing", "hint": "line a\nline b"}])})
case("drift_structure", state={"issues": []}, io={"validate_config_structure": R([
    {"severity": "warn", "message": "careful", "hint": ""}])})
case("drift_structure", state={"issues": ["pre"]}, io={"validate_config_structure": R([
    {"severity": "error", "message": "E1", "hint": "h1"},
    {"severity": "warn", "message": "W1", "hint": "x\ny\nz"}])})
case("drift_structure", state={"issues": []}, io={"validate_config_structure": X("RuntimeError", "v")})

# ── provider_has_credentials ──────────────────────────────────────────────
case("provider_has_credentials", provider="openrouter", io={"get_env_value": [R(""), R("sk-x")]})
case("provider_has_credentials", provider="openrouter", io={"get_env_value": [R("sk-1")]})
case("provider_has_credentials", provider="openrouter", io={"get_env_value": [R(""), R("")]})
case("provider_has_credentials", provider="openai", io={
    "provider_registry": R({"openai": {"auth_type": "api_key"}}),
    "get_auth_status": R({"configured": False, "logged_in": True, "api_key": None})})
case("provider_has_credentials", provider="openai", io={
    "provider_registry": R({"openai": {"auth_type": "api_key"}}),
    "get_auth_status": R({"configured": False})})
case("provider_has_credentials", provider="custom", io={"provider_registry": R({})})
case("provider_has_credentials", provider="xai", io={
    "provider_registry": R({"xai": {"auth_type": "oauth"}})})
case("provider_has_credentials", provider="openai", io={"provider_registry": X("RuntimeError", "r")})

# ── python_repair_hint ────────────────────────────────────────────────────
def prh(method, nix, cmd):
    return {"detect_install_method": R(method), "is_nix_install_method": R(nix),
            "recommended_update_command_for_method": R(cmd)}
case("python_repair_hint", io=prh("nix", True, "nix shell nixpkgs#hermes"))
case("python_repair_hint", io=prh("docker", False, "docker pull hermes"))
case("python_repair_hint", io=prh("apt", False, "sudo apt install hermes-agent"))
case("python_repair_hint", io=prh("pip", False, "hermes update"))
case("python_repair_hint", io=prh("nix", True, "nix profile install"))
case("python_repair_hint", io={"detect_install_method": X("OSError", "d")})
case("python_repair_hint", io={"detect_install_method": R("docker"),
                              "is_nix_install_method": X("RuntimeError", "i")})

# ── unreadable_reason ─────────────────────────────────────────────────────
case("unreadable_reason", path="/h/state.db", io={"path_stat": R(None), "os_access": R(True)})
case("unreadable_reason", path="/h/state.db", io={"path_stat": R(None), "os_access": R(False)})
case("unreadable_reason", path="/h/state.db", io={"path_stat": X("FileNotFoundError", "missing")})
case("unreadable_reason", path="/h/state.db", io={"path_stat": X("PermissionError", "denied")})
case("unreadable_reason", path="/h/state.db", io={"path_stat": X("RuntimeError", "boom")})
case("unreadable_reason", path="/h/state.db", io={"path_stat": X("SystemExit", "1", code=1)})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES: counts[c["func"]] = counts.get(c["func"], 0) + 1
    assert all(v >= 5 for v in counts.values()), counts
    print(f"wrote {len(CASES)} cases across {len(counts)} funcs -> {OUT}")
    print(json.dumps(counts, sort_keys=True))


if __name__ == "__main__":
    main()
