#!/usr/bin/env python3
"""Generate tests/fixtures/tools_config_flow2.jsonl for the tools_config.py
UNCLAIMED FLOW oracle (src/cli/tools_config_flow2.c).

Each line is {"func": <python-name>, "in": {...}}.  "in.io" carries scripted
seam results ({"ret","raise","state"} or a queue LIST) consumed identically by
the C harness dispatcher and the oracle's recording stubs.  "in.constants"
carries the module constants, "in.state" the seeded mutable module globals.

List-valued seam results are wrapped in {"ret": [...]} so a queue is never
confused with a single list result.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "tools_config_flow2.jsonl")
CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def R(v):
    return {"ret": v}


def X(t, msg="", **extra):
    d = {"type": t, "message": msg}
    d.update(extra)
    return {"raise": d}


CFG = ["web", "file", "terminal", "x_search", "browser", "kanban"]
DOFF = ["spotify", "discord", "video", "video_gen", "x_search", "kanban"]
RESTR = {"discord": ["telegram"], "discord_admin": ["discord"]}
PLAT = {"telegram": {"label": "Telegram", "default_toolset": "hermes-telegram"},
        "discord": {"label": "Discord", "default_toolset": "hermes-discord"}}
CATS = {"web": {"name": "web"}, "image_gen": {"name": "img"}}
ENVR = {"file": [("FILE_KEY", "file")], "tts": [("TTS_KEY", "tts")]}
CONST = {"configurable_toolsets": [[k, k.upper(), "d"] for k in CFG],
         "default_off_toolsets": DOFF, "toolset_platform_restrictions": RESTR,
         "platforms": PLAT, "tool_categories": CATS,
         "toolset_env_requirements": ENVR}


# ── 1. _coerce_platform_toolsets_value ───────────────────────────────────
case("_coerce_platform_toolsets_value", value=None, platform="telegram", io={}, state={"warned": []})
case("_coerce_platform_toolsets_value", value="['web', 'file']", platform="telegram",
     io={"parse_toolsets_value": R(["web", "file"])}, state={"warned": []})
case("_coerce_platform_toolsets_value", value="garbage", platform="telegram",
     io={"parse_toolsets_value": R(None), "logger.warning": R(None)}, state={"warned": []})
case("_coerce_platform_toolsets_value", value="garbage", platform="telegram",
     io={"parse_toolsets_value": R(None)}, state={"warned": ["telegram"]})
case("_coerce_platform_toolsets_value", value=123, platform="discord",
     io={"parse_toolsets_value": R(None), "logger.warning": R(None)}, state={"warned": []})
case("_coerce_platform_toolsets_value", value="x", platform="telegram",
     io={"parse_toolsets_value": X("ValueError", "bad")}, state={"warned": []})

# ── 2. _platform_toolsets_explicitly_saved ───────────────────────────────
case("_platform_toolsets_explicitly_saved", config={"platform_toolsets": {"telegram": ["web"]}},
     platform="telegram", io={"parse_toolsets_value": R(["web"])}, state={"warned": []})
case("_platform_toolsets_explicitly_saved", config={}, platform="telegram", io={}, state={"warned": []})
case("_platform_toolsets_explicitly_saved", config={"platform_toolsets": {"telegram": "['web']"}},
     platform="telegram", io={"parse_toolsets_value": R(["web"])}, state={"warned": []})
case("_platform_toolsets_explicitly_saved", config={"platform_toolsets": {"telegram": "garbage"}},
     platform="telegram", io={"parse_toolsets_value": R(None), "logger.warning": R(None)}, state={"warned": []})
case("_platform_toolsets_explicitly_saved", config={"platform_toolsets": {"telegram": "x"}},
     platform="telegram", io={"parse_toolsets_value": X("RuntimeError", "boom")}, state={"warned": []})

# ── 3. _default_off_toolsets ─────────────────────────────────────────────
case("_default_off_toolsets", platform="telegram", explicitly_configured=False,
     constants={"default_off_toolsets": DOFF, "toolset_platform_restrictions": RESTR})
case("_default_off_toolsets", platform="discord", explicitly_configured=False,
     constants={"default_off_toolsets": DOFF, "toolset_platform_restrictions": {}})
case("_default_off_toolsets", platform="telegram", explicitly_configured=True,
     constants={"default_off_toolsets": DOFF, "toolset_platform_restrictions": RESTR})
case("_default_off_toolsets", platform="discord", explicitly_configured=False,
     constants={"default_off_toolsets": DOFF, "toolset_platform_restrictions": RESTR})
case("_default_off_toolsets", platform="x", explicitly_configured=True,
     constants={"default_off_toolsets": [], "toolset_platform_restrictions": {}})

# ── 4. _configurable_subset_of ───────────────────────────────────────────
C4 = {"constants": {"configurable_toolsets": CONST["configurable_toolsets"]}}
case("_configurable_subset_of", tool_names=["web_search"], platform="telegram",
     io={"toolset_allowed_for_platform": R(True), "resolve_toolset": R(["web_search"])}, **C4)
case("_configurable_subset_of", tool_names=["web_search"], platform="telegram",
     io={"toolset_allowed_for_platform": R(True), "resolve_toolset": R([])}, **C4)
case("_configurable_subset_of", tool_names=["web_search"], platform="telegram",
     io={"toolset_allowed_for_platform": R(True), "resolve_toolset": R(["web_search", "extra"])}, **C4)
case("_configurable_subset_of", tool_names=["web_search"], platform="telegram",
     io={"toolset_allowed_for_platform": R(False)}, **C4)
case("_configurable_subset_of", tool_names=["web_search"], platform="telegram",
     io={"toolset_allowed_for_platform": X("RuntimeError", "gate")}, **C4)
case("_configurable_subset_of", tool_names=[], platform="telegram", io={}, **C4)

# ── 5. _composite_toolsets ───────────────────────────────────────────────
C5 = {"constants": CONST}
case("_composite_toolsets", toolset_names=["hermes-telegram"], platform="telegram", explicitly_configured=False,
     io={"resolve_toolset": [R(["web_search", "file_read"]), R(["web_search", "file_read"])],
         "toolset_allowed_for_platform": [R(True), R(True), R(False), R(False), R(False), R(False)],
         "xai_credentials_present": R(False)}, **C5)
case("_composite_toolsets", toolset_names=[], platform="telegram", explicitly_configured=False,
     io={"resolve_toolset": R([]), "toolset_allowed_for_platform": [R(False)]}, **C5)
case("_composite_toolsets", toolset_names=["hermes-telegram"], platform="telegram", explicitly_configured=True,
     io={"resolve_toolset": R(["web_search", "x_search_tools"]),
         "toolset_allowed_for_platform": [R(False), R(False), R(False), R(False), R(False), R(False), R(True)],
         "xai_credentials_present": R(True)}, **C5)
case("_composite_toolsets", toolset_names=["hermes-telegram"], platform="telegram", explicitly_configured=False,
     io={"resolve_toolset": X("ValueError", "resolve")}, **C5)
case("_composite_toolsets", toolset_names=["hermes-telegram"], platform="telegram", explicitly_configured=False,
     io={"resolve_toolset": [R(["web_search"]), R(["web_search"])],
         "toolset_allowed_for_platform": R(True), "xai_credentials_present": X("OSError", "xai")}, **C5)

# ── 6. _enabled_plugin_toolsets ──────────────────────────────────────────
C6 = {"constants": {"default_off_toolsets": DOFF}}
case("_enabled_plugin_toolsets", config={}, platform="telegram",
     toolset_names=["spotify"], plugin_ts_keys=["spotify", "ps1"], **C6)
case("_enabled_plugin_toolsets", config={}, platform="telegram",
     toolset_names=[], plugin_ts_keys=["ps1", "ps2"], **C6)
case("_enabled_plugin_toolsets", config={"known_plugin_toolsets": {"telegram": ["ps1"]}},
     platform="telegram", toolset_names=[], plugin_ts_keys=["ps1", "ps2"], **C6)
case("_enabled_plugin_toolsets", config={"known_plugin_toolsets": {"telegram": ["ps1"]}},
     platform="telegram", toolset_names=["ps1"], plugin_ts_keys=["ps1"], **C6)
case("_enabled_plugin_toolsets", config={"known_plugin_toolsets": None}, platform="discord",
     toolset_names=[], plugin_ts_keys=["discord", "ps3"], **C6)

# ── 7. _context_engine_active ────────────────────────────────────────────
case("_context_engine_active", config={"context": {"engine": "custom"}}, io={})
case("_context_engine_active", config={"context": {"engine": "compressor"}}, io={})
case("_context_engine_active", config={"context": {"engine": "  Compressor  "}}, io={})
case("_context_engine_active", config={"context": {"engine": ""}}, io={})
case("_context_engine_active", config={}, io={})
case("_context_engine_active", config={"context": "not-a-dict"}, io={})

# ── 8. _merge_mcp_servers ────────────────────────────────────────────────
case("_merge_mcp_servers", config={}, toolset_names=["no_mcp"],
     explicit_passthrough=["srv1", "mcp-srv2"], include_default_mcp_servers=True,
     io={"enabled_mcp_server_names": R(["srv1", "srv2"])})
case("_merge_mcp_servers", config={}, toolset_names=[],
     explicit_passthrough=[], include_default_mcp_servers=True,
     io={"enabled_mcp_server_names": R(["srv1", "srv2"])})
case("_merge_mcp_servers", config={}, toolset_names=["srv1"],
     explicit_passthrough=["srv1"], include_default_mcp_servers=True,
     io={"enabled_mcp_server_names": R(["srv1", "srv2"])})
case("_merge_mcp_servers", config={"mcp_servers": {"configured": 1}}, toolset_names=[],
     explicit_passthrough=["mcp-configured"], include_default_mcp_servers=False,
     io={"enabled_mcp_server_names": R([])})
case("_merge_mcp_servers", config={}, toolset_names=[], explicit_passthrough=[],
     include_default_mcp_servers=True, io={"enabled_mcp_server_names": X("RuntimeError", "mcp")})

# ── 9. _prune_toolsets_stripped_by_disabled ──────────────────────────────
case("_prune_toolsets_stripped_by_disabled", enabled_toolsets=["web"], disabled_names=["search"],
     io={"resolve_toolset": R(["web_search", "web_extract"]), "validate_toolset": R(True),
         "apply_toolset_selection": R(["web_extract"])})
case("_prune_toolsets_stripped_by_disabled", enabled_toolsets=["web"], disabled_names=["web"],
     io={"resolve_toolset": R(["web_search"]), "validate_toolset": R(True),
         "apply_toolset_selection": R([])})
case("_prune_toolsets_stripped_by_disabled", enabled_toolsets=["context_engine"], disabled_names=[],
     io={"validate_toolset": R(False), "apply_toolset_selection": R([])})
case("_prune_toolsets_stripped_by_disabled", enabled_toolsets=["web"], disabled_names=["search"],
     io={"resolve_toolset": R(["web_search", "web_extract"]), "validate_toolset": R(True),
         "apply_toolset_selection": R([])})
case("_prune_toolsets_stripped_by_disabled", enabled_toolsets=["web"], disabled_names=[],
     io={"validate_toolset": R(True), "resolve_toolset": X("ValueError", "resolve")})

# ── 10. _disabled_entry_tools ────────────────────────────────────────────
case("_disabled_entry_tools", name="search",
     io={"apply_toolset_selection": [R(["web_search", "web_extract"]), R(["web_extract"])]})
case("_disabled_entry_tools", name="web",
     io={"apply_toolset_selection": [R([]), R([])]})
case("_disabled_entry_tools", name="search",
     io={"apply_toolset_selection": [R(["a", "b"]), R(["b"])]})
case("_disabled_entry_tools", name="search",
     io={"apply_toolset_selection": X("RuntimeError", "sel")})
case("_disabled_entry_tools", name="search",
     io={"apply_toolset_selection": [R(["a"]), X("OSError", "sel2")]})

# ── 11. _platform_default_toolset ────────────────────────────────────────
for plat in ["telegram", "discord", "whatsapp", ""]:
    case("_platform_default_toolset", platform=plat, constants={"platforms": PLAT}, io={})
case("_platform_default_toolset", platform="telegram", constants={}, io={})

# ── 12. _is_configurable ─────────────────────────────────────────────────
for key in ["web", "file", "tts", "unknown"]:
    case("_is_configurable", ts_key=key, constants={"tool_categories": CATS, "toolset_env_requirements": ENVR})
case("_is_configurable", ts_key="web", constants={})

# ── 13. _toolset_label ───────────────────────────────────────────────────
CE = {"constants": {}}
case("_toolset_label", ts_key="web", io={"get_effective_configurable_toolsets": R([["web", "Web", "d"], ["file", "File", "d"]])}, **CE)
case("_toolset_label", ts_key="xyz", io={"get_effective_configurable_toolsets": R([["web", "Web", "d"]])}, **CE)
case("_toolset_label", ts_key="web", io={"get_effective_configurable_toolsets": R([])}, **CE)
case("_toolset_label", ts_key="web", io={"get_effective_configurable_toolsets": X("RuntimeError", "eff")}, **CE)
case("_toolset_label", ts_key="file", io={"get_effective_configurable_toolsets": R([["web", "Web", "d"], ["file", "Files", "d"]])}, **CE)

# ── 14. _configurable_keys ───────────────────────────────────────────────
case("_configurable_keys", io={}, constants={"configurable_toolsets": CONST["configurable_toolsets"]})
case("_configurable_keys", io={}, constants={"configurable_toolsets": [["z", "Z", ""], ["a", "A", ""]]})
case("_configurable_keys", io={}, constants={"configurable_toolsets": []})
case("_configurable_keys", io={}, constants={})
case("_configurable_keys", io={}, constants={"configurable_toolsets": [["dup", "1", ""], ["dup", "2", ""]]})

# ── 15. _platform_default_keys ───────────────────────────────────────────
case("_platform_default_keys", io={}, constants={"platforms": PLAT})
case("_platform_default_keys", io={}, constants={"platforms": {"a": {"default_toolset": "z"}, "b": {"default_toolset": "a"}}})
case("_platform_default_keys", io={}, constants={"platforms": {}})
case("_platform_default_keys", io={}, constants={})
case("_platform_default_keys", io={}, constants={"platforms": {"a": {"default_toolset": ""}}})

# ── 16. _provider_env_ready ──────────────────────────────────────────────
case("_provider_env_ready", provider={"env_vars": []}, io={})
case("_provider_env_ready", provider={"env_vars": [{"key": "A"}, {"key": "B"}]},
     io={"get_env_value": [R("x"), R("y")]})
case("_provider_env_ready", provider={"env_vars": [{"key": "A"}, {"key": "B"}]},
     io={"get_env_value": [R("x"), R("")]})
case("_provider_env_ready", provider={}, io={})
case("_provider_env_ready", provider={"env_vars": [{"key": "A"}]},
     io={"get_env_value": X("OSError", "env")})

# ── 17. _projected_plugin_toolsets ───────────────────────────────────────
case("_projected_plugin_toolsets", io={}, state={"gateway_provider": False})
case("_projected_plugin_toolsets", io={"gateway_provider": R([["p1", "P1", "d"]])},
     state={"gateway_provider": True})
case("_projected_plugin_toolsets", io={"gateway_provider": R(None)}, state={"gateway_provider": True})
case("_projected_plugin_toolsets", io={"gateway_provider": X("RuntimeError", "provider")},
     state={"gateway_provider": True})
case("_projected_plugin_toolsets", io={}, state={})

# ── 18. _print_toolset_diff ──────────────────────────────────────────────
case("_print_toolset_diff", added=["web"], removed=[], indent="  ",
     io={"get_effective_configurable_toolsets": R([["web", "Web", "d"]])})
case("_print_toolset_diff", added=[], removed=["file"], indent="  ",
     io={"get_effective_configurable_toolsets": R([["file", "Files", "d"]])})
case("_print_toolset_diff", added=["web"], removed=["file"], indent="    ",
     io={"get_effective_configurable_toolsets": R([["web", "Web", "d"], ["file", "Files", "d"]])})
case("_print_toolset_diff", added=[], removed=[], indent="  ", io={})
case("_print_toolset_diff", added=["web"], removed=[], indent="  ",
     io={"get_effective_configurable_toolsets": X("RuntimeError", "label")})

# ── 19. _checklist_diff ──────────────────────────────────────────────────
case("_checklist_diff", new_enabled=["web", "mcp-srv"], prev=["file"], platform="telegram",
     io={"checklist_toolset_keys": R(["web", "file", "terminal"])})
case("_checklist_diff", new_enabled=[], prev=["web"], platform="telegram",
     io={"checklist_toolset_keys": R(["web"])})
case("_checklist_diff", new_enabled=["web"], prev=[], platform="telegram",
     io={"checklist_toolset_keys": R([])})
case("_checklist_diff", new_enabled=["a"], prev=["b"], platform="telegram",
     io={"checklist_toolset_keys": X("ValueError", "keys")})
case("_checklist_diff", new_enabled=["web", "file"], prev=["web"], platform="telegram",
     io={"checklist_toolset_keys": R(["web", "file"])})

# ── 20. _current_platform_tools ──────────────────────────────────────────
case("_current_platform_tools", config={"a": 1}, pkey="telegram", io={"get_platform_tools": R(["web", "file"])})
case("_current_platform_tools", config={}, pkey="discord", io={"get_platform_tools": R([])})
case("_current_platform_tools", config={}, pkey="telegram", io={"get_platform_tools": X("OSError", "resolve")})
case("_current_platform_tools", config={"b": 2}, pkey="whatsapp", io={"get_platform_tools": R(["terminal"])})
case("_current_platform_tools", config={}, pkey="telegram", io={"get_platform_tools": R(["mcp-x", "web"])})

# ── 21. _toolsets_needing_setup ──────────────────────────────────────────
case("_toolsets_needing_setup", new_enabled=["web"], config={},
     io={"is_configurable": R(True), "toolset_needs_configuration_prompt": R(True)})
case("_toolsets_needing_setup", new_enabled=["web"], config={},
     io={"is_configurable": R(False)})
case("_toolsets_needing_setup", new_enabled=["web"], config={},
     io={"is_configurable": R(True), "toolset_needs_configuration_prompt": R(False)})
case("_toolsets_needing_setup", new_enabled=[], config={}, io={})
case("_toolsets_needing_setup", new_enabled=["web"], config={},
     io={"is_configurable": X("RuntimeError", "cfg")})
case("_toolsets_needing_setup", new_enabled=["file", "web"], config={},
     io={"is_configurable": [R(False), R(True)], "toolset_needs_configuration_prompt": R(True)})

# ── 22. _configure_newly_added ───────────────────────────────────────────
CN = {"config": {}}
case("_configure_newly_added", added=["web"], already=[], io={
    "is_configurable": R(True), "toolset_needs_configuration_prompt": R(True), "configure_toolset": R(None)}, **CN)
case("_configure_newly_added", added=["web"], already=["web"], io={}, **CN)
case("_configure_newly_added", added=["web"], already=[], io={
    "is_configurable": R(True), "toolset_needs_configuration_prompt": R(True),
    "configure_toolset": X("RuntimeError", "configure")}, **CN)
case("_configure_newly_added", added=["file", "web"], already=[], io={
    "is_configurable": [R(True), R(False)], "toolset_needs_configuration_prompt": R(True),
    "configure_toolset": R(None)}, **CN)
case("_configure_newly_added", added=[], already=[], io={}, **CN)


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False, sort_keys=False) + "\n")
    print(f"wrote {len(CASES)} cases to {OUT}")


if __name__ == "__main__":
    main()
