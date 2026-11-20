#!/usr/bin/env python3
"""Generate tests/fixtures/doctor_tools_flow2.jsonl (>=5 cases per function).

Each line is {"func": <name>, "in": {...}}; "in.io" carries scripted seam
results ({"ret","raise","state"} or a queue LIST) consumed identically by the
C harness dispatcher and the oracle's recording stubs. AUTO report seams need
no script. Pure data — no quarry import.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "doctor_tools_flow2.jsonl")
CASES = []


def case(func, **inp): CASES.append({"func": func, "in": inp})
def R(v): return {"ret": v}
def X(t, message="", **extra):
    d = {"type": t, "message": message}; d.update(extra); return {"raise": d}


# ── doctor_tool ───────────────────────────────────────────────────────────
case("doctor_tool", name="git", io={"pm_package_for_command": R(None),
     "pm_tool_path": R("/store/git"), "safe_which": R("/usr/bin/git")})
case("doctor_tool", name="rg", io={"pm_package_for_command": R("ripgrep"),
     "pm_tool_path": [R(None), R("/store/rg")]})
case("doctor_tool", name="git", io={"pm_package_for_command": R(None),
     "pm_tool_path": R(None), "safe_which": R("/usr/bin/git")})
case("doctor_tool", name="rg", io={"pm_package_for_command": X("ValueError", "bad")})
case("doctor_tool", name="rg", io={"pm_package_for_command": R("ripgrep"),
     "pm_tool_path": X("SystemExit", "1", code=1)})
case("doctor_tool", name="rg", io={"pm_package_for_command": R(None),
     "pm_tool_path": R(None), "safe_which": X("RuntimeError", "which")})

# ── run_ok ────────────────────────────────────────────────────────────────
case("run_ok", cmd=["true"], timeout=10, io={"subprocess_run": R({"returncode": 0})})
case("run_ok", cmd=["false"], timeout=10, io={"subprocess_run": R({"returncode": 1})})
case("run_ok", cmd=["echo", "ok"], timeout=5, io={"subprocess_run": R({"returncode": 0})})
case("run_ok", cmd=["slow"], timeout=3, io={"subprocess_run": X("TimeoutExpired", "timeout")})
case("run_ok", cmd=["x"], timeout=1, io={"subprocess_run": X("RuntimeError", "boom")})
case("run_ok", cmd=["x"], timeout=1, io={"subprocess_run": X("SystemExit", "2", code=2)})

# ── doctor_web_capability_rows ────────────────────────────────────────────
case("doctor_web_capability_rows", io={
    "ensure_web_plugins_loaded": R(None),
    "get_active_search_provider": R({"name": "brave"}),
    "get_active_extract_provider": R({"name": "tavily"}),
    "_provider_is_ready": [R(True), R(False)]})
case("doctor_web_capability_rows", io={
    "ensure_web_plugins_loaded": R(None),
    "get_active_search_provider": R(None),
    "get_active_extract_provider": R(None)})
case("doctor_web_capability_rows", io={"ensure_web_plugins_loaded": X("ValueError", "x")})
case("doctor_web_capability_rows", io={"ensure_web_plugins_loaded": X("SystemExit", "1", code=1)})
case("doctor_web_capability_rows", io={
    "ensure_web_plugins_loaded": R(None),
    "get_active_search_provider": X("RuntimeError", "s"),
    "get_active_extract_provider": R(None)})
case("doctor_web_capability_rows", io={
    "ensure_web_plugins_loaded": R(None),
    "get_active_search_provider": R({"name": None}),
    "get_active_extract_provider": R(None),
    "_provider_is_ready": R(True)})
case("doctor_web_capability_rows", io={
    "ensure_web_plugins_loaded": R(None),
    "get_active_search_provider": R({"name": "x"}),
    "_provider_is_ready": X("RuntimeError", "r")})

# ── check_git_and_rg ──────────────────────────────────────────────────────
def gir(**io):
    d = {"pm_package_for_command": [R(None), R(None)],
         "pm_tool_path": [R("/usr/bin/git"), R("/usr/bin/rg")],
         "safe_which": [R(""), R("")],
         "check_bool": [R(True), R(True)],
         "system_package_install_cmd": R("apt install ripgrep")}
    d.update(io); return d
case("check_git_and_rg", io=gir())
case("check_git_and_rg", io=gir(check_bool=[R(True), R(False)]))
case("check_git_and_rg", io=gir(pm_package_for_command=[X("RuntimeError", "git")]))
case("check_git_and_rg", io=gir(check_bool=[R(True), X("RuntimeError", "cb")]))
case("check_git_and_rg", io=gir(pm_tool_path=[R(""), R("")], check_bool=[R(False), R(False)]))
case("check_git_and_rg", io=gir(check_bool=[R(True), R(False)], system_package_install_cmd=X("ValueError", "s")))

# ── check_docker_backend ──────────────────────────────────────────────────
case("check_docker_backend", terminal_env="local", running_in_container=False,
     io={"find_docker": R(None), "is_termux": R(False)})
case("check_docker_backend", terminal_env="local", running_in_container=False,
     io={"find_docker": R("/usr/bin/docker"), "docker_runtime_name": R("Docker")})
case("check_docker_backend", terminal_env="docker", running_in_container=False,
     io={"find_docker": R(None)})
case("check_docker_backend", terminal_env="docker", running_in_container=False, io={
    "find_docker": R("/usr/bin/docker"), "docker_runtime_name": R("Docker"),
    "docker_runtime_start_hint": R("start docker daemon"),
    "subprocess_run": R({"returncode": 0}), "check_bool": R(True)})
case("check_docker_backend", terminal_env="docker", running_in_container=False, io={
    "find_docker": R("/usr/bin/podman"), "docker_runtime_name": R("Podman"),
    "docker_runtime_start_hint": R("enable podman"),
    "subprocess_run": R({"returncode": 1}), "check_bool": R(False)})
case("check_docker_backend", terminal_env="local", running_in_container=True,
     io={"find_docker": R(None), "is_termux": R(True)})
case("check_docker_backend", terminal_env="local", running_in_container=False,
     io={"find_docker": R(None), "is_termux": X("RuntimeError", "t")})
case("check_docker_backend", terminal_env="docker", running_in_container=False,
     io={"find_docker": R("/d"), "docker_runtime_name": X("OSError", "n")})

# ── check_ssh_backend ─────────────────────────────────────────────────────
def ssh(**io):
    d = {"os_getenv": [R("host"), R("user"), R("2222"), R("~/.ssh/key")],
         "expanduser": R("/home/u/.ssh/key"),
         "subprocess_run": R({"returncode": 0}), "check_bool": R(True)}
    d.update(io); return d
case("check_ssh_backend", io=ssh())
case("check_ssh_backend", io=ssh(os_getenv=R(None)))
case("check_ssh_backend", io=ssh(os_getenv=[R("host"), R(""), R(""), R("")], check_bool=R(False)))
case("check_ssh_backend", io=ssh(expanduser=X("RuntimeError", "e")))
case("check_ssh_backend", io=ssh(subprocess_run=X("TimeoutExpired", "t"), check_bool=R(False)))
case("check_ssh_backend", io=ssh(check_bool=X("OSError", "cb")))

# ── check_daytona_backend ─────────────────────────────────────────────────
case("check_daytona_backend", io={"os_getenv": R("key"), "check_bool": R(True)})
case("check_daytona_backend", io={"os_getenv": R(None), "check_bool": R(False)})
case("check_daytona_backend", io={"os_getenv": R(""), "check_bool": R(False)})
case("check_daytona_backend", io={"os_getenv": X("RuntimeError", "g")})
case("check_daytona_backend", io={"os_getenv": R("key"), "check_bool": X("ValueError", "cb")})

# ── check_vercel_backend ──────────────────────────────────────────────────
def ver(**io):
    d = {"vercel_runtimes": R(["node20", "node22", "node24"]),
         "os_getenv": [R("node24"), R("51200"), R("true")],
         "check_bool": [R(True), R(True), R(True)],
         "find_spec": R({}),
         "describe_vercel_auth": R({"ok": True, "label": "ready", "detail_lines": []})}
    d.update(io); return d
case("check_vercel_backend", io=ver())
case("check_vercel_backend", io=ver(os_getenv=[R("bad"), R("51200"), R("false")],
     check_bool=[R(False), R(True), R(True)]))
case("check_vercel_backend", io=ver(describe_vercel_auth=R({"ok": False, "label": "partial-token", "detail_lines": ["line a", "line b"]})))
case("check_vercel_backend", io=ver(describe_vercel_auth=R({"ok": False, "label": "none", "detail_lines": ["fix me"]})))
case("check_vercel_backend", io=ver(find_spec=R(None), check_bool=[R(True), R(True), R(False)]))
case("check_vercel_backend", io=ver(os_getenv=X("RuntimeError", "g")))
case("check_vercel_backend", io=ver(describe_vercel_auth=X("OSError", "auth")))

# ── check_plugin_backend ──────────────────────────────────────────────────
case("check_plugin_backend", terminal_env="weird", io={
    "discover_plugins": R(None), "get_provider": R({"x": 1}),
    "doctor_checks": R([[True, "L", "(detail)"], [False, "L2", "(d2)"]]),
    "check_bool": [R(True), R(False)]})
case("check_plugin_backend", terminal_env="weird", io={
    "discover_plugins": X("ValueError", "d"), "get_provider": R(None)})
case("check_plugin_backend", terminal_env="weird", io={
    "discover_plugins": R(None), "get_provider": R(None)})
case("check_plugin_backend", terminal_env="weird", io={
    "discover_plugins": R(None), "get_provider": X("SystemExit", "9", code=9)})
case("check_plugin_backend", terminal_env="weird", io={
    "discover_plugins": R(None), "get_provider": R({"x": 1}),
    "doctor_checks": X("RuntimeError", "checks")})

# ── check_terminal_backend ────────────────────────────────────────────────
case("check_terminal_backend", io={
    "os_getenv": R("local"), "is_container": R(False),
    "find_docker": R(None), "is_termux": R(False)})
case("check_terminal_backend", io={
    "os_getenv": R("docker"), "is_container": R(False), "find_docker": R(None)})
case("check_terminal_backend", io={
    "os_getenv": R("local"), "is_container": R(True),
    "find_docker": R(None), "is_termux": R(False)})
case("check_terminal_backend", io={
    "os_getenv": [R("ssh"), R("host"), R("user"), R(""), R("")],
    "is_container": R(False),
    "find_docker": R(None), "is_termux": R(False),
    "subprocess_run": R({"returncode": 0}), "check_bool": R(True)})
case("check_terminal_backend", io={
    "os_getenv": [R("daytona"), R("key")], "is_container": R(False),
    "find_docker": R(None), "is_termux": R(False),
    "check_bool": R(True)})
case("check_terminal_backend", io={
    "os_getenv": R("weird"), "is_container": R(False),
    "find_docker": R(None), "is_termux": R(False),
    "discover_plugins": R(None), "get_provider": R(None)})
case("check_terminal_backend", io={"os_getenv": R("local"), "is_container": X("ValueError", "c"),
     "find_docker": R(None), "is_termux": R(False)})

# ── check_chromium ────────────────────────────────────────────────────────
case("check_chromium", should_fix=False, io={"chromium_executable": R("/usr/bin/chromium")})
case("check_chromium", should_fix=False, io={
    "chromium_executable": R(None), "chromium_not_needed_reason": R("a CDP override is set")})
case("check_chromium", should_fix=False, io={
    "chromium_executable": R(None), "chromium_not_needed_reason": R(""),
    "chromium_missing_hint": R("install chromium")})
case("check_chromium", should_fix=True, io={
    "chromium_executable": [R(None), R("/found")], "is_termux": R(False), "pm_ensure": R(None)})
case("check_chromium", should_fix=True, io={
    "chromium_executable": [R(None), R("/found")], "is_termux": R(False),
    "pm_ensure": X("RuntimeError", "boom")})
case("check_chromium", should_fix=True, io={
    "chromium_executable": R(None), "is_termux": R(False), "pm_ensure": X("SystemExit", "1", code=1)})
case("check_chromium", should_fix=False, io={"chromium_executable": R(None),
     "chromium_not_needed_reason": X("RuntimeError", "reason")})

# ── check_lightpanda ──────────────────────────────────────────────────────
case("check_lightpanda", io={"using_lightpanda_engine": R(False)})
case("check_lightpanda", io={
    "using_lightpanda_engine": R(True), "lightpanda_engine_status": R([False, "shadowed"])})
case("check_lightpanda", io={
    "using_lightpanda_engine": R(True), "lightpanda_engine_status": X("RuntimeError", "boom")})
case("check_lightpanda", io={
    "using_lightpanda_engine": R(True), "lightpanda_engine_status": R([True, "ok"]),
    "find_lightpanda_binary": R("/bin/lp"), "check_bool": R(True)})
case("check_lightpanda", io={
    "using_lightpanda_engine": R(True), "lightpanda_engine_status": R([True, "ok"]),
    "find_lightpanda_binary": R(None), "check_bool": R(False),
    "LIGHTPANDA_INSTALL_HINT": R("install hint")})
case("check_lightpanda", io={"using_lightpanda_engine": X("OSError", "u")})

# ── check_node_and_browser ────────────────────────────────────────────────
case("check_node_and_browser", should_fix=False, io={
    "pm_tool_path": R("/store/node"), "chromium_executable": R("/chrome"),
    "using_lightpanda_engine": R(False)})
case("check_node_and_browser", should_fix=False, io={
    "pm_tool_path": R(None), "is_termux": [R(False), R(False)],
    "chromium_executable": R(None), "chromium_not_needed_reason": R("reason"),
    "using_lightpanda_engine": R(False)})
case("check_node_and_browser", should_fix=False, io={
    "pm_tool_path": R(None), "is_termux": R(True), "safe_which": R("/usr/bin/node"),
    "chromium_executable": R("/chrome"), "using_lightpanda_engine": R(False)})
case("check_node_and_browser", should_fix=False, io={
    "pm_tool_path": R(None), "is_termux": [R(True), R(True)], "safe_which": R(None),
    "chromium_executable": R("/chrome"), "using_lightpanda_engine": R(False)})
case("check_node_and_browser", should_fix=False, io={"pm_tool_path": X("RuntimeError", "pm")})

# ── audit_one ─────────────────────────────────────────────────────────────
def aud(vulns, extra=None):
    io = {"with_hermes_node_path": R({}), "subprocess_run": R({"stdout": "{}"}),
          "json_loads": R({"metadata": {"vulnerabilities": vulns}})}
    if extra: io["pnpm_env"] = R({})
    return io
case("audit_one", tool="npm", cwd="/app", label="Bridge", audit_extra=[], io=aud({}))
case("audit_one", tool="pnpm", cwd="/proj", label="Root npm package", audit_extra=["--filter", "."], io=aud({}, extra=True))
case("audit_one", tool="pnpm", cwd="/proj", label="web workspace", audit_extra=["--filter", "{./web}..."],
     io=aud({"critical": 2, "high": 1, "moderate": 0}, extra=True))
case("audit_one", tool="pnpm", cwd="/proj", label="Root npm package", audit_extra=["--filter", "."],
     io=aud({"critical": 2, "high": 1, "moderate": 0}, extra=True))
case("audit_one", tool="npm", cwd="/app", label="Bridge", audit_extra=[], io=aud({"moderate": 1}))
case("audit_one", tool="npm", cwd="/app", label="Bridge", audit_extra=[], io={
    "with_hermes_node_path": R({}), "subprocess_run": X("OSError", "no lockfile")})
case("audit_one", tool="npm", cwd="/app", label="Bridge", audit_extra=[], io={
    "with_hermes_node_path": R({}), "subprocess_run": R({"stdout": "not json"}),
    "json_loads": X("ValueError", "bad json")})
case("audit_one", tool="npm", cwd="/app", label="Bridge", audit_extra=[], io={"with_hermes_node_path": X("RuntimeError", "env")})

# ── check_npm_audit ───────────────────────────────────────────────────────
def npm(**io):
    d = {"project_root": R("/proj"), "pm_tool_path": [R(None), R(None)],
         "is_termux": R(False), "resolve_whatsapp_bridge_dir": R("/proj/scripts/whatsapp-bridge"),
         "path_exists": [R(False), R(False), R(False), R(False)]}
    d.update(io); return d
case("check_npm_audit", io=npm())
case("check_npm_audit", io=npm(pm_tool_path=[R("/store/npm"), R(None)]))
case("check_npm_audit", io=npm(resolve_whatsapp_bridge_dir=X("ValueError", "resolve")))
case("check_npm_audit", io=npm(path_exists=X("OSError", "pe")))
case("check_npm_audit", io=npm(is_termux=R(True), safe_which=R(None)))
case("check_npm_audit", io=npm(path_exists=[R(True), R(False), R(False), R(False)],
     with_hermes_node_path=R({}), subprocess_run=R({"stdout": "{}"}),
     json_loads=R({"metadata": {"vulnerabilities": {}}})))
case("check_npm_audit", io=npm(project_root=X("RuntimeError", "root")))

# ── check_tool_availability ───────────────────────────────────────────────
def tav(**io):
    d = {"project_root": R("/proj"),
         "check_tool_availability": R({"available": ["skills", "web"], "unavailable": []}),
         "apply_tool_availability_overrides": R({"available": ["skills", "web"], "unavailable": []}),
         "doctor_web_capability_rows": R([["ok", "web search", "(brave)"]]),
         "doctor_tool_availability_detail": R("(ok)"),
         "missing_api_key_toolsets": R([]),
         "load_config": R({}), "get_platform_tools": R([]), "toolsets_partially_stripped": R({})}
    d.update(io); return d
case("check_tool_availability", io=tav())
case("check_tool_availability", io=tav(
    check_tool_availability=R({"available": ["skills"], "unavailable": []}),
    apply_tool_availability_overrides=R({"available": ["skills"], "unavailable": []})))
case("check_tool_availability", io=tav(
    check_tool_availability=R({"available": [], "unavailable": [{"name": "browser", "missing_vars": ["KEY"]}]}),
    apply_tool_availability_overrides=R({"available": [], "unavailable": [{"name": "browser", "missing_vars": ["KEY"]}]})))
case("check_tool_availability", io=tav(
    doctor_web_capability_rows=R([["warn", "web search", "(no provider selected or registered)"]])))
case("check_tool_availability", io=tav(
    check_tool_availability=R({"available": [], "unavailable": [{"name": "image_gen"}]}),
    apply_tool_availability_overrides=R({"available": [], "unavailable": [{"name": "image_gen"}]})))
case("check_tool_availability", io=tav(check_tool_availability=X("RuntimeError", "cta")))
case("check_tool_availability", io=tav(missing_api_key_toolsets=R([{"name": "x"}])))

# ── warn_partially_stripped_toolsets ──────────────────────────────────────
case("warn_partially_stripped_toolsets", io={
    "load_config": R({}), "get_platform_tools": R([]), "toolsets_partially_stripped": R({})})
case("warn_partially_stripped_toolsets", io={
    "load_config": R({}), "get_platform_tools": R(["skills"]),
    "toolsets_partially_stripped": R({"skills": [["a", "b"], ["reason1"], "hint text"]})})
case("warn_partially_stripped_toolsets", io={
    "load_config": R({}), "get_platform_tools": R([]),
    "toolsets_partially_stripped": R({"web": [[], [], "x"]})})
case("warn_partially_stripped_toolsets", io={"load_config": X("OSError", "cfg")})
case("warn_partially_stripped_toolsets", io={
    "load_config": R({}), "get_platform_tools": X("ValueError", "gpt")})
case("warn_partially_stripped_toolsets", io={
    "load_config": R({}), "get_platform_tools": R([]),
    "toolsets_partially_stripped": X("RuntimeError", "tps")})


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
