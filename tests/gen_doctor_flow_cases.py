#!/usr/bin/env python3
"""Generate tests/fixtures/doctor_flow.jsonl for the doctor-diagnostic UNCLAIMED
FLOW oracle. Each line is {"func": <name>, "in": {...}}; "in.io" carries the
scripted seam results ({"ret","raise","import_raise","state"} or a queue LIST)
consumed identically by the C harness dispatcher and the oracle's recording
stubs. AUTO cosmetic seams (print/color/check_ok/check_warn/check_fail/check_info/
_section) need no script. List-valued seam results are wrapped in {"ret": [...]}.
Pure data — no quarry import. >=5 cases per function.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "doctor_flow.jsonl")
CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def R(v):
    return {"ret": v}


def X(type, message="", **extra):
    d = {"type": type, "message": message}
    d.update(extra)
    return {"raise": d}


def IMP(type, message="", **extra):
    d = {"type": type, "message": message}
    d.update(extra)
    return {"import_raise": d}


def F(issues=None, manual=None, fixed=0):
    return {"issues": issues or [], "manual_issues": manual or [], "fixed": fixed}


# ── _mark ────────────────────────────────────────────────────────────────
case("mark", glyph="\u2713", col="\x1b[32m", text="ok", detail="")
case("mark", glyph="\u26a0", col="\x1b[33m", text="warn", detail="d")
case("mark", glyph="\u2717", col="\x1b[31m", text="fail", detail="detail here")
case("mark", glyph="\u2192", col="\x1b[36m", text="", detail="")
case("mark", glyph="\u2713", col="\x1b[32m", text="a b", detail="x y")
case("mark", glyph="\u25c6", col="\x1b[36m", text="t", detail="")

# ── check_bool ───────────────────────────────────────────────────────────
case("check_bool", cond=True, ok="good", bad="bad")
case("check_bool", cond=False, ok="good", bad="bad")
case("check_bool", cond=False, ok=["g", "gd"], bad=["b", "bd"], fail=True)
case("check_bool", cond=1, ok="g", bad="b")
case("check_bool", cond=0, ok="g", bad="b")
case("check_bool", cond=None, ok=["g"], bad=["b", "bd"])
case("check_bool", cond="", ok="g", bad="b")
case("check_bool", cond=False, ok="g", bad="b", fail=False)

# ── Finding.merge ────────────────────────────────────────────────────────
case("finding_merge", state={"finding": F(["a"], [], 1)}, other=F(["b"], ["m"], 2))
case("finding_merge", state={"finding": F()}, other=F())
case("finding_merge", state={"finding": F([], ["x"], 0)}, other=F(["y"], [], 3))
case("finding_merge", state={"finding": F(["a", "b"])}, other=F([], ["m1", "m2"], 0))
case("finding_merge", state={"finding": F()}, other=F(["z"], [], 5))

# ── ensure_dir ───────────────────────────────────────────────────────────
ED = dict(path="/h/cache", exists_msg="exists", created_msg="created", missing_msg="missing")
case("ensure_dir", should_fix=False, state={"finding": F()}, io={"path_exists": R(True)}, **ED)
case("ensure_dir", should_fix=True, state={"finding": F()}, io={"path_exists": R(False), "path_mkdir": R(None)}, **ED)
case("ensure_dir", should_fix=False, state={"finding": F()}, io={"path_exists": R(False)}, **ED)
case("ensure_dir", should_fix=False, state={"finding": F()}, io={"path_exists": X("OSError", "denied")}, **ED)
case("ensure_dir", should_fix=True, state={"finding": F()}, io={"path_exists": R(False), "path_mkdir": X("OSError", "x")}, **ED)
case("ensure_dir", should_fix=False, state={"finding": F()}, io={}, **ED)
case("ensure_dir", should_fix=False, state={"finding": F()}, io={"path_exists": X("KeyboardInterrupt", "")}, **ED)

# ── warn_on_error ────────────────────────────────────────────────────────
case("warn_on_error", text="T", detail="({e})", body="body", io={"body": R(None)})
case("warn_on_error", text="T", detail="({e})", body="body", io={"body": X("ValueError", "boom")})
case("warn_on_error", text="", detail="({e})", body="body", io={"body": X("ValueError", "boom")})
case("warn_on_error", text="T", detail="({e})", body="body", io={"body": X("SystemExit", "1", code=1)})
case("warn_on_error", text="T", detail="({e})", body="body", io={"body": X("KeyboardInterrupt", "")})
case("warn_on_error", text="Error: {e}", detail="d:{e}", body="body", io={"body": R(None)})
case("warn_on_error", text="T", detail="({e})", body="body", io={})
case("warn_on_error", text="X {e}", detail="Y {e}", body="body", io={"body": X("RuntimeError", "boom")})

# ── _login_row ───────────────────────────────────────────────────────────
case("login_row", label="Nous", status={"logged_in": True})
case("login_row", label="Nous", status={"logged_in": False}, show_error=False)
case("login_row", label="Codex", status={"logged_in": False, "error": "bad"}, show_error=True)
case("login_row", label="Codex", status={"logged_in": False, "error": ""}, show_error=True)
case("login_row", label="X", status={})
case("login_row", label="X", status={"logged_in": True}, ok_detail="(custom)")
case("login_row", label="X", status={"logged_in": 0})

# ── _print_summary ───────────────────────────────────────────────────────
case("print_summary", should_fix=False, state={"finding": F()})
case("print_summary", should_fix=False, state={"finding": F(["a"])})
case("print_summary", should_fix=False, state={"finding": F(["a"], ["m"])})
case("print_summary", should_fix=True, state={"finding": F([], [], 2)})
case("print_summary", should_fix=True, state={"finding": F(["a"], [], 1)})
case("print_summary", should_fix=True, state={"finding": F([], ["m"], 0)})
case("print_summary", should_fix=False, state={"finding": F(["a", "b"])})

# ── _ack_advisory ────────────────────────────────────────────────────────
case("ack_advisory", ack_target="x", advisory_ids=["x", "y"], dhh="~/.hermes",
     io={"advisories_import": R(None), "ack_advisory": R(True)})
case("ack_advisory", ack_target="x", advisory_ids=["x"], dhh="~/.hermes",
     io={"advisories_import": R(None), "ack_advisory": R(False)})
case("ack_advisory", ack_target="zz", advisory_ids=["x", "y"], dhh="~/.hermes",
     io={"advisories_import": R(None)})
case("ack_advisory", ack_target="zz", advisory_ids=[], dhh="~/.hermes",
     io={"advisories_import": R(None)})
case("ack_advisory", ack_target="x", advisory_ids=["x"], dhh="~/.hermes",
     io={"advisories_import": IMP("ImportError", "no module")})
case("ack_advisory", ack_target="x", advisory_ids=["x"], dhh="~/.hermes",
     io={"advisories_import": R(None), "ack_advisory": X("RuntimeError", "boom")})
case("ack_advisory", ack_target="x", advisory_ids=["x"], dhh="~/.hermes",
     io={"advisories_import": R(None), "ack_advisory": X("SystemExit", "3", code=3)})
case("ack_advisory", ack_target="x", advisory_ids=["x"], dhh="~/.hermes", io={})

# ── _check_auth_providers ────────────────────────────────────────────────
AUTH_OK = {"auth_import": R(None), "get_nous_auth_status_local": R({"logged_in": True}),
           "get_codex_auth_status": R({"logged_in": True}),
           "get_minimax_oauth_auth_status": R({"logged_in": True, "region": "cn"}),
           "xai_import": R(None), "get_xai_oauth_auth_status": R({"logged_in": True})}
case("check_auth_providers", should_fix=False, state={"finding": F()}, io=dict(AUTH_OK))
case("check_auth_providers", should_fix=False, state={"finding": F()},
     io=dict(AUTH_OK, get_codex_auth_status=R({"logged_in": False}), _safe_which=R(None)))
case("check_auth_providers", should_fix=False, state={"finding": F()},
     io=dict(AUTH_OK, get_codex_auth_status=R({"logged_in": False}), _safe_which=R("/usr/bin/codex")))
case("check_auth_providers", should_fix=False, state={"finding": F()},
     io=dict(AUTH_OK, get_nous_auth_status_local=X("ValueError", "boom")))
case("check_auth_providers", should_fix=False, state={"finding": F()},
     io=dict(AUTH_OK, auth_import=IMP("ImportError", "no auth")))
case("check_auth_providers", should_fix=False, state={"finding": F()},
     io=dict(AUTH_OK, get_nous_auth_status_local=X("SystemExit", "1", code=1)))
case("check_auth_providers", should_fix=False, state={"finding": F()},
     io=dict(AUTH_OK, get_minimax_oauth_auth_status=R(None)))
case("check_auth_providers", should_fix=False, state={"finding": F()},
     io=dict(AUTH_OK, get_xai_oauth_auth_status=X("ValueError", "xai boom")))
case("check_auth_providers", should_fix=False, state={"finding": F()},
     io=dict(AUTH_OK, get_xai_oauth_auth_status=R(None)))
case("check_auth_providers", should_fix=False, state={"finding": F()}, io={})

# ── _check_api_connectivity ──────────────────────────────────────────────
def RES(label, lines, issues):
    return {"label": label, "lines": lines, "issues": issues}
case("check_api_connectivity", should_fix=False, state={"finding": F()},
     io={"build_probes": R([1, 2]), "run_probes": R([RES("A", [["\u2713", "A", "ok"]], [])])})
case("check_api_connectivity", should_fix=False, state={"finding": F()},
     io={"build_probes": R([1]), "run_probes": R([RES("A", [["\u2717", "A", "bad"]], ["fix A"])]),
         "_has_healthy_oauth_fallback_for_apikey_provider": R(False)})
case("check_api_connectivity", should_fix=False, state={"finding": F()},
     io={"build_probes": R([1]), "run_probes": R([RES("A", [["\u2717", "A", ""]], ["fix A"])]),
         "_has_healthy_oauth_fallback_for_apikey_provider": R(True)})
case("check_api_connectivity", should_fix=False, state={"finding": F()},
     io={"build_probes": R([1]), "run_probes": R([RES("A", [["\u2713", "A", "d1"], ["\u26a0", "A", ""]], [])])})
case("check_api_connectivity", should_fix=False, state={"finding": F()},
     io={"build_probes": X("RuntimeError", "boom")})
case("check_api_connectivity", should_fix=False, state={"finding": F()},
     io={"build_probes": R([1]), "run_probes": X("OSError", "net")})
case("check_api_connectivity", should_fix=False, state={"finding": F()},
     io={"build_probes": R([1]), "run_probes": R([RES("A", [], ["i"])]),
         "_has_healthy_oauth_fallback_for_apikey_provider": X("RuntimeError", "x")})
case("check_api_connectivity", should_fix=False, state={"finding": F()}, io={})
case("check_api_connectivity", should_fix=False, state={"finding": F()},
     io={"build_probes": R([]), "run_probes": R([])})

# ── run_doctor ───────────────────────────────────────────────────────────
RD_BASE = {"os_environ_setdefault": R(None), "installs_held": R(None),
           "live_import": R(None), "maybe_run_live_checks": R(None)}
case("run_doctor", args={"fix": False}, checks=[{"title": "Sec", "name": "chk1"}],
     advisory_ids=[], dhh="~/.hermes", state={"finding": F()},
     io=dict(RD_BASE, chk1=R(F())))
case("run_doctor", args={"fix": False}, checks=[{"title": "Sec", "name": "chk1"}],
     advisory_ids=[], dhh="~/.hermes", state={"finding": F()},
     io=dict(RD_BASE, chk1=R(F(["issue1"]))))
case("run_doctor", args={"ack": "x"}, checks=[], advisory_ids=["x"], dhh="~/.hermes",
     state={"finding": F()}, io={"os_environ_setdefault": R(None), "advisories_import": R(None), "ack_advisory": R(True)})
case("run_doctor", args={"ack": "zz"}, checks=[], advisory_ids=["x"], dhh="~/.hermes",
     state={"finding": F()}, io={"os_environ_setdefault": R(None), "advisories_import": R(None)})
case("run_doctor", args={"fix": False}, checks=[{"title": "Sec", "name": "chk1"}],
     advisory_ids=[], dhh="~/.hermes", state={"finding": F()},
     io=dict(RD_BASE, chk1=X("RuntimeError", "check boom")))
case("run_doctor", args={"fix": False}, checks=[], advisory_ids=[], dhh="~/.hermes",
     state={"finding": F()}, io=dict(RD_BASE, installs_held=IMP("ImportError", "no")))
case("run_doctor", args={"fix": False}, checks=[], advisory_ids=[], dhh="~/.hermes",
     state={"finding": F()}, io={"os_environ_setdefault": X("OSError", "env")})
case("run_doctor", args={"fix": False}, checks=[{"title": None, "name": "chk1"}],
     advisory_ids=[], dhh="~/.hermes", state={"finding": F()},
     io=dict(RD_BASE, chk1=R(F()), live_import=IMP("ImportError", "no live")))
case("run_doctor", args={"fix": True}, checks=[{"title": "S", "name": "chk1"}],
     advisory_ids=[], dhh="~/.hermes", state={"finding": F()},
     io=dict(RD_BASE, chk1=R(F([], [], 1)), maybe_run_live_checks=X("ValueError", "live boom")))
case("run_doctor", args={"fix": False}, checks=[{"title": "S", "name": "chk1"}],
     advisory_ids=[], dhh="~/.hermes", state={"finding": F()},
     io=dict(RD_BASE, chk1=R(F()), maybe_run_live_checks=X("SystemExit", "1", code=1)))
case("run_doctor", args={"fix": False}, checks=[{"title": "A", "name": "c1"}, {"title": None, "name": "c2"}],
     advisory_ids=[], dhh="~/.hermes", state={"finding": F()},
     io=dict(RD_BASE, c1=R(F(["i1"], [], 1)), c2=R(F([], ["m1"], 2))))
case("run_doctor", args={"fix": False}, checks=[], advisory_ids=[], dhh="~/.hermes",
     state={"finding": F()}, io={})

# ── maybe_run_live_checks ────────────────────────────────────────────────
case("maybe_run_live_checks", args={"live": False}, state={"live_issues": []}, io={})
case("maybe_run_live_checks", args={"live": True}, state={"live_issues": []},
     io={"run_live_checks": R([{"name": "X", "status": "pass", "detail": ""}])})
case("maybe_run_live_checks", args={"live": True}, state={"live_issues": []},
     io={"run_live_checks": X("ValueError", "boom")})
case("maybe_run_live_checks", args={"live": True}, state={"live_issues": []},
     io={"run_live_checks": X("SystemExit", "1", code=1)})
case("maybe_run_live_checks", args={}, state={"live_issues": []}, io={})
case("maybe_run_live_checks", args={"live": True}, state={"live_issues": []},
     io={"run_live_checks": X("RuntimeError", "")})
case("maybe_run_live_checks", args={"live": True}, state={"live_issues": []}, io={})

# ── _keyed_probe ─────────────────────────────────────────────────────────
KP = dict(name="Firecrawl", url="https://api.firecrawl.dev/v2/team/credit-usage",
          env_var="FIRECRAWL_API_KEY", scheme="Bearer", timeout=10.0)
case("keyed_probe", io={"os_getenv": R("")}, **KP)
case("keyed_probe", io={"os_getenv": R("   ")}, **KP)
case("keyed_probe", io={"os_getenv": R("key"), "_http_get": R({"status_code": 200}),
                        "_classify_http": R({"name": "Firecrawl", "status": "pass", "detail": "(HTTP 200)"})}, **KP)
case("keyed_probe", io={"os_getenv": R("key"), "_http_get": R({"status_code": 401}),
                        "_classify_http": R({"name": "Firecrawl", "status": "fail", "detail": "(HTTP 401)"})}, **KP)
case("keyed_probe", io={"os_getenv": X("OSError", "env")}, **KP)
case("keyed_probe", io={"os_getenv": R("key"), "_http_get": X("TimeoutError", "timed out")}, **KP)
case("keyed_probe", io={"os_getenv": R("key"), "_http_get": R({}), "_classify_http": X("RuntimeError", "x")}, **KP)
case("keyed_probe", io={}, **KP)

# ── _probe_audio ─────────────────────────────────────────────────────────
case("probe_audio", kind="tts", config={"tts": {"provider": "local"}}, timeout=10.0, io={})
case("probe_audio", kind="tts", config={"tts": {"provider": ""}}, timeout=10.0, io={})
case("probe_audio", kind="tts", config={"tts": {"provider": "unknown"}}, timeout=10.0, io={})
case("probe_audio", kind="tts", config={"tts": {"provider": "openai"}}, timeout=10.0, io={"os_getenv": R("")})
case("probe_audio", kind="tts", config={"tts": {"provider": "openai"}}, timeout=10.0,
     io={"os_getenv": R("k"), "_http_get": R({"status_code": 200}),
         "_classify_http": R({"name": "TTS", "status": "pass", "detail": "(HTTP 200)"})})
case("probe_audio", kind="stt", config={"stt": {"provider": "elevenlabs"}}, timeout=10.0,
     io={"os_getenv": R("k"), "_http_get": R({"status_code": 200}),
         "_classify_http": R({"name": "STT", "status": "pass", "detail": "(HTTP 200)"})})
case("probe_audio", kind="tts", config={}, timeout=10.0, io={})
case("probe_audio", kind="tts", config={"tts": {"provider": "groq"}}, timeout=10.0, io={"os_getenv": X("OSError", "x")})
case("probe_audio", kind="tts", config={"tts": {"provider": "openai"}}, timeout=10.0,
     io={"os_getenv": R("k"), "_http_get": R({}), "_classify_http": X("RuntimeError", "x")})
case("probe_audio", kind="tts", config={"tts": {"provider": "openai"}}, timeout=10.0, io={})

# ── _bits ────────────────────────────────────────────────────────────────
case("bits", pairs=[{"value": 1, "fmt": "f1"}, {"value": None, "fmt": "f2"}], io={"f1": R("a")})
case("bits", pairs=[{"value": 0, "fmt": "f1"}], io={"f1": R("zero")})
case("bits", pairs=[{"value": None, "fmt": "f1"}], io={})
case("bits", pairs=[{"value": 1, "fmt": "f1"}, {"value": 2, "fmt": "f2"}], io={"f1": R("a"), "f2": R("b")})
case("bits", pairs=[], io={})
case("bits", pairs=[{"value": 1, "fmt": "f1"}], io={"f1": X("RuntimeError", "boom")})
case("bits", pairs=[{"value": "", "fmt": "f1"}], io={"f1": R("e")})
case("bits", pairs=[{"value": 1, "fmt": "f1"}], io={})

# ── host_gateway_note ────────────────────────────────────────────────────
case("host_gateway_note", io={"host_topology_import": R(None), "host_gateway_topology": R(None)})
case("host_gateway_note", io={"host_topology_import": R(None), "host_gateway_topology": R({}),
                              "topology_describe": R("the host gateway (PID 42) serving profiles default")})
case("host_gateway_note", io={"host_topology_import": IMP("ImportError", "no")})
case("host_gateway_note", io={"host_topology_import": R(None), "host_gateway_topology": X("ValueError", "x")})
case("host_gateway_note", io={"host_topology_import": R(None), "host_gateway_topology": X("SystemExit", "1", code=1)})
case("host_gateway_note", io={"host_topology_import": R(None), "host_gateway_topology": R({}),
                              "topology_describe": X("RuntimeError", "desc boom")})
case("host_gateway_note", io={})

# ── _session_count ───────────────────────────────────────────────────────
case("session_count", path="/h/state.db", io={"open_store_admin": R({}), "session_row_count": R(5)})
case("session_count", path="/h/state.db", io={"open_store_admin": R({}), "session_row_count": R(0)})
case("session_count", path="/h/state.db", io={"open_store_admin": X("OSError", "locked")})
case("session_count", path="/h/state.db", io={"open_store_admin": R({}), "session_row_count": X("RuntimeError", "x")})
case("session_count", path="/h/state.db", io={})
case("session_count", path="/h/state.db", io={"open_store_admin": R({}), "session_row_count": R(100)})

# ── _endpoint_url ────────────────────────────────────────────────────────
case("endpoint_url", entry={"api": "HTTP://X.com/"})
case("endpoint_url", entry={"base_url": " http://y.com/ "})
case("endpoint_url", entry={"url": "http://z.com"})
case("endpoint_url", entry={"api": "", "base_url": "http://b.com"})
case("endpoint_url", entry={})
case("endpoint_url", entry={"api": "http://a.com", "base_url": "http://b.com"})
case("endpoint_url", entry={"api": "  http://c.com///  "})
case("endpoint_url", entry={"url": None})

# ── _check_retired_session_reset ─────────────────────────────────────────
SR_BASE = {"config_effective_import": R(None), "session_reset_import": R(None),
           "load_user_config_effective": R({})}
case("check_retired_session_reset", should_fix=False, state={"finding": F()},
     io=dict(SR_BASE, retired_reset_policy=R(None)))
case("check_retired_session_reset", should_fix=False, state={"finding": F()},
     io=dict(SR_BASE, retired_reset_policy=R(["idle", "30"]), reset_plugin_enabled=R(True)))
case("check_retired_session_reset", should_fix=False, state={"finding": F()},
     io=dict(SR_BASE, retired_reset_policy=R(["daily", "x"]), reset_plugin_enabled=R(False), format_notice=R("notice")))
case("check_retired_session_reset", should_fix=False, state={"finding": F()},
     io=dict(SR_BASE, config_effective_import=IMP("ImportError", "no")))
case("check_retired_session_reset", should_fix=False, state={"finding": F()},
     io=dict(SR_BASE, load_user_config_effective=X("RuntimeError", "x")))
case("check_retired_session_reset", should_fix=False, state={"finding": F()},
     io=dict(SR_BASE, retired_reset_policy=X("ValueError", "x")))
case("check_retired_session_reset", should_fix=False, state={"finding": F()},
     io=dict(SR_BASE, retired_reset_policy=R(["idle", "30"]), reset_plugin_enabled=X("RuntimeError", "x")))
case("check_retired_session_reset", should_fix=False, state={"finding": F()},
     io=dict(SR_BASE, retired_reset_policy=R(["daily", "x"]), reset_plugin_enabled=R(False), format_notice=X("RuntimeError", "x")))
case("check_retired_session_reset", should_fix=False, state={"finding": F()}, io={})

# ── _check_xai_retirement ────────────────────────────────────────────────
XAI_BASE = {"config_import": R(None), "xai_retirement_import": R(None), "load_config": R({})}
MU = "https://example.com/migrate"
case("check_xai_retirement", should_fix=False, migration_url=MU, state={"finding": F()},
     io=dict(XAI_BASE, find_retired_xai_refs=R([])))
case("check_xai_retirement", should_fix=False, migration_url=MU, state={"finding": F()},
     io=dict(XAI_BASE, find_retired_xai_refs=R(["r1"]), format_issue=R("issue1")))
case("check_xai_retirement", should_fix=False, migration_url=MU, state={"finding": F()},
     io=dict(XAI_BASE, find_retired_xai_refs=R(["r1", "r2"]), format_issue=[R("i1"), R("i2")]))
case("check_xai_retirement", should_fix=False, migration_url=MU, state={"finding": F()},
     io=dict(XAI_BASE, config_import=IMP("ImportError", "no")))
case("check_xai_retirement", should_fix=False, migration_url=MU, state={"finding": F()},
     io=dict(XAI_BASE, load_config=X("RuntimeError", "x")))
case("check_xai_retirement", should_fix=False, migration_url=MU, state={"finding": F()},
     io=dict(XAI_BASE, find_retired_xai_refs=X("ValueError", "x")))
case("check_xai_retirement", should_fix=False, migration_url=MU, state={"finding": F()},
     io=dict(XAI_BASE, find_retired_xai_refs=R(["r1"]), format_issue=X("RuntimeError", "x")))
case("check_xai_retirement", should_fix=False, migration_url=MU, state={"finding": F()},
     io=dict(XAI_BASE, find_retired_xai_refs=R(None)))
case("check_xai_retirement", should_fix=False, migration_url=MU, state={"finding": F()}, io={})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"gen_doctor_flow: {len(CASES)} cases, {len(counts)} funcs")
    low = {k: v for k, v in counts.items() if v < 5}
    if low:
        print("WARNING funcs with <5 cases:", low)
    print("wrote", OUT)


if __name__ == "__main__":
    main()