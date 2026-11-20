#!/usr/bin/env python3
"""Generate tests/fixtures/observability_metrics_flow.jsonl — the scripted-seam
case corpus for the observability_metrics_flow FLOW port (the real defining
module of the task-named metrics.py: hermes_cli/observability/
shared_metrics_fields.py). >=5 cases per ported func.

Happy-path / branch cases are CAPTURED against the LIVE quarry modules: the
collaborator names the field builders read (fields._bucket / _norm /
user_created_skill, contract._non_negative_number / count_bucket / size_bucket /
execution_surface / model_error_class, catalog.provider_metric_name /
model_metric_name / aux_task_metric_name / slash_command_metric_name /
extension_metric_name) are wrapped with recorders that DELEGATE to the real
functions and freeze their returns into per-name queue scripts. The oracle
replays those scripts through its AST-extracted live defs, so both sides see
identical seam results. Ported siblings stay REAL during capture (internal
composition). Error / fail-closed / BaseException / null-script cases are
hand-authored.

Each case: {"func": <live_name>, "in": {...args..., "io": {seam: script}}}.
A bare JSON array script is a per-call QUEUE; a raise is {"__raise__": kind,
"message": msg}. Deterministic (no randomness). ASCII scope; integral floats
are EXCLUDED (int/float identity domain — see the port header).
"""
import importlib.util
import json
import math
import os
import sys
from fractions import Fraction

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
QUARRY = os.path.join(os.path.dirname(REPO), "hermes-agent")
if not os.path.isfile(os.path.join(QUARRY, "hermes_cli", "observability", "shared_metrics_fields.py")):
    QUARRY = os.path.join(os.path.expanduser("~"), "hermes-agent")
OUT = os.path.join(HERE, "fixtures", "observability_metrics_flow.jsonl")

# real quarry modules FIRST (the oracle keeps a real `tools` parent when present)
sys.path.insert(0, QUARRY)
import hermes_cli.observability.shared_metrics_fields as F          # noqa: E402
import hermes_cli.observability.shared_metrics_contract as CTR      # noqa: E402
import hermes_cli.observability.shared_metrics_catalog as CAT       # noqa: E402

# then the oracle (AST-extracted live namespace + proj/norm/mk_error helpers)
_spec = importlib.util.spec_from_file_location(
    "oracle_observability_metrics_flow", os.path.join(HERE, "oracle_observability_metrics_flow.py"))
O = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(O)

CASES = []
CURRENT_IO = []


def _wrap(name, orig):
    def f(*a, **k):
        ret = orig(*a, **k)
        CURRENT_IO.append((name, O.norm_args(O.proj(name, a, k)), O.norm_value(ret)))
        return ret
    return f


# patch ONLY the collaborator names the builders read; never the ported
# siblings (internal composition stays real) nor contract._bucket / _norm
# (the seamed count_bucket / size_bucket / execution_surface bodies use them).
_orig_skill_pred = None
for _m, _rules in F._MILESTONE_RULES.items():
    for _nm, _pred in _rules:
        if _nm == "first_skill_created":
            _orig_skill_pred = _pred
F._MILESTONE_RULES[CTR.SKILL_LIFECYCLE_METRIC] = (
    ("first_skill_created", _wrap("user_created_skill", _orig_skill_pred)),
)
F._bucket = _wrap("_bucket", F._bucket)
F._norm = _wrap("_norm", F._norm)
F.user_created_skill = _wrap("user_created_skill", F.user_created_skill)
CTR._non_negative_number = _wrap("contract._non_negative_number", CTR._non_negative_number)
CTR.count_bucket = _wrap("contract.count_bucket", CTR.count_bucket)
CTR.size_bucket = _wrap("contract.size_bucket", CTR.size_bucket)
CTR.execution_surface = _wrap("contract.execution_surface", CTR.execution_surface)
CTR.model_error_class = _wrap("contract.model_error_class", CTR.model_error_class)
CAT.provider_metric_name = _wrap("catalog.provider_metric_name", CAT.provider_metric_name)
CAT.model_metric_name = _wrap("catalog.model_metric_name", CAT.model_metric_name)
CAT.aux_task_metric_name = _wrap("catalog.aux_task_metric_name", CAT.aux_task_metric_name)
CAT.slash_command_metric_name = _wrap("catalog.slash_command_metric_name", CAT.slash_command_metric_name)
CAT.extension_metric_name = _wrap("catalog.extension_metric_name", CAT.extension_metric_name)


def _capture_call(func, inp):
    """Drive the REAL fields func with the same binding as oracle.drive."""
    mk = O.mk_error
    if func == "_number":
        return F._number(inp.get("value"))
    if func == "provider_identifier":
        return F.provider_identifier(inp.get("value"))
    if func == "session_fields":
        return F.session_fields(inp.get("start_fields", {}), turns=inp["turns"],
                                failed_turns=inp["failed_turns"], last_outcome=inp["last_outcome"],
                                active_ms=inp["active_ms"])
    if func == "install_age_bucket":
        return F.install_age_bucket(inp.get("age_seconds"))
    if func == "ttft_bucket":
        return F.ttft_bucket(inp.get("event", {}))
    if func == "context_fill_bucket":
        return F.context_fill_bucket(inp.get("tokens_before"), inp.get("context_length"))
    if func == "model_token_fields":
        return F.model_token_fields(inp.get("usage"), model=inp.get("model"),
                                    provider=inp.get("provider"), call_role=inp["call_role"],
                                    aux_task=inp.get("aux_task"))
    if func == "setup_completed_fields":
        return F.setup_completed_fields(surface=inp.get("surface"), provider=inp.get("provider"))
    if func == "compression_failure_class":
        return F.compression_failure_class(inp["outcome"], inp.get("failure_class"))
    if func == "compression_fields":
        return F.compression_fields(trigger=inp.get("trigger"), outcome=inp.get("outcome"),
                                    tokens_before=inp.get("tokens_before"),
                                    context_length=inp.get("context_length"),
                                    failure_class=inp.get("failure_class"))
    if func == "model_switch_fields":
        return F.model_switch_fields(from_provider=inp.get("from_provider"),
                                     to_provider=inp.get("to_provider"), surface=inp.get("surface"))
    if func == "fallback_fields":
        return F.fallback_fields(from_provider=inp.get("from_provider"),
                                 to_provider=inp.get("to_provider"), reason=mk(inp.get("reason")))
    if func == "slash_command_fields":
        return F.slash_command_fields(command=inp.get("command"), surface=inp.get("surface"))
    if func == "extension_install_fields":
        return F.extension_install_fields(kind=inp.get("kind"), source=inp.get("source"),
                                          name=inp.get("name"), outcome=inp.get("outcome"),
                                          failure_class=inp.get("failure_class"),
                                          registry=inp.get("registry"), error=mk(inp.get("error")))
    if func == "tagged_failure_class":
        return F.tagged_failure_class(mk(inp.get("error")))
    if func == "exception_failure_class":
        return F.exception_failure_class(mk(inp.get("error")))
    if func == "extension_failure_class":
        return F.extension_failure_class(inp["kind"], inp["outcome"], inp.get("failure_class"),
                                         mk(inp.get("error")))
    if func == "extension_registry":
        return F.extension_registry(inp["kind"], inp.get("registry"))
    if func == "_long_session":
        return F._long_session(inp.get("d", {}))
    if func == "milestones_for":
        return F.milestones_for(inp["metric_name"], inp.get("dimensions", {}))
    raise RuntimeError("unknown func " + func)


def add(func, io=None, **inp):
    if io is None:
        CURRENT_IO.clear()
        try:
            _capture_call(func, inp)
        except BaseException:  # noqa: BLE001 - real-raise cases keep partial io
            pass
        queues = {}
        for name, _args, ret in CURRENT_IO:
            queues.setdefault(name, []).append(ret)
        io = queues
    CASES.append({"func": func, "in": {**inp, "io": io}})


def raise_(kind, msg="boom"):
    return {"__raise__": kind, "message": msg}


def exc(type_, module="builtins", mro=None, **kw):
    d = {"__exc__": True, "type": type_, "module": module, "message": kw.pop("message", "x")}
    if mro is not None:
        # the mro must START with the type itself (type(error).__mro__[0])
        if not mro or mro[0].get("name") != type_:
            mro = [{"name": type_, "module": module}] + list(mro)
        d["mro"] = mro
    d.update(kw)
    return d


def mro_of(*pairs):
    return [{"name": n, "module": m} for n, m in pairs]


BASE_MRO = mro_of(("Exception", "builtins"), ("BaseException", "builtins"), ("object", "builtins"))
OSERROR_MRO = mro_of(("OSError", "builtins"), ("Exception", "builtins"),
                     ("BaseException", "builtins"), ("object", "builtins"))

# ── _number ──
add("_number", value=5)
add("_number", value=0)
add("_number", value=-3)
add("_number", value="x")
add("_number", value=None)
add("_number", value=True)
add("_number", io={"contract._non_negative_number": raise_("RuntimeError")}, value=5)
add("_number", io={}, value=5)

# ── provider_identifier ──
add("provider_identifier", value="openai")
add("provider_identifier", value="custom:acme")
add("provider_identifier", value="")
add("provider_identifier", value=None)
add("provider_identifier", value=" Nous ")
add("provider_identifier", io={"catalog.provider_metric_name": raise_("ValueError")}, value="openai")
add("provider_identifier", io={}, value="openai")

# ── session_fields ──
add("session_fields", start_fields={"entrypoint": "cli", "execution_surface": "tui", "platform": "none"},
    turns=3, failed_turns=0, last_outcome="success", active_ms=120000)
add("session_fields", start_fields={}, turns=0, failed_turns=0, last_outcome="unknown", active_ms=0)
add("session_fields", start_fields={"entrypoint": "scheduled_task"}, turns=25, failed_turns=7,
    last_outcome="timed_out", active_ms=-5)
add("session_fields", start_fields={"entrypoint": None, "platform": None}, turns=1, failed_turns=0,
    last_outcome="success", active_ms=60000)
add("session_fields", start_fields={}, turns=2, failed_turns=1, last_outcome="bogus", active_ms=300000)
add("session_fields", start_fields={}, turns=2, failed_turns=1, last_outcome=None, active_ms=300000)
add("session_fields", start_fields=None, turns=1, failed_turns=0, last_outcome="success", active_ms=5)
add("session_fields", start_fields={}, turns=1, failed_turns=0, last_outcome=["x"], active_ms=5)
add("session_fields", io={"_bucket": raise_("RuntimeError")},
    start_fields={}, turns=1, failed_turns=0, last_outcome="success", active_ms=5)
add("session_fields", io={}, start_fields={}, turns=1, failed_turns=0, last_outcome="success", active_ms=5)

# ── install_age_bucket ──
add("install_age_bucket", age_seconds=7200)
add("install_age_bucket", age_seconds=0)
add("install_age_bucket", age_seconds=3600)
add("install_age_bucket", age_seconds=90 * 86400)
add("install_age_bucket", age_seconds=None)
add("install_age_bucket", age_seconds="x")
add("install_age_bucket", age_seconds=-5)
add("install_age_bucket", io={"contract._non_negative_number": raise_("RuntimeError")}, age_seconds=5)
add("install_age_bucket", io={}, age_seconds=5)

# ── ttft_bucket ──
add("ttft_bucket", event={"started_at": 10, "first_chunk_at": 10.5})
add("ttft_bucket", event={"started_at": 10})
add("ttft_bucket", event={"first_chunk_at": 10.5})
add("ttft_bucket", event={})
add("ttft_bucket", event={"started_at": 2.5, "first_chunk_at": 1.25})
add("ttft_bucket", event={"started_at": 0.5, "first_chunk_at": 1.75})
add("ttft_bucket", event={"started_at": 1, "first_chunk_at": 16})
add("ttft_bucket", event=None)
add("ttft_bucket", io={"contract._non_negative_number": [raise_("RuntimeError")]},
    event={"started_at": 1, "first_chunk_at": 2})
add("ttft_bucket", io={}, event={"started_at": 1, "first_chunk_at": 2})

# ── context_fill_bucket ──
add("context_fill_bucket", tokens_before=25, context_length=200)
add("context_fill_bucket", tokens_before=150, context_length=200)
add("context_fill_bucket", tokens_before=180, context_length=200)
add("context_fill_bucket", tokens_before=200, context_length=200)
add("context_fill_bucket", tokens_before=250, context_length=200)
add("context_fill_bucket", tokens_before=None, context_length=200)
add("context_fill_bucket", tokens_before=25, context_length=0)
add("context_fill_bucket", tokens_before=25, context_length=None)
add("context_fill_bucket", tokens_before=0, context_length=200)
add("context_fill_bucket", io={"contract._non_negative_number": [raise_("ValueError")]},
    tokens_before=25, context_length=200)
add("context_fill_bucket", io={}, tokens_before=25, context_length=200)

# ── model_token_fields ──
add("model_token_fields", usage={"input_tokens": 5, "output_tokens": 7}, model="gpt-4",
    provider="openai", call_role="primary")
add("model_token_fields", usage={"input_tokens": 5}, model="gpt-4", provider="openai",
    call_role="auxiliary", aux_task="title")
add("model_token_fields", usage={"input_tokens": 0}, model="m", provider="p", call_role="primary")
add("model_token_fields", usage={"input_tokens": True, "output_tokens": -3, "cache_read": "x"},
    model="m", provider="p", call_role="primary")
add("model_token_fields", usage={"input_tokens": 5.5}, model="m", provider="p", call_role="primary")
add("model_token_fields", usage="x", model="m", provider="p", call_role="primary")
add("model_token_fields", usage={"reasoning_tokens": 2}, model="m", provider="p",
    call_role="auxiliary", aux_task=None)
add("model_token_fields", usage={"output_tokens": 1}, model=None, provider=None, call_role="primary")
add("model_token_fields", io={"catalog.provider_metric_name": [raise_("RuntimeError")]},
    usage={"input_tokens": 5}, model="m", provider="p", call_role="primary")
add("model_token_fields", io={}, usage={"input_tokens": 5}, model="m", provider="p", call_role="primary")

# ── setup_completed_fields ──
add("setup_completed_fields", surface=" CLI ", provider="openai")
add("setup_completed_fields", surface="desktop", provider="")
add("setup_completed_fields", surface="bogus", provider=None)
add("setup_completed_fields", surface=None, provider="openai")
add("setup_completed_fields", surface="cli", provider=0)
add("setup_completed_fields", io={"_norm": raise_("RuntimeError")}, surface="cli", provider="p")
add("setup_completed_fields", io={}, surface="cli", provider="p")

# ── compression_failure_class ──
add("compression_failure_class", outcome="failed", failure_class="exception:ValueError")
add("compression_failure_class", outcome="success", failure_class=None)
add("compression_failure_class", outcome="success", failure_class="summary_model_benched")
add("compression_failure_class", outcome="success", failure_class="none")
add("compression_failure_class", outcome="failed", failure_class="")
add("compression_failure_class", outcome="failed", failure_class="bogus")
add("compression_failure_class", outcome="failed", failure_class="rollback:TimeoutError")
add("compression_failure_class", outcome="skipped", failure_class="lock_contended")
add("compression_failure_class", outcome="failed", failure_class=5)
add("compression_failure_class", io={"_norm": [None]}, outcome="failed", failure_class="x")
add("compression_failure_class", io={"_norm": raise_("RuntimeError")}, outcome="failed", failure_class="x")
add("compression_failure_class", io={}, outcome="failed", failure_class="x")

# ── compression_fields ──
add("compression_fields", trigger="auto", outcome="success", tokens_before=25, context_length=200,
    failure_class=None)
add("compression_fields", trigger="manual", outcome="ok", tokens_before=150, context_length=200,
    failure_class="exception:X")
add("compression_fields", trigger="idle", outcome="skipped", tokens_before=180, context_length=200,
    failure_class="lock_contended")
add("compression_fields", trigger="bogus", outcome="bogus", tokens_before=25, context_length=200)
add("compression_fields", trigger="overflow", outcome="failed", tokens_before=None, context_length=200,
    failure_class="no_progress")
add("compression_fields", io={"_norm": [raise_("RuntimeError")]},
    trigger="auto", outcome="success", tokens_before=25, context_length=200)
add("compression_fields", io={}, trigger="auto", outcome="success", tokens_before=25, context_length=200)

# ── model_switch_fields ──
add("model_switch_fields", from_provider="openai", to_provider="anthropic", surface="tui")
add("model_switch_fields", from_provider="openai", to_provider="anthropic", surface=None)
add("model_switch_fields", from_provider=None, to_provider=None, surface="cli")
add("model_switch_fields", io={"contract.execution_surface": raise_("RuntimeError")},
    from_provider="a", to_provider="b", surface="cli")
add("model_switch_fields", io={"contract.execution_surface": "cli",
                               "catalog.provider_metric_name": ["a", raise_("ValueError")]},
    from_provider="a", to_provider="b", surface="cli")
add("model_switch_fields", io={}, from_provider="a", to_provider="b", surface="cli")

# ── fallback_fields ──
add("fallback_fields", from_provider="openai", to_provider="anthropic", reason="rate_limit")
add("fallback_fields", from_provider="openai", to_provider="anthropic", reason={"value": "rate_limit"})
add("fallback_fields", from_provider="a", to_provider="b", reason={"value": None})
add("fallback_fields", from_provider="a", to_provider="b", reason=None)
add("fallback_fields", from_provider="a", to_provider="b", reason={"value": "bogus"})
add("fallback_fields", io={"contract.model_error_class": raise_("RuntimeError")},
    from_provider="a", to_provider="b", reason="rate_limit")
add("fallback_fields", io={}, from_provider="a", to_provider="b", reason="rate_limit")

# ── slash_command_fields ──
add("slash_command_fields", command="/help", surface="cli")
add("slash_command_fields", command="bogus", surface="tui")
add("slash_command_fields", command=None, surface=None)
add("slash_command_fields", io={"catalog.slash_command_metric_name": raise_("RuntimeError")},
    command="/help", surface="cli")
add("slash_command_fields", io={"catalog.slash_command_metric_name": "help",
                                "contract.execution_surface": raise_("ValueError")},
    command="/help", surface="cli")
add("slash_command_fields", io={}, command="/help", surface="cli")

# ── extension_install_fields ──
add("extension_install_fields", kind="skill", source="hub", name="my-skill", outcome="success",
    registry="skills.sh", failure_class=None)
add("extension_install_fields", kind="plugin", source="local", name="p", outcome="failed",
    failure_class="clone_failed")
add("extension_install_fields", kind="mcp_server", source="url", name="m", outcome="ok",
    failure_class=None, error=exc("ValueError"))
add("extension_install_fields", kind="bogus", source="hub", name="x", outcome="success")
add("extension_install_fields", kind="skill", source="bogus", name="x", outcome="installed")
add("extension_install_fields", kind="skill", source="hub", name="x", outcome="failed",
    failure_class=None, error=exc("PermissionError"))
add("extension_install_fields", io={"_norm": [raise_("RuntimeError")]},
    kind="skill", source="hub", name="x", outcome="success")
add("extension_install_fields", io={"_norm": ["skill", "hub", "success"],
                                    "catalog.extension_metric_name": raise_("ValueError")},
    kind="skill", source="hub", name="x", outcome="success")
add("extension_install_fields", io={}, kind="skill", source="hub", name="x", outcome="success")

# ── tagged_failure_class ──
add("tagged_failure_class", error=exc("ValueError", failure_class="tagged"))
add("tagged_failure_class", error=exc("ValueError"))
add("tagged_failure_class", error=exc("ValueError", failure_class=""))
add("tagged_failure_class", error=exc("ValueError", failure_class=5))
add("tagged_failure_class", error=exc("HookError", module="probe", mro=BASE_MRO, hook=True))
add("tagged_failure_class", error=exc("HookError", module="probe", mro=BASE_MRO, hook=True,
                                      failure_class="tagged"))
add("tagged_failure_class", error=exc("PropError", module="probe", mro=BASE_MRO,
                                      failure_class_property=True))
add("tagged_failure_class", error=None)
add("tagged_failure_class", error={"failure_class": "x"})

# ── exception_failure_class ──
add("exception_failure_class", error=exc("PermissionError"))
add("exception_failure_class", error=exc("ConnectionResetError"))
add("exception_failure_class", error=exc("TimeoutError"))
add("exception_failure_class", error=exc("gaierror", module="socket", mro=mro_of(
    ("gaierror", "socket"), ("OSError", "builtins"), ("Exception", "builtins"),
    ("BaseException", "builtins"), ("object", "builtins"))))
add("exception_failure_class", error=exc("URLError", module="urllib.error", mro=mro_of(
    ("URLError", "urllib.error"), ("OSError", "builtins"), ("Exception", "builtins"),
    ("BaseException", "builtins"), ("object", "builtins"))))
add("exception_failure_class", error=exc("FileNotFoundError"))
add("exception_failure_class", error=exc("InterruptedError"))
add("exception_failure_class", error=exc("ValueError"))
add("exception_failure_class", error=exc("KeyboardInterrupt"))
add("exception_failure_class", error=exc("ExceptionGroup"))
add("exception_failure_class", error=exc("ClientError", module="httpx", mro=BASE_MRO))
add("exception_failure_class", error=exc("PermErr", module="requests", mro=mro_of(
    ("PermErr", "requests"), ("PermissionError", "builtins"), ("OSError", "builtins"),
    ("Exception", "builtins"), ("BaseException", "builtins"), ("object", "builtins"))))
add("exception_failure_class", error=exc("NetOSError", module="aiohttp", mro=OSERROR_MRO))
add("exception_failure_class", error=exc("ValueError", failure_class="tagged"))
add("exception_failure_class", error=exc("H", module="probe", mro=BASE_MRO, hook=True))
add("exception_failure_class", error={"attr": 1})
add("exception_failure_class", error=None)
add("exception_failure_class", error="plainstr")

# ── extension_failure_class ──
add("extension_failure_class", kind="skill", outcome="success", failure_class="x")
add("extension_failure_class", kind="skill", outcome="failed", failure_class="not_found")
add("extension_failure_class", kind="skill", outcome="failed", failure_class="exception:ValueError")
add("extension_failure_class", kind="skill", outcome="failed", failure_class=None,
    error=exc("ValueError"))
add("extension_failure_class", kind="skill", outcome="failed", failure_class="",
    error=exc("PermissionError"))
add("extension_failure_class", kind="skill", outcome="failed", failure_class="bogus")
add("extension_failure_class", kind="bogus", outcome="failed", failure_class="x")
add("extension_failure_class", kind="plugin", outcome="failed", failure_class="deps_failed")
add("extension_failure_class", kind="mcp_server", outcome="failed", failure_class="auth_required")
add("extension_failure_class", io={"_norm": raise_("RuntimeError")},
    kind="skill", outcome="failed", failure_class="x")
add("extension_failure_class", io={}, kind="skill", outcome="failed", failure_class="x")

# ── extension_registry ──
add("extension_registry", kind="plugin", registry="skills.sh")
add("extension_registry", kind="skill", registry="skills.sh")
add("extension_registry", kind="skill", registry="SKILLS.SH ")
add("extension_registry", kind="skill", registry="github")
add("extension_registry", kind="skill", registry=None)
add("extension_registry", kind="skill", registry="bogus")
add("extension_registry", kind="skill", registry="")
add("extension_registry", io={"_norm": raise_("RuntimeError")}, kind="skill", registry="github")
add("extension_registry", io={}, kind="skill", registry="github")

# ── _long_session ──
add("_long_session", d={"turn_count_bucket": "0"})
add("_long_session", d={"turn_count_bucket": "3_to_5"})
add("_long_session", d={"turn_count_bucket": "6_to_10"})
add("_long_session", d={})
add("_long_session", d={"turn_count_bucket": None})
add("_long_session", d={"turn_count_bucket": 5})
add("_long_session", d={"turn_count_bucket": {}})
add("_long_session", d=None)

# ── milestones_for ──
add("milestones_for", metric_name=CTR.TASK_STARTED_METRIC,
    dimensions={"platform": "telegram", "entrypoint": "scheduled_task"})
add("milestones_for", metric_name=CTR.TASK_STARTED_METRIC, dimensions={"platform": "none"})
add("milestones_for", metric_name=CTR.TASK_STARTED_METRIC, dimensions={})
add("milestones_for", metric_name=CTR.TASK_FINISHED_METRIC, dimensions={"outcome": "success"})
add("milestones_for", metric_name=CTR.TOOL_USAGE_METRIC,
    dimensions={"outcome": "success", "tool_name": "mcp"})
add("milestones_for", metric_name=CTR.TOOL_USAGE_METRIC,
    dimensions={"outcome": "success", "tool_name": "delegate_task"})
add("milestones_for", metric_name=CTR.SKILL_LIFECYCLE_METRIC,
    dimensions={"action": "created", "provenance": "user"})
add("milestones_for", io={"user_created_skill": [False]},
    metric_name=CTR.SKILL_LIFECYCLE_METRIC, dimensions={"action": "created", "provenance": "user"})
add("milestones_for", metric_name=CTR.SKILL_LOAD_METRIC, dimensions={"reuse_state": "reused"})
add("milestones_for", metric_name=CTR.SESSION_METRIC, dimensions={"turn_count_bucket": "6_to_10"})
add("milestones_for", metric_name=CTR.SETUP_COMPLETED_METRIC, dimensions={})
add("milestones_for", metric_name="nope", dimensions={})
add("milestones_for", metric_name=CTR.TASK_STARTED_METRIC, dimensions={"platform": []})
add("milestones_for", io={"user_created_skill": raise_("SystemExit", "bye")},
    metric_name=CTR.SKILL_LIFECYCLE_METRIC, dimensions={})
add("milestones_for", io={"user_created_skill": raise_("KeyboardInterrupt")},
    metric_name=CTR.SKILL_LIFECYCLE_METRIC, dimensions={})
add("milestones_for", io={"user_created_skill": raise_("GeneratorExit")},
    metric_name=CTR.SKILL_LIFECYCLE_METRIC, dimensions={})
add("milestones_for", io={}, metric_name=CTR.SKILL_LIFECYCLE_METRIC, dimensions={})


# ── domain guards + write ──
def check_floats(v, path):
    if isinstance(v, float):
        assert math.isfinite(v), f"non-finite float at {path}"
        assert not v.is_integer(), f"integral float at {path} (int/float identity domain)"
        den = Fraction(v).denominator
        assert den & (den - 1) == 0, f"non-binary-exact float {v} at {path}"
        assert len(repr(v)) <= 10, f"long float repr {v} at {path}"
    elif isinstance(v, dict):
        for k, x in v.items():
            check_floats(x, path + "." + str(k))
    elif isinstance(v, list):
        for i, x in enumerate(v):
            check_floats(x, f"{path}[{i}]")


def main():
    counts = {}
    for rec in CASES:
        counts[rec["func"]] = counts.get(rec["func"], 0) + 1
        check_floats(rec, rec["func"])
    thin = {k: v for k, v in counts.items() if v < 5}
    assert not thin, f"funcs with <5 cases: {thin}"
    assert len(counts) == len(O.PORTED), f"missing funcs: {set(O.PORTED) - set(counts)}"
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for rec in CASES:
            f.write(json.dumps(rec, ensure_ascii=True) + "\n")
    print(f"GEN {O.NAME}: {len(CASES)} cases, {len(counts)} funcs -> {OUT}")
    for k in O.PORTED:
        print(f"  {k}: {counts[k]}")


if __name__ == "__main__":
    main()