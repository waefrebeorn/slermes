#!/usr/bin/env python3
"""Generate tests/fixtures/memory_manager_flow2.jsonl — >=5 cases per ported
function (14 funcs), covering decision branches, error paths, the FULL builtin
exception hierarchy (TypeError/ValueError folding, Exception swallow, and
BaseException passthrough), real composition, state mutation history on raise,
and fail-closed missing scripts.

Script semantics match the harness/oracle (FAIL-CLOSED): a MISSING script is a
RuntimeError raise spec; array = per-call queue (last repeats); single value =
constant; scripted null = None; {"__raise__":cls,"msg":...} = exception.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "memory_manager_flow2.jsonl")

CASES = []


def R(cls, msg="boom"):
    return {"__raise__": cls, "msg": msg}


def ST(bound=None, home="", ctx="primary", claim=None, shut=False):
    return {"bound_scope": bound, "home_key": home, "agent_context": ctx,
            "teardown_claim": claim, "shutting_down": shut}


def add(func, inp=None, seams=None, state=None):
    d = {"state": ST() if state is None else state}
    d.update(inp or {})
    d["seams"] = seams or {}
    CASES.append({"func": func, "in": d})


def params(**kw):
    """params mapping: name -> {"kind": ...}."""
    return {k: {"kind": v} for k, v in kw.items()}


# ── _signature_params ──
add("signature_params", {"fn": "f"}, {"signature": params(x="KEYWORD_ONLY")})
add("signature_params", {"fn": "f"}, {"signature": R("TypeError")})
add("signature_params", {"fn": "f"}, {"signature": R("ValueError")})
add("signature_params", {"fn": "f"}, {"signature": R("UnicodeDecodeError")})  # ValueError family
add("signature_params", {"fn": "f"}, {"signature": R("RuntimeError")})        # propagate
add("signature_params", {"fn": "f"}, {"signature": R("SystemExit", "bye")})   # BaseException
add("signature_params", {"fn": "f"})                                          # fail-closed

# ── _has_var_kwargs ──
add("has_var_kwargs", {"params": {}})
add("has_var_kwargs", {"params": params(x="KEYWORD_ONLY", y="POSITIONAL_ONLY")})
add("has_var_kwargs", {"params": params(x="POSITIONAL_ONLY", y="VAR_KEYWORD")})
add("has_var_kwargs", {"params": params(x="VAR_POSITIONAL", y="KEYWORD_ONLY", z="VAR_KEYWORD")})
add("has_var_kwargs", {"params": params()})
add("has_var_kwargs", {"params": params(x="POSITIONAL_ONLY")})

# ── _accepts_require_checkpoint ──
add("accepts_require_checkpoint", {"fn": "f"},
    {"signature": params(require_checkpoint="KEYWORD_ONLY")})
add("accepts_require_checkpoint", {"fn": "f"},
    {"signature": params(require_checkpoint="POSITIONAL_OR_KEYWORD")})
add("accepts_require_checkpoint", {"fn": "f"},
    {"signature": params(require_checkpoint="POSITIONAL_ONLY")})
add("accepts_require_checkpoint", {"fn": "f"},
    {"signature": params(messages="POSITIONAL_ONLY", kw="VAR_KEYWORD")})
add("accepts_require_checkpoint", {"fn": "f"}, {"signature": R("TypeError")})
add("accepts_require_checkpoint", {"fn": "f"}, {"signature": R("RuntimeError")})
add("accepts_require_checkpoint", {"fn": "f"})  # fail-closed

# ── _scrub ──
add("scrub", {"text": "hello world", "urls": True}, {"redact_for_egress": "redacted"})
add("scrub", {"text": "hello world", "urls": False}, {"redact_for_egress": "redacted-no-url"})
add("scrub", {"text": "abc"}, {"redact_for_egress": None})
add("scrub", {"text": "abc"}, {"redact_for_egress": R("RuntimeError")})
add("scrub", {"text": "abc"}, {"redact_for_egress": R("SystemExit", "bye")})
add("scrub", {"text": "abc"})  # fail-closed

# ── _owner_allows ──
add("owner_allows", {"providers": []})                                    # no seam
add("owner_allows", {"providers": None})                                  # no seam
add("owner_allows", {"providers": [{"name": "builtin"}]}, {"caller_scope": {"home_key": "h"}})
add("owner_allows", {"providers": [{"name": "builtin"}]}, {"caller_scope": None})
add("owner_allows", {"providers": [{"name": "x"}], "hook": "sync_turn"},
    {"caller_scope": R("RuntimeError")})
add("owner_allows", {"providers": [{"name": "x"}], "hook": "sync_turn"})   # fail-closed

# ── _fork_turn_hooks_suppressed ──
for ctx in ("subagent", "primary", "", "SUBAGENT", None):
    add("fork_turn_hooks_suppressed", state=ST(ctx=ctx))
add("fork_turn_hooks_suppressed", state=ST(ctx="subagent"))

# ── _provider_sync_accepts ──
add("provider_sync_accepts", {"sync_fn": "st", "keyword": "messages"},
    {"signature": params(messages="KEYWORD_ONLY")})
add("provider_sync_accepts", {"sync_fn": "st", "keyword": "messages"},
    {"signature": params(other="KEYWORD_ONLY")})
add("provider_sync_accepts", {"sync_fn": "st", "keyword": "messages"},
    {"signature": params(kw="VAR_KEYWORD")})
add("provider_sync_accepts", {"sync_fn": "st", "keyword": "messages"},
    {"signature": R("TypeError")})                                        # assume yes
add("provider_sync_accepts", {"sync_fn": "st", "keyword": "messages"},
    {"signature": R("RuntimeError")})                                     # propagate
add("provider_sync_accepts", {"sync_fn": "st", "keyword": "messages"})     # fail-closed

# ── _registry_key ──
add("registry_key", {"provider_name": "p", "home_key": "h"}, state=ST(home="h"))
add("registry_key", {"provider_name": "p", "home_key": "h"},
    state=ST(bound={"home_key": "b"}, home="h"))
add("registry_key", {"provider_name": "p", "home_key": "h"},
    state=ST(bound={"home_key": None}, home="h"))
add("registry_key", {"provider_name": "p", "home_key": "h"},
    state=ST(bound={"home_key": ""}, home="h"))
add("registry_key", {"provider_name": "p", "home_key": "h"}, state=ST(bound={}, home="h"))
add("registry_key", {"provider_name": "p", "home_key": ""},
    state=ST(bound={"home_key": "b"}, home=""))

# ── _checkpoint_api_version ──
add("checkpoint_api_version", {"provider": {"name": "p"}})                 # absent -> 1
add("checkpoint_api_version",
    {"provider": {"name": "p", "pre_compress_checkpoint_api_version": None}})   # int(None) -> None
add("checkpoint_api_version",
    {"provider": {"name": "p", "pre_compress_checkpoint_api_version": 3}})
add("checkpoint_api_version",
    {"provider": {"name": "p", "pre_compress_checkpoint_api_version": "2"}})
add("checkpoint_api_version",
    {"provider": {"name": "p", "pre_compress_checkpoint_api_version": "abc"}})
add("checkpoint_api_version",
    {"provider": {"name": "p", "pre_compress_checkpoint_api_version": "2.5"}})

# ── supports_pre_compress_checkpoint ──
add("supports_pre_compress_checkpoint", {"providers": [], "api_version": 2})
add("supports_pre_compress_checkpoint",
    {"providers": [{"name": "a", "pre_compress_checkpoint_api_version": 3}], "api_version": 2})
add("supports_pre_compress_checkpoint",
    {"providers": [{"name": "a", "pre_compress_checkpoint_api_version": 1}], "api_version": 2})
add("supports_pre_compress_checkpoint",
    {"providers": [{"name": "a", "pre_compress_checkpoint_api_version": 1},
                   {"name": "b", "pre_compress_checkpoint_api_version": None}], "api_version": 2})
add("supports_pre_compress_checkpoint",
    {"providers": [{"name": "a", "pre_compress_checkpoint_api_version": 1},
                   {"name": "b", "pre_compress_checkpoint_api_version": 2}], "api_version": 2})
add("supports_pre_compress_checkpoint",
    {"providers": [{"name": "a", "pre_compress_checkpoint_api_version": "x"}], "api_version": 2})

# ── _each_provider ──
P1 = {"name": "builtin"}
P2 = {"name": "ext"}
add("each_provider", {"providers": [], "call_name": "cp"})
add("each_provider", {"providers": [P1], "call_name": "cp"}, {"cp": "ok"})
add("each_provider", {"providers": [P1, P2], "call_name": "cp"},
    {"cp": ["ok", R("ValueError")], "logger.log": None})
add("each_provider", {"providers": [P1], "call_name": "cp", "exc_info": True},
    {"cp": R("RuntimeError", "bad"), "logger.log": None})
add("each_provider", {"providers": [P1], "call_name": "cp"}, {"cp": None})
add("each_provider", {"providers": [P1], "call_name": "cp"},
    {"logger.log": None})                                                  # fail-closed swallowed
add("each_provider", {"providers": [P1], "call_name": "cp"}, {"cp": R("SystemExit", "bye")})
add("each_provider", {"providers": [P1], "call_name": "cp", "level": 30, "label": "sync"},
    {"cp": R("RuntimeError", "k"), "logger.log": None})

# ── _call_unless_stalled ──
add("call_unless_stalled", {"provider": P1, "call_name": "sc"}, {"sc": "discarded"})
add("call_unless_stalled", {"provider": P1, "call_name": "sc"}, {"sc": R("RuntimeError", "boom")})
add("call_unless_stalled", {"provider": P2, "call_name": "sc", "home_key": "h"},
    {"gate.enter": None, "sc": None, "gate.leave": None}, state=ST(home="h"))
add("call_unless_stalled", {"provider": P2, "call_name": "sc", "home_key": "h"},
    {"gate.enter": [[2, 5, 1]], "logger.warning": None}, state=ST(home="h"))
add("call_unless_stalled", {"provider": P2, "call_name": "sc", "home_key": "h"},
    {"gate.enter": [[2, 5, 0]]}, state=ST(home="h"))
add("call_unless_stalled", {"provider": P2, "call_name": "sc", "home_key": "h"},
    {"gate.enter": [[2, 5, 100]], "logger.warning": None}, state=ST(home="h"))
add("call_unless_stalled", {"provider": P2, "call_name": "sc", "home_key": "h"},
    {"gate.enter": R("RuntimeError")}, state=ST(home="h"))
add("call_unless_stalled", {"provider": P2, "call_name": "sc", "home_key": "h"},
    {"gate.enter": None, "sc": R("RuntimeError", "bad"), "gate.leave": None}, state=ST(home="h"))
add("call_unless_stalled", {"provider": P2, "call_name": "sc", "home_key": "h"},
    {"gate.enter": None, "sc": R("RuntimeError", "bad"),
     "gate.leave": R("SystemExit", "bye")}, state=ST(home="h"))

# ── claim_teardown ──
add("claim_teardown", {"session_end": True}, state=ST(claim=None, shut=False))
add("claim_teardown", {"session_end": False}, state=ST(claim=None, shut=False))
add("claim_teardown", {"session_end": False}, state=ST(claim=None, shut=True))
add("claim_teardown", {"session_end": True}, state=ST(claim="soft_release", shut=False))
add("claim_teardown", {"session_end": False}, state=ST(claim="soft_release", shut=False))
add("claim_teardown", {"session_end": False}, state=ST(claim="session_end", shut=False))

# ── release_soft ──
add("release_soft", seams={"flush_pending": True, "shutdown_all": None},
    state=ST(claim=None, shut=False))
add("release_soft", seams={"flush_pending": True, "shutdown_all": None},
    state=ST(claim="session_end", shut=False))
add("release_soft", seams={"flush_pending": True, "shutdown_all": None},
    state=ST(claim=None, shut=True))
add("release_soft", seams={"flush_pending": R("RuntimeError", "flush")},
    state=ST(claim=None, shut=False))
add("release_soft", seams={"flush_pending": True, "shutdown_all": R("RuntimeError", "down")},
    state=ST(claim=None, shut=False))
add("release_soft", seams={}, state=ST(claim=None, shut=False))  # fail-closed at flush_pending


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    print("wrote %s: %d cases, %d funcs" % (OUT, len(CASES), len(counts)))
    if short:
        raise SystemExit("FUNCS WITH <5 CASES: %s" % short)
    print("per-func min count: %d" % min(counts.values()))


if __name__ == "__main__":
    main()
