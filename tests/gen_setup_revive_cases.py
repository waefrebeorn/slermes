#!/usr/bin/env python3
"""Generate tests/fixtures/setup_revive.jsonl for the setup cluster oracle.

Each case: {"func", "in": {config, args, io, env, nav}}. io scripts feed the
identical seam stubs on the C harness and the Python oracle side: a single
value repeats; an array is consumed per call (last element repeats); a
{"__raise__": kind, "msg": ...} object scripts an exception.
"""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
CASES = []


def C(func, config=None, args=None, io=None, env=None, nav=None):
    case = {"func": func, "in": {}}
    if config is not None:
        case["in"]["config"] = config
    if args:
        case["in"]["args"] = args
    if io:
        case["in"]["io"] = io
    if env:
        case["in"]["env"] = env
    if nav is not None:
        case["in"]["nav"] = nav
    CASES.append(case)


def R(kind, msg="", **extra):
    d = {"__raise__": kind, "msg": msg}
    d.update(extra)
    return d


GB = R("_SetupGoBack", "0", prompt_index=0)


def GB2():
    return R("_SetupGoBack", "2", prompt_index=2)


CANCEL = R("_SetupCancelled", "")

# ── handle_setup_menu_navigation ──
F = "handle_setup_menu_navigation"
C(F, args={"event": "begin"})
C(F, args={"event": "begin"}, nav={"section_index": -1})
C(F, args={"event": "begin"},
  nav={"section_index": 0, "prompt_index": 0, "active_prompt_index": -1,
       "resolved_choices": [], "replay_choices": []})
C(F, args={"event": "begin"},
  nav={"section_index": 1, "prompt_index": 1, "replay_choices": ["a", "b"]})
C(F, args={"event": "begin"},
  nav={"section_index": 0, "prompt_index": 3, "replay_choices": ["x"]})
C(F, args={"event": "resolve", "value": {"k": 1}},
  nav={"section_index": 0, "active_prompt_index": 1, "resolved_choices": ["x", "y", "z"]})
C(F, args={"event": "resolve", "value": 5},
  nav={"section_index": 0, "active_prompt_index": -1, "resolved_choices": ["q"]})
C(F, args={"event": "cancel"}, nav={"section_index": 0})
C(F, args={"event": "back"}, nav={"section_index": 0, "active_prompt_index": 2})

# ── run_setup_steps ──
F = "run_setup_steps"
STEPS2 = [["S1", "a1"], ["S2", "a2"]]
C(F, args={"steps": STEPS2}, io={"a1": None, "a2": None})
C(F, args={"steps": STEPS2}, io={"a1": None, "a2": None}, nav={"section_index": -1})
C(F, args={"steps": STEPS2}, io={"a1": None, "a2": [GB2(), None], "print_info": None},
  nav={"section_index": -1})
C(F, args={"steps": STEPS2}, io={"a1": [None, None], "a2": [GB, None], "print_info": None},
  nav={"section_index": -1})
C(F, args={"steps": [["S1", "a1"]]}, io={"a1": [GB, None], "print_info": None}, nav={"section_index": -1})
C(F, args={"steps": [["S1", "a1"]]}, io={"a1": CANCEL}, nav={"section_index": -1})
C(F, args={"steps": [["S1", "a1"]]}, io={"a1": R("ValueError", "bad")},
  nav={"section_index": -1})
C(F, args={"steps": []}, io={}, nav={"section_index": -1})

# ── run_setup_action_with_navigation ──
F = "run_setup_action_with_navigation"
NAVIO = {"set_menu_navigation_handler": 7, "reset_menu_navigation_handler": None,
         "_info": None, "print_info": None}
C(F, args={"label": "Model", "action": "act"}, io=dict(NAVIO, act=None))
C(F, args={"label": "Model", "action": "act"}, io=dict(NAVIO, act=CANCEL))
C(F, args={"label": "Model", "action": "act", "cancelled_message": "Bye."},
  io=dict(NAVIO, act=CANCEL))
C(F, args={"label": "Section", "action": "act"}, io=dict(NAVIO, act=[GB, None]))
C(F, args={"label": "Model", "action": "act"}, io=dict(NAVIO, act=None),
  nav={"section_index": 5, "prompt_index": 9, "resolved_choices": ["z"]})
C(F, args={"label": "Model", "action": "act"},
  io=dict(NAVIO, act=R("RuntimeError", "boom")))

# ── setup_model_provider ──
F = "setup_model_provider"
MP_BASE = {"print_header": None, "_info": None, "save_config": None,
           "logger.debug": None, "print_warning": None, "print_info": None}
C(F, config={"old": True},
  io=dict(MP_BASE, select_provider_and_model=None, load_config={"model": {"provider": "nous"}, "x": 1}))
C(F, config={}, io=dict(MP_BASE, select_provider_and_model=R("SystemExit", "1"),
                        load_config={}))
C(F, config={}, io=dict(MP_BASE, select_provider_and_model=R("KeyboardInterrupt", ""),
                        load_config={}))
C(F, config={}, io=dict(MP_BASE, select_provider_and_model=R("ValueError", "boom"),
                        provider_setup_failure_lines=[["Lead", "r1", "r2"]], load_config={}))
C(F, config={}, io=dict(MP_BASE, select_provider_and_model=R("ValueError", "boom"),
                        provider_setup_failure_lines=[["Only"]], load_config={}))
C(F, config={"keep": 1}, io=dict(MP_BASE, select_provider_and_model=None,
                                 load_config=R("RuntimeError", "disk")))
C(F, config={}, args={"quick": True},
  io=dict(MP_BASE, select_provider_and_model=None, load_config={"q": 1}))

# ── setup_tools ──
F = "setup_tools"
C(F, config={}, io={"tools_command": None})
C(F, config={"k": 1}, args={"first_install": True}, io={"tools_command": None})
C(F, config={}, io={"tools_command": R("ValueError", "x")})
C(F, config={}, io={"tools_command": "ignored"})
C(F, config={"a": {"b": 2}}, io={"tools_command": None})

# ── setup_telemetry ──
F = "setup_telemetry"
TEL_BASE = {"print_header": None, "_info": None, "save_consent": None,
            "print_success": None, "print_info": None}
C(F, config={}, io=dict(TEL_BASE, prompt_yes_no=True))
C(F, config={}, io=dict(TEL_BASE, prompt_yes_no=False))
C(F, config={"telemetry": {"shared_metrics": {"enabled": True, "send": True}}},
  io=dict(TEL_BASE, prompt_yes_no=False))
C(F, config={"telemetry": {"shared_metrics": {"enabled": True, "send": False}}},
  io=dict(TEL_BASE, prompt_yes_no=True))
C(F, config={"telemetry": {"shared_metrics": {"enabled": "yes", "send": True}}},
  io=dict(TEL_BASE, prompt_yes_no=True))
C(F, config={"telemetry": "x"}, io=dict(TEL_BASE, prompt_yes_no=CANCEL))

# ── skip_configured_section ──
F = "skip_configured_section"
SK_BASE = {"print": None, "print_success": None}
C(F, config={}, args={"section_key": "model", "label": "Model"},
  io=dict(SK_BASE, _get_section_config_summary=None))
C(F, config={}, args={"section_key": "model", "label": "Model"},
  io=dict(SK_BASE, _get_section_config_summary=""))
C(F, config={}, args={"section_key": "terminal", "label": "Terminal"},
  io=dict(SK_BASE, _get_section_config_summary="backend: local", prompt_yes_no=True))
C(F, config={}, args={"section_key": "terminal", "label": "Terminal"},
  io=dict(SK_BASE, _get_section_config_summary="backend: local", prompt_yes_no=False))
C(F, config={}, args={"section_key": "model", "label": "Model & Provider"},
  io=dict(SK_BASE, _get_section_config_summary="configured", prompt_yes_no=False))
C(F, config={}, args={"section_key": "gateway", "label": "Gateway"},
  io=dict(SK_BASE, _get_section_config_summary=R("RuntimeError", "sum")))

# ── run_xai_oauth_login_from_setup ──
F = "run_xai_oauth_login_from_setup"
XAI_BASE = {"_setup.print_warning": None, "_setup._info": None,
            "_save_xai_oauth_tokens": None, "unsuppress_credential_source": None}
C(F, io=dict(XAI_BASE, _is_remote_session=False,
             _xai_oauth_device_code_login={"tokens": {"a": 1}, "discovery": {"d": 2},
                                           "redirect_uri": "u", "last_refresh": 5}))
C(F, io=dict(XAI_BASE, _is_remote_session=True,
             _xai_oauth_device_code_login={"tokens": {}}))
C(F, io=dict(XAI_BASE, _is_remote_session=False,
             _xai_oauth_device_code_login=R("ValueError", "nope")))
C(F, io=dict(XAI_BASE, _is_remote_session=False,
             _xai_oauth_device_code_login=R("SystemExit", "130")))
C(F, io=dict(XAI_BASE, _is_remote_session=False,
             _xai_oauth_device_code_login={"tokens": {}},
             _save_xai_oauth_tokens=R("RuntimeError", "disk")))
C(F, env={"auth_import_error": "missing helpers"}, io={"_setup.print_warning": None})
C(F, io=dict(XAI_BASE, _is_remote_session=False, _xai_oauth_device_code_login={}))
C(F, io=dict(XAI_BASE, _is_remote_session=False,
             _xai_oauth_device_code_login={"tokens": {}, "discovery": None,
                                           "redirect_uri": None}))

# ── setup_telegram_auto_result ──
F = "setup_telegram_auto_result"
C(F, env={"telegram_import_error": True})
C(F, io={"get_hermes_home": "/h/profiles/p1",
         "auto_setup_telegram_bot_result": {"ok": True}})
C(F, io={"get_hermes_home": "/h", "auto_setup_telegram_bot_result": None})
C(F, io={"get_hermes_home": R("RuntimeError", "x"), "auto_setup_telegram_bot_result": {}})
C(F, io={"get_hermes_home": R("SystemExit", "1")})
C(F, io={"get_hermes_home": None, "auto_setup_telegram_bot_result": {"x": 1}})
C(F, io={"get_hermes_home": "/h", "auto_setup_telegram_bot_result": R("ValueError", "qr")})
C(F, io={"get_hermes_home": "/h/profiles/p1/", "auto_setup_telegram_bot_result": {"ok": 1}})

# ── print_macos_fda_tip ──
F = "print_macos_fda_tip"
C(F, env={"platform": "win32"})
C(F, env={"platform": "darwin", "home": "/Users/dev"}, io={"os.listdir": [[]]})
C(F, env={"platform": "darwin", "home": "/Users/dev"},
  io={"os.listdir": R("PermissionError", "denied"), "_info": None})
C(F, env={"platform": "darwin", "home": "/Users/dev"},
  io={"os.listdir": R("FileNotFoundError", "gone")})
C(F, env={"platform": "darwin", "home": "/Users/dev"},
  io={"os.listdir": R("OSError", "indeterminate")})
C(F, env={"platform": "darwin", "home": "/Users/dev"},
  io={"os.listdir": R("ValueError", "weird")})

# ── blank_slate_minimal_toolsets ──
F = "blank_slate_minimal_toolsets"
C(F, config={}, env={"toolsets": {}, "configurable_toolsets": []},
  io={"_get_plugin_toolset_keys": [[]]})
C(F, config={},
  env={"configurable_toolsets": [["web", "Web", "d"]],
       "toolsets": {"search": ["web_search"], "kanban": ["kanban_tool"]}},
  io={"_get_plugin_toolset_keys": [["plug1"]],
      "resolve_toolset": [["web_search", "web_fetch"], ["web_search"], ["kanban_tool"]]})
C(F, config={},
  env={"configurable_toolsets": [],
       "toolsets": {"hermes-cli": ["x"], "grp": {"includes": ["a"]},
                    "post": {"posture": "strict"}, "plain": ["t"]}},
  io={"_get_plugin_toolset_keys": [[]], "resolve_toolset": [["t"]]})
C(F, config={}, env={"configurable_toolsets": [], "toolsets": {"unk": []}},
  io={"_get_plugin_toolset_keys": [[]], "resolve_toolset": [[]]})
C(F, config={}, env={"toolsets_import_error": "no module"}, io={"logger.debug": None})
C(F, config={}, env={"toolsets": {}, "configurable_toolsets": []},
  io={"_get_plugin_toolset_keys": R("RuntimeError", "p"), "logger.debug": None})
C(F, config={"platform_toolsets": "x"}, env={"toolsets": {}, "configurable_toolsets": []})
C(F, config={}, env={"configurable_toolsets": [],
                     "toolsets": {"file": ["read_file"], "terminal": ["bash"]}},
  io={"_get_plugin_toolset_keys": [[]], "resolve_toolset": [["read_file"], ["bash"]]})

# ── blank_slate_minimize_config ──
F = "blank_slate_minimize_config"
C(F, config={})
C(F, config={"agent": {"max_turns": 200, "custom": 1}})
C(F, config={"memory": {"memory_enabled": True, "user_profile_enabled": True, "x": 2}})
C(F, config={"compression": "x"})
C(F, config={"agent": {"max_turns": 5}, "compression": {"enabled": True},
             "memory": {"memory_enabled": True}, "checkpoints": {"enabled": True},
             "smart_model_routing": {"enabled": True}})

# ── run_portal_one_shot ──
F = "run_portal_one_shot"
PO_BASE = {"_print_banner": None, "_info": None, "logger.debug": None, "print": None,
           "print_error": None, "print_info": None, "print_success": None,
           "_record_setup_completed": None}
C(F, config={"old": 1}, io=dict(PO_BASE, _model_flow_nous=None, load_config={"fresh": 1}))
C(F, config={}, io=dict(PO_BASE, _model_flow_nous=R("KeyboardInterrupt", "")))
C(F, config={}, io=dict(PO_BASE, _model_flow_nous=R("SystemExit", "1")))
C(F, config={}, io=dict(PO_BASE, _model_flow_nous=R("EOFError", "")))
C(F, config={}, io=dict(PO_BASE, _model_flow_nous=R("ValueError", "bad"),
                        provider_setup_failure_lines=[["Lead", "rest"]]))
C(F, config={"keep": 1}, io=dict(PO_BASE, _model_flow_nous=None,
                                 load_config=R("RuntimeError", "disk")))
C(F, config={"keep": 1}, io=dict(PO_BASE, _model_flow_nous=None, load_config=None))

# ── run_first_time_quick_setup ──
F = "run_first_time_quick_setup"
FT_BASE = {"setup_terminal_backend": None, "remove_env_value": None, "save_config": None,
           "print_success": None, "_info": None, "print": None,
           "_print_setup_summary": None, "print_info": None, "setup_gateway": None,
           "ensure_gateway_service": None, "print_header": None,
           "logger.debug": None, "print_warning": None}
C(F, config={}, io=dict(FT_BASE, _keep_imported_model=True, prompt_choice=0))
C(F, config={"old": 1},
  io=dict(FT_BASE, _keep_imported_model=False, _model_flow_nous=None,
          load_config={"p": 1}, prompt_choice=1))
C(F, config={}, args={"imported_from": "openclaw"},
  io=dict(FT_BASE, _keep_imported_model=True, prompt_choice=0))
C(F, config={},
  io=dict(FT_BASE, _keep_imported_model=False, _model_flow_nous=R("ValueError", "x"),
          provider_setup_failure_lines=[["L", "r"]], load_config={}, prompt_choice=0))
C(F, config={}, io=dict(FT_BASE, _keep_imported_model=True, prompt_choice=0,
                        setup_gateway=R("RuntimeError", "gw")))
C(F, config={}, args={"hermes_home": "/Users/d"},
  env={"platform": "darwin", "home": "/Users/d"},
  io=dict(FT_BASE, _keep_imported_model=True, prompt_choice=1,
          **{"os.listdir": R("PermissionError", "denied")}))

# ── run_blank_slate_setup ──
F = "run_blank_slate_setup"
BS_BASE = {"print_header": None, "_info": None, "save_config": None,
           "setup_terminal_backend": None, "print": None, "print_success": None,
           "print_info": None, "print_warning": None, "logger.debug": None,
           "select_provider_and_model": None, "load_config": {"m": 1},
           "_get_plugin_toolset_keys": [[]], "set_bundled_skills_opt_out": None,
           "sync_skills": {"copied": []}, "_blank_slate_done": None,
           "provider_setup_failure_lines": [["L"]]}
BS_ENV = {"toolsets": {}, "configurable_toolsets": []}
C(F, config={}, env=BS_ENV, io=dict(BS_BASE, prompt_choice=0))
C(F, config={}, env=BS_ENV,
  io=dict(BS_BASE, prompt_choice=1,
          prompt_yes_no=[False, False, False, False, False],
          tools_command=None, setup_gateway=None))
C(F, config={}, env=BS_ENV,
  io=dict(BS_BASE, prompt_choice=0, select_provider_and_model=R("ValueError", "e")))
C(F, config={}, env=BS_ENV,
  io=dict(BS_BASE, prompt_choice=0, sync_skills=R("RuntimeError", "s")))
C(F, config={}, env=dict(BS_ENV, toolsets_import_error="no"),
  io=dict(BS_BASE, prompt_choice=0))
C(F, config={}, env=BS_ENV, io=dict(BS_BASE, prompt_choice=CANCEL))

# ── blank_slate_walkthrough ──
F = "blank_slate_walkthrough"
WT_BASE = {"print_header": None, "print_info": None, "print_success": None,
           "_info": None, "print_warning": None, "logger.debug": None,
           "set_bundled_skills_opt_out": None, "print": None,
           "setup_gateway": None, "save_config": None, "_blank_slate_done": None}
C(F, config={}, io=dict(WT_BASE, prompt_yes_no=[False] * 5, sync_skills=None))
C(F, config={"old": 1},
  io=dict(WT_BASE, prompt_yes_no=[True] * 5, sync_skills={"copied": [1, 2, 3]},
          tools_command=None, load_config={"t": 1}))
C(F, config={}, io=dict(WT_BASE, prompt_yes_no=[True, False, False, False, False],
                        sync_skills=R("ValueError", "s")))
C(F, config={}, io=dict(WT_BASE, prompt_yes_no=[False, True, False, False, False],
                        sync_skills=None, tools_command=R("RuntimeError", "t")))
C(F, config={}, io=dict(WT_BASE, prompt_yes_no=[False, True, False, False, False],
                        sync_skills=None, tools_command=None,
                        load_config=R("RuntimeError", "r")))
C(F, config={}, io=dict(WT_BASE, prompt_yes_no=[True, False, False, False, False],
                        sync_skills=None))

# ── setup_tts_provider ──
F = "setup_tts_provider"
TTS_BASE = {"print": None, "_setup.print_header": None, "_setup._info": None,
            "_setup.prompt_choice": None, "_setup.print_info": None,
            "_setup.get_env_value": None, "_setup.print_warning": None,
            "_setup.save_config": None, "_setup.print_success": None,
            "_setup.prompt": None}
C(F, config={}, io={**TTS_BASE, "managed_nous_tools_enabled": False, "_setup.prompt_choice": 9})
C(F, config={}, io={**TTS_BASE, "managed_nous_tools_enabled": False, "_setup.prompt_choice": [0, 1]})
C(F, config={"tts": {"provider": "edge", "edge": {"voice": "xx-YY"}}},
  io={**TTS_BASE, "managed_nous_tools_enabled": False, "_setup.prompt_choice": [0, 0]})
C(F, config={}, io={**TTS_BASE, "managed_nous_tools_enabled": False,
                    "_setup.prompt_choice": [0, 4], "_setup.prompt": "  custom  "})
C(F, config={}, io={**TTS_BASE, "managed_nous_tools_enabled": False,
                    "_setup.prompt_choice": [0, 4], "_setup.prompt": ""})
C(F, config={}, io={**TTS_BASE, "managed_nous_tools_enabled": True,
                    "get_nous_subscription_features": {"nous_auth_present": True},
                    "_setup.prompt_choice": 0, "_setup.get_env_value": "k"})
C(F, config={}, io={**TTS_BASE, "managed_nous_tools_enabled": True,
                    "get_nous_subscription_features": {"nous_auth_present": True},
                    "_setup.prompt_choice": 0, "_setup.get_env_value": None})
C(F, config={}, io={**TTS_BASE, "managed_nous_tools_enabled": False,
                    "_setup.prompt_choice": 1, "_tts_api_key_step": "elevenlabs"})
C(F, config={}, io={**TTS_BASE, "managed_nous_tools_enabled": False,
                    "_setup.prompt_choice": [7, 0], "_tts_local_install_step": "edge"})
C(F, config={}, io={**TTS_BASE, "managed_nous_tools_enabled": False,
                    "_setup.prompt_choice": 3, "_tts_xai_step": "xai"})
C(F, config={}, io={**TTS_BASE, "managed_nous_tools_enabled": True,
                    "get_nous_subscription_features": {"nous_auth_present": False},
                    "_setup.prompt_choice": [0, 0]})


def main():
    out = HERE / "fixtures" / "setup_revive.jsonl"
    out.parent.mkdir(parents=True, exist_ok=True)
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = [k for k, v in counts.items() if v < 5]
    print("wrote %d cases for %d functions -> %s" % (len(CASES), len(counts), out))
    if short:
        raise SystemExit("functions with <5 cases: %s" % short)


if __name__ == "__main__":
    main()
