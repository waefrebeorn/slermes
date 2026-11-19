#!/usr/bin/env python3
"""Generator for tests/fixtures/browser_tools_flow.jsonl.

Emits >=5 cases per ported function of tools/browser_use_cli.py (the UNCLAIMED
browser-tools surface ported as src/tools/browser_tools_flow.c). Each case is
{"func": <name>, "in": {...args..., "env": {env dict},
"state": {real_profile_fallback,real_profile_note_delivered}, "io": {seam: script}}}.

Script shapes (shared with the C universal seam bt_call and the oracle):
  absent key            -> None
  {"$seq": [...]}       -> per-call queue (exhaustion raises RuntimeError)
  {"__raise__": cls, "msg": m} -> raises a REAL exception of that class
  any other JSON value  -> reused for every call

Domain (documented in the port header): str() operands ASCII; _clamp_timeout
inputs finite ints/floats/bools/int-strings or TypeError/ValueError forms (NaN/inf
excluded); _published_daemon_pid pid-file texts strict JSON or clearly-invalid;
_code_url_verdict code ASCII; browser_cfg / session_obj are dicts (the live .get
raises on None); env-taking functions get a dict env. BaseException-trio scripted
raises exercise the passthrough past `except Exception`.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "browser_tools_flow.jsonl")
CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def io(**scripts):
    return scripts


def seq(*vals):
    return {"$seq": list(vals)}


def raise_(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


RP = "_HERMES_BU_REAL_PROFILE_FALLBACK"
LF = "_HERMES_BU_LOCAL_FALLBACK"
BD = "_HERMES_BU_BOT_DESKTOP_BROWSER"
PV = "_HERMES_BU_PRIVATE_BROWSER"


def state(rpf=None, rpnd=None):
    return {"real_profile_fallback": rpf or {}, "real_profile_note_delivered": rpnd or []}


# consent seams fired by _real_profile_consented (via _lazy_call) when local=true
CONSENT_IO = io(import_module=True, use_real_profile=True)
NO_CONSENT_IO = io(import_module=True, use_real_profile=False)


# ── _set_cdp_env ────────────────────────────────────────────────────────────
case("_set_cdp_env", env={}, cdp="http://127.0.0.1:9222")
case("_set_cdp_env", env={}, cdp="https://host/devtools")
case("_set_cdp_env", env={}, cdp="ws://127.0.0.1:9222/devtools/browser/x")
case("_set_cdp_env", env={"BU_CDP_WS": "old"}, cdp="http://h")
case("_set_cdp_env", env={}, cdp="")
case("_set_cdp_env", env={}, cdp="HTTP://UPPER")  # startswith is case-sensitive -> WS

# ── _has_cdp_env ────────────────────────────────────────────────────────────
case("_has_cdp_env", env={})
case("_has_cdp_env", env={"BU_CDP_WS": "ws://x"})
case("_has_cdp_env", env={"BU_CDP_URL": "http://x"})
case("_has_cdp_env", env={"BU_CDP_WS": "", "BU_CDP_URL": ""})
case("_has_cdp_env", env={"BU_CDP_WS": None, "BU_CDP_URL": "http://x"})
case("_has_cdp_env", env={"BU_CDP_WS": "ws://x", "BU_CDP_URL": "http://y"})

# ── _clamp_timeout ──────────────────────────────────────────────────────────
case("_clamp_timeout", timeout_s=300)
case("_clamp_timeout", timeout_s=1)
case("_clamp_timeout", timeout_s=9999)
case("_clamp_timeout", timeout_s=5.9)
case("_clamp_timeout", timeout_s=-3)
case("_clamp_timeout", timeout_s="12")
case("_clamp_timeout", timeout_s="abc")
case("_clamp_timeout", timeout_s=None)
case("_clamp_timeout", timeout_s=[])
case("_clamp_timeout", timeout_s=True)
case("_clamp_timeout", timeout_s="  45  ")

# ── _no_code_error ──────────────────────────────────────────────────────────
case("_no_code_error", code="")
case("_no_code_error", code="   ")
case("_no_code_error", code="print(1)")
case("_no_code_error", code=None)
case("_no_code_error", code="\t\n")
case("_no_code_error", code="  x  ")

# ── _sidecar_daemon_name ────────────────────────────────────────────────────
case("_sidecar_daemon_name", name="abc")
case("_sidecar_daemon_name", name="")
case("_sidecar_daemon_name", name=None)
case("_sidecar_daemon_name", name="x" * 70)
case("_sidecar_daemon_name", name="r7k2")
case("_sidecar_daemon_name", name="a" * 58)  # _local_ + 58 = 65 -> truncate to 64

# ── _served_profile_tag ─────────────────────────────────────────────────────
case("_served_profile_tag", io=io(get_hermes_home_override=None))
case("_served_profile_tag", io=io(get_hermes_home_override="/h", hermes_home_key="k1"))
case("_served_profile_tag", io=io(get_hermes_home_override="", hermes_home_key="k2"))
case("_served_profile_tag", io=io(get_hermes_home_override="/h", hermes_home_key=""))
case("_served_profile_tag", io=io(get_hermes_home_override=raise_("RuntimeError", "x")))
case("_served_profile_tag")  # fail-closed: absent override script -> None -> ""

# ── _browser_use_api_key_set ────────────────────────────────────────────────
case("_browser_use_api_key_set", io=io(get_secret="sk-1"))
case("_browser_use_api_key_set", io=io(get_secret=""))
case("_browser_use_api_key_set", io=io(get_secret=None))
case("_browser_use_api_key_set", io=io(get_secret=raise_("ValueError", "x")))
case("_browser_use_api_key_set", io=io(get_secret="0"))  # truthy non-empty str

# ── _use_gateway ────────────────────────────────────────────────────────────
case("_use_gateway", browser_cfg={"use_gateway": True}, io=io(is_truthy_value=True))
case("_use_gateway", browser_cfg={"cloud_provider": "nous"},
     io=io(is_truthy_value=False, nous_managed_provider="nous"))
case("_use_gateway", browser_cfg={"cloud_provider": "other"},
     io=io(is_truthy_value=False, nous_managed_provider="nous"))
case("_use_gateway", browser_cfg={"cloud_provider": "  NOUS  "},
     io=io(is_truthy_value=False, nous_managed_provider="nous"))
case("_use_gateway", browser_cfg={}, io=io(is_truthy_value=False, nous_managed_provider=raise_("ImportError", "x")))
case("_use_gateway", browser_cfg={"cloud_provider": None},
     io=io(is_truthy_value=False, nous_managed_provider="nous"))

# ── _harness_credentials ────────────────────────────────────────────────────
case("_harness_credentials", io=io(serves_routed_profile=False, get_secret="sk"))
case("_harness_credentials", io=io(serves_routed_profile=False, get_secret=None))
case("_harness_credentials", io=io(serves_routed_profile=True, current_secret_scope={"BROWSER_USE_API_KEY": "s"}))
case("_harness_credentials", io=io(serves_routed_profile=True, current_secret_scope={}))
case("_harness_credentials", io=io(serves_routed_profile=True, current_secret_scope=None))
case("_harness_credentials", io=io(serves_routed_profile=True, current_secret_scope={"BROWSER_USE_API_KEY": None}))
case("_harness_credentials", io=io(serves_routed_profile=raise_("RuntimeError", "x")))

# ── _export_real_profile_note ───────────────────────────────────────────────
case("_export_real_profile_note", env={}, key="k", io=io(real_profile_note="note"))
case("_export_real_profile_note", env={}, key="k", io=io(real_profile_note=None))
case("_export_real_profile_note", env={}, key="k", io=io(real_profile_note=""))
case("_export_real_profile_note", env={RP: "existing"}, key="k", io=io(real_profile_note="new"))
case("_export_real_profile_note", env={}, key="k", io=io(real_profile_note=raise_("OSError", "x")))

# ── _mark_local_browser ─────────────────────────────────────────────────────
case("_mark_local_browser", env={}, session_obj={}, key="k", io=io(real_profile_note=None))
case("_mark_local_browser", env={}, session_obj={"features": {"lightpanda": True}}, key="k",
     io=io(real_profile_note=None))
case("_mark_local_browser", env={}, session_obj={"shared_profile": True}, key="k", io=io(real_profile_note=None))
case("_mark_local_browser", env={}, session_obj={"features": {"real_profile": True}}, key="k",
     io=io(real_profile_note=None))
case("_mark_local_browser", env={}, session_obj={"features": None}, key="k", io=io(real_profile_note="n"))
case("_mark_local_browser", env={}, session_obj={}, key="k", io=io(real_profile_note=raise_("OSError", "x")))

# ── _quiet ──────────────────────────────────────────────────────────────────
case("_quiet", fn_seam="fn", default="d", log_prefix="", io=io(fn="v"))
case("_quiet", fn_seam="fn", default="d", log_prefix="p", io=io(fn=raise_("RuntimeError", "boom")))
case("_quiet", fn_seam="fn", default="d", log_prefix="", io=io(fn=raise_("ValueError", "boom")))
case("_quiet", fn_seam="fn", default="d", log_prefix="p", io=io(fn=raise_("KeyboardInterrupt", "")))
case("_quiet", fn_seam="fn", default=None, log_prefix="", io=io(fn=None))
case("_quiet", fn_seam="fn", default={"x": 1}, log_prefix="p", io=io(fn=raise_("RuntimeError", "b")))

# ── _lazy_call ──────────────────────────────────────────────────────────────
case("_lazy_call", module="m", lname="f", default="d", log_prefix="p", io=io(import_module=True, f="v"))
case("_lazy_call", module="m", lname="f", default="d", log_prefix="p", io=io(import_module=raise_("ImportError", "no")))
case("_lazy_call", module="m", lname="f", default="d", log_prefix="p", io=io(import_module=True, f=raise_("RuntimeError", "b")))
case("_lazy_call", module="m", lname="f", default="d", log_prefix="", io=io(import_module=True, f=None))
case("_lazy_call", module="m", lname="f", default="d", log_prefix="p", io=io(import_module=True, f=raise_("SystemExit", "")))
case("_lazy_call", module="m", lname="f", default=False, log_prefix="p", io=io(import_module=True, f=True))

# ── _real_profile_consented ─────────────────────────────────────────────────
case("_real_profile_consented", io=io(import_module=True, use_real_profile=True))
case("_real_profile_consented", io=io(import_module=True, use_real_profile=False))
case("_real_profile_consented", io=io(import_module=raise_("ImportError", "x")))
case("_real_profile_consented", io=io(import_module=True, use_real_profile=raise_("RuntimeError", "x")))
case("_real_profile_consented", io=io(import_module=True, use_real_profile=None))

# ── _code_url_verdict ───────────────────────────────────────────────────────
case("_code_url_verdict", code="print(1)")
case("_code_url_verdict", code="new_tab('https://a.com')",
     io=io(hybrid_routes_locally=False, evaluate_url_safety=None))
case("_code_url_verdict", code="new_tab('https://a.com')",
     io=io(hybrid_routes_locally=True, evaluate_url_safety=None))
case("_code_url_verdict", code="new_tab('https://bad.com')",
     io=io(hybrid_routes_locally=False, evaluate_url_safety={"error": "Blocked: private"}))
case("_code_url_verdict", code="https://a.com https://b.com",
     io=io(hybrid_routes_locally=seq(False, True), evaluate_url_safety=seq(None, None)))
case("_code_url_verdict", code="https://a.com",
     io=io(hybrid_routes_locally=False, evaluate_url_safety={}))
case("_code_url_verdict", code="https://a.com",
     io=io(hybrid_routes_locally=False, evaluate_url_safety={"error": None}))
case("_code_url_verdict", code="https://a.com",
     io=io(hybrid_routes_locally=raise_("RuntimeError", "dns")))

# ── _attach_vault_supervisor ────────────────────────────────────────────────
case("_attach_vault_supervisor", env={}, task_id="t")
case("_attach_vault_supervisor", env={"BU_CDP_WS": "ws://x"}, task_id="t",
     io=io(get_dialog_policy_config=["allow", 5], resolve_cdp_override="ws://r",
           **{"SUPERVISOR_REGISTRY.get_or_start": None}))
case("_attach_vault_supervisor", env={"BU_CDP_URL": "http://x"}, task_id=None,
     io=io(get_dialog_policy_config=[None, 10], resolve_cdp_override="http://r",
           **{"SUPERVISOR_REGISTRY.get_or_start": None}))
case("_attach_vault_supervisor", env={"BU_CDP_WS": "ws://x"}, task_id="t",
     io=io(get_dialog_policy_config=raise_("RuntimeError", "x")))
case("_attach_vault_supervisor", env={"BU_CDP_WS": "ws://x"}, task_id="t",
     io=io(get_dialog_policy_config=["allow", 5], resolve_cdp_override=raise_("OSError", "x")))
case("_attach_vault_supervisor", env={"BU_CDP_WS": "ws://x"}, task_id="t",
     io=io(get_dialog_policy_config=["allow", 5], resolve_cdp_override="ws://r",
           **{"SUPERVISOR_REGISTRY.get_or_start": raise_("KeyboardInterrupt", "")}))

# ── _record_driven_tab ──────────────────────────────────────────────────────
case("_record_driven_tab", env={}, task_id="t")
case("_record_driven_tab", env={"BU_CDP_WS": "ws://x"}, task_id="t",
     io=io(**{"SUPERVISOR_REGISTRY.get": None}))
case("_record_driven_tab", env={"BU_CDP_WS": "ws://x"}, task_id="t",
     io=io(**{"SUPERVISOR_REGISTRY.get": {"id": 1}}, harness_current_target="tab1",
           **{"supervisor.set_driven_target": None}))
case("_record_driven_tab", env={"BU_CDP_URL": "http://x"}, task_id=None,
     io=io(**{"SUPERVISOR_REGISTRY.get": {"id": 1}}, harness_current_target="tab2",
           **{"supervisor.set_driven_target": None}))
case("_record_driven_tab", env={"BU_CDP_WS": "ws://x"}, task_id="t",
     io=io(**{"SUPERVISOR_REGISTRY.get": raise_("RuntimeError", "x")}))
case("_record_driven_tab", env={"BU_CDP_WS": "ws://x"}, task_id="t",
     io=io(**{"SUPERVISOR_REGISTRY.get": {"id": 1}}, harness_current_target=raise_("OSError", "x")))

# ── _published_daemon_pid ───────────────────────────────────────────────────
case("_published_daemon_pid", env={"BU_NAME": "n"},
     io=io(harness_pid_file="/run/pid.json", read_text='{"pid": 123, "started": 1}'))
case("_published_daemon_pid", env={"BU_NAME": "n"}, io=io(harness_pid_file="/run/pid", read_text="456"))
case("_published_daemon_pid", env={"BU_NAME": "n"}, io=io(harness_pid_file="/run/pid", read_text="  789  "))
case("_published_daemon_pid", env={"BU_NAME": "n"},
     io=io(harness_pid_file="/run/pid.json", read_text='{"pid": "abc"}'))
case("_published_daemon_pid", env={"BU_NAME": "n"},
     io=io(harness_pid_file="/run/pid.json", read_text='{"started": 1}'))
case("_published_daemon_pid", env={"BU_NAME": "n"}, io=io(harness_pid_file="/run/pid", read_text=""))
case("_published_daemon_pid", env={"BU_NAME": "n"},
     io=io(harness_pid_file="/run/pid", read_text=raise_("OSError", "x")))
case("_published_daemon_pid", env={"BU_NAME": "n"},
     io=io(harness_pid_file=raise_("RuntimeError", "x")))
case("_published_daemon_pid", env={"BU_NAME": "n"}, io=io(harness_pid_file="/run/pid", read_text="notjson"))

# ── _take_real_profile_note ─────────────────────────────────────────────────
case("_take_real_profile_note", route_key="k", state=state())
case("_take_real_profile_note", route_key="k", state=state(rpf={"k": "note"}))
case("_take_real_profile_note", route_key="k", state=state(rpf={"k": "note"}, rpnd=["k"]))
case("_take_real_profile_note", route_key="k", state=state(rpf={"k": ""}))
case("_take_real_profile_note", route_key="other", state=state(rpf={"k": "note"}))
case("_take_real_profile_note", route_key="k", state=state(rpf={"k": "n1", "j": "n2"}, rpnd=["j"]))

# ── _route_backend ──────────────────────────────────────────────────────────
case("_route_backend", env={}, session="", task_id="t", local=False,
     io=io(_resolve_real_profile_cdp=None, _resolve_backend_cdp=None))
case("_route_backend", env={}, session="s", task_id="t", local=False,
     io=io(_resolve_real_profile_cdp=None, _resolve_backend_cdp="err"))
case("_route_backend", env={}, session="", task_id="t", local=True,
     io=io(_resolve_real_profile_cdp=None, **NO_CONSENT_IO))
case("_route_backend", env={}, session="", task_id="t", local=True,
     io=io(_resolve_real_profile_cdp="rp_err", real_profile_fallback_note="note",
           _resolve_backend_cdp=None, **CONSENT_IO))
case("_route_backend", env={}, session="", task_id="t", local=True, real_profile=False,
     io=io(_resolve_backend_cdp=None))
case("_route_backend", env={"BU_CDP_WS": "ws://x"}, session="", task_id="t", local=True,
     io=io(_resolve_real_profile_cdp=None, _resolve_backend_cdp=None, **CONSENT_IO))
case("_route_backend", env={}, session="", task_id="t", local=False,
     io=io(_resolve_real_profile_cdp=raise_("RuntimeError", "x")))

# ── _route_exec ─────────────────────────────────────────────────────────────
case("_route_exec", env={}, session="", task_id="t", local=False, local_sidecar=False, route_key="k",
     state=state(), io=io(_resolve_real_profile_cdp=None, _resolve_backend_cdp=None))
case("_route_exec", env={RP: "note"}, session="", task_id="t", local=False, local_sidecar=False,
     route_key="k", state=state(), io=io(_resolve_real_profile_cdp=None, _resolve_backend_cdp=None))
case("_route_exec", env={LF: "1"}, session="", task_id="t", local=False, local_sidecar=False,
     route_key="k", state=state(), io=io(_resolve_real_profile_cdp=None, _resolve_backend_cdp=None))
case("_route_exec", env={}, session="", task_id="t", local=False, local_sidecar=True, route_key="k",
     state=state(rpf={"k": "old"}), io=io(_resolve_backend_cdp=None))
case("_route_exec", env={}, session="", task_id="t", local=False, local_sidecar=False, route_key="k",
     state=state(), io=io(_resolve_real_profile_cdp=None, _resolve_backend_cdp="err"))
case("_route_exec", env={RP: "n", LF: "1"}, session="s", task_id="t", local=False, local_sidecar=False,
     route_key="k", state=state(rpf={"k": "pre"}),
     io=io(_resolve_real_profile_cdp=None, _resolve_backend_cdp=None))

# ── browser_exec_available ──────────────────────────────────────────────────
case("browser_exec_available", io=io(_find_cli=None))
case("browser_exec_available", io=io(_find_cli=["py", "-m", "browser_harness.run"]))
case("browser_exec_available", io=io(_find_cli=raise_("RuntimeError", "x")))
case("browser_exec_available", io=io(_find_cli=[]))
case("browser_exec_available")  # absent script -> None -> False


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as fh:
        for c in CASES:
            fh.write(json.dumps(c, ensure_ascii=False) + "\n")
    # per-function count check (>=5 each)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    print(f"gen {NAME}: {len(CASES)} cases, {len(counts)} funcs")
    if short:
        print(f"GEN WARNING: funcs with <5 cases: {short}")
    return 1 if short else 0


NAME = "browser_tools_flow"
if __name__ == "__main__":
    raise SystemExit(main())