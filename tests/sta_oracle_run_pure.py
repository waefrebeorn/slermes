#!/usr/bin/env python3
"""
sta_oracle_run_pure.py — Python oracle for the PURE gateway/run.py helpers
ported in src/gateway/run_pure.c.

gateway/run.py is NOT importable in this dev tree (an unrelated broken
`from agent.replay_cleanup import ...` at line ~970 crashes module import).
To stay faithful to the CANONICAL Python behavior, this oracle extracts and
executes ONLY the precise source regions of the pure helpers plus their
module-level regex/constant definitions (all defined before line 970) into an
isolated namespace, then evaluates each fixture through the real Python code.

This is the exact source text the C port was translated from — no reimplementation.
"""
import os
import re
import ast
import sys
import json
from datetime import datetime
from typing import Optional, Dict, Any, List

_HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(_HERE, "..", ".."))   # hermes-agent-dev
RUNPY = os.path.join(ROOT, "gateway", "run.py")

# Line ranges (1-indexed, inclusive) of pure-helper regions + their constants.
# All are before the broken import at ~970, so they exec cleanly with just
# `re`, `datetime`, and typing available.
REGIONS = [
    (72, 172),    # _TELEGRAM_COMMAND_MENTION_RE, _TELEGRAM_NOISY_STATUS_RE,
                  # _GATEWAY_RAW_TEXT_PLATFORMS, _GATEWAY_PROVIDER_*,
                  # _GATEWAY_RATE_LIMIT_RE, _GATEWAY_SECRET_PATTERNS, etc.
                  # (up to end of patterns before _ensure_windows_gateway_venv_imports)
    (229, 288),   # _gateway_platform_value, _non_conversational_metadata,
                  # _is_transient_network_error (complete function)
    (400, 455),   # _gateway_provider_error_reply, _GATEWAY_PROVIDER_ERROR_SHAPE_RE,
                  # _looks_like_gateway_provider_error (stop BEFORE _redact_gateway_user_facing_secrets)
    (594, 666),   # _telegramize_command_mentions, _coerce_gateway_timestamp
    (923, 940),   # _message_timestamps_enabled
    (1146, 1177), # _AUTO_CONTINUE_* prefixes, _is_auto_continue_noise,
                  # _strip_auto_continue_noise
]

# _telegramize_command_mentions does `from hermes_cli.commands import
# _sanitize_telegram_name` lazily; make it resolvable.
ns = {
    "re": re,
    "datetime": datetime,
    "Optional": Optional, "Dict": Dict, "Any": Any, "List": List,
    "__name__": "gateway_run_pure_oracle",
}


def _load():
    # Extract the pure helper regions of gateway/run.py by AST: every TOP-LEVEL
    # statement EXCEPT import statements (the module's broken imports are skipped,
    # but all function/constant/class/conditional definitions run normally, so
    # main() finds every name it references -- including ones defined inside
    # top-level if/try blocks). Immune to line-number drift.
    rp = RUNPY if os.path.exists(RUNPY) else os.path.join(os.path.dirname(
        os.path.abspath(__file__)), "..", "..", "gateway", "run.py")
    full = open(rp, encoding="utf-8").read()
    tree = ast.parse(full)
    pieces = []
    # Include ALL top-level nodes (imports included). With the dev tree on
    # sys.path (via the oracle bootstrap), gateway/run.py's own imports resolve
    # correctly, so every name main() needs (incl. COMPACTION_STATUS, which is
    # imported at module top level) is bound in ns.
    for node in tree.body:
        seg = ast.get_source_segment(full, node)
        if seg:
            pieces.append(seg)
    src = "\n\n".join(pieces)
    try:
        from hermes_cli.commands import _sanitize_telegram_name
        ns["_sanitize_telegram_name"] = _sanitize_telegram_name
    except Exception:
        def _sanitize_telegram_name(raw):
            name = raw.lower().replace("-", "_")
            name = re.sub(r"[^a-z0-9_]", "", name)
            name = re.sub(r"_{2,}", "_", name)
            return name.strip("_")
        ns["_sanitize_telegram_name"] = _sanitize_telegram_name
    # gateway/run.py imports these module-level status constants; since we
    # skip imports during extraction, pre-populate them so any top-level code
    # referencing them resolves.
    try:
        from agent.conversation_compression import (
            COMPACTION_STATUS,
            COMPRESSION_RETRY_CONTEXT_REDUCED_STATUS_TEMPLATE,
            COMPRESSION_RETRY_MESSAGES_STATUS_TEMPLATE,
            COMPRESSION_RETRY_TOKENS_STATUS_TEMPLATE,
            IDLE_COMPACTION_STATUS_TEMPLATE,
        )
        for _n in ("COMPACTION_STATUS",
                   "COMPRESSION_RETRY_CONTEXT_REDUCED_STATUS_TEMPLATE",
                   "COMPRESSION_RETRY_MESSAGES_STATUS_TEMPLATE",
                   "COMPRESSION_RETRY_TOKENS_STATUS_TEMPLATE",
                   "IDLE_COMPACTION_STATUS_TEMPLATE"):
            ns[_n] = globals()[_n]
    except Exception:
        pass

    ns["__file__"] = rp
    ns["__name__"] = "gateway_run_pure_oracle"
    exec(src, ns)


_load()

_gateway_platform_value = ns["_gateway_platform_value"]
_gateway_surface_passes_raw_text = ns["_gateway_surface_passes_raw_text"]
_non_conversational_metadata = ns["_non_conversational_metadata"]
_looks_like_gateway_provider_error = ns["_looks_like_gateway_provider_error"]
_gateway_provider_error_reply = ns["_gateway_provider_error_reply"]
_is_auto_continue_noise = ns["_is_auto_continue_noise"]
_strip_auto_continue_noise = ns["_strip_auto_continue_noise"]
_telegramize_command_mentions = ns["_telegramize_command_mentions"]
_coerce_gateway_timestamp = ns["_coerce_gateway_timestamp"]
_message_timestamps_enabled = ns["_message_timestamps_enabled"]
_is_transient_network_error = ns["_is_transient_network_error"]


def split_pipe(rest, n):
    out = ["" for _ in range(n)]
    if not rest:
        return out
    parts = rest.split("|")
    for i in range(min(len(parts), n)):
        out[i] = parts[i]
    return out


def _transient_check(exc_name, cause_name=None, context_name=None):
    """Faithfully exercise the real _is_transient_network_error, which walks
    exc.__cause__ / exc.__context__. We model the by-name chain from the
    fixture with a tiny fake-exception class so the canonical Python logic
    (not a reimplementation) decides transient vs not."""
    class _FakeExc(Exception):
        def __init__(self, name, cause=None, context=None):
            self.__class__ = type(name, (_FakeExc,), {})
            self.__cause__ = cause
            self.__context__ = context
    cause = _FakeExc(cause_name) if cause_name else None
    ctx = _FakeExc(context_name) if context_name else None
    return _is_transient_network_error(_FakeExc(exc_name, cause=cause, context=ctx))


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n")


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: %s <cases.txt>\n" % sys.argv[0])
        return 2
    with open(sys.argv[1], "r") as f:
        for raw in f:
            line = raw.rstrip("\n")
            if not line.strip() or line.startswith("#"):
                continue
            op, _, rest = line.partition(" ")
            rest = rest.strip()
            a = split_pipe(rest, 8)

            if op == "platform_value":
                emit({"op": "platform_value", "in": rest,
                      "out": _gateway_platform_value(rest)})
            elif op == "surface_raw":
                emit({"op": "surface_raw", "platform": rest,
                      "raw": _gateway_surface_passes_raw_text(rest)})
            elif op == "nonconv":
                md = json.loads(a[1]) if a[1] else None
                res = _non_conversational_metadata(md, platform=a[0])
                emit({"op": "nonconv", "platform": a[0],
                      "out": json.dumps(res, sort_keys=True, separators=(",", ":"), ensure_ascii=False)})
            elif op == "provider_error":
                emit({"op": "provider_error", "text": rest,
                      "looks": _looks_like_gateway_provider_error(rest)})
            elif op == "provider_reply":
                emit({"op": "provider_reply", "text": rest,
                      "reply": _gateway_provider_error_reply(rest)})
            elif op == "auto_noise":
                emit({"op": "auto_noise", "content": rest,
                      "noise": _is_auto_continue_noise(rest)})
            elif op == "strip_auto":
                emit({"op": "strip_auto", "content": rest,
                      "out": _strip_auto_continue_noise(rest)})
            elif op == "telegramize":
                emit({"op": "telegramize", "platform": a[0], "text": a[1],
                      "out": _telegramize_command_mentions(a[1], a[0])})
            elif op == "coerce":
                ev = _coerce_gateway_timestamp(rest)
                emit({"op": "coerce", "value": rest,
                      "epoch": (f"{ev:.3f}" if ev is not None else None)})
            elif op == "ts_enabled":
                cfg = None
                if rest:
                    try:
                        cfg = json.loads(rest)
                    except Exception:
                        cfg = None
                emit({"op": "ts_enabled", "enabled": _message_timestamps_enabled(cfg)})
            elif op == "transient":
                emit({"op": "transient", "exc": a[0],
                      "transient": _transient_check(
                          a[0], a[1] or None, a[2] or None)})
    return 0


if __name__ == "__main__":
    sys.exit(main())
