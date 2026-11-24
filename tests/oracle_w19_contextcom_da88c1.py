#!/usr/bin/env python3
"""Differential oracle for agent/context_compressor_telemetry.py (gate w19_contextcom_da88c1).

Extracts the live module function and the CompressionTelemetryMixin methods from the Python
AST and drives them on a fake compressor. The message-token estimator is the seam: the
fixture's `rough` list is what estimate_messages_tokens_rough answers, in call order, so both
sides are filled with identical numbers.
"""
import ast
import json
import sys
import types
import uuid
import __future__
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "agent" / "context_compressor_telemetry.py"
GATE = "w19_contextcom_da88c1"
_ANN = __future__.annotations.compiler_flag


def stub_module(dotted, **attrs):
    parts = dotted.split(".")
    for i in range(1, len(parts) + 1):
        dn = ".".join(parts[:i])
        if dn not in sys.modules or sys.modules[dn] is None:
            m = types.ModuleType(dn)
            m.__path__ = []
            sys.modules[dn] = m
    m = sys.modules[dotted]
    for k, v in attrs.items():
        setattr(m, k, v)
    return m


G = {"__name__": "tel_live", "uuid": uuid, "Any": object, "isinstance": isinstance,
     "getattr": getattr, "len": len, "dict": dict, "str": str}


def _exec(path, names, into):
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in names:
            node.decorator_list = []
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec",
                         flags=_ANN, dont_inherit=True), into)


_exec(SRC, {"_safe_int", "_begin_compression_telemetry", "_record_compression_regions",
            "_record_aux_compression_call", "_note_aux_route_fallback",
            "_compression_method", "_record_compression_effect",
            "_record_committed_compression_effect"}, G)


class Fake:
    def __init__(self, r):
        self.provider = r.get("provider")
        self.model = r.get("model")
        self.context_length = r.get("context_length")
        self.threshold_tokens = r.get("threshold_tokens")
        self._compression_telemetry_seed = r.get("seed")
        self._active_compression_telemetry = r.get("telemetry")
        self._last_summary_fallback_used = bool(r.get("fallback_used"))
        self._last_aux_model_failure_error = r.get("failure_error_prev")
        self._last_summary_dropped_count = r.get("last_dropped_count")
        self._last_aux_model_failure_model = None


Fake._begin_compression_telemetry = G["_begin_compression_telemetry"]
Fake._record_compression_regions = G["_record_compression_regions"]
Fake._record_aux_compression_call = G["_record_aux_compression_call"]
Fake._note_aux_route_fallback = G["_note_aux_route_fallback"]
Fake._compression_method = G["_compression_method"]
Fake._record_compression_effect = G["_record_compression_effect"]
Fake._record_committed_compression_effect = G["_record_committed_compression_effect"]


def run_case(r):
    fake = Fake(r)
    rough = list(r.get("rough") or [])
    state = {"i": 0}

    def estimate(_msgs):
        i = state["i"]
        state["i"] = i + 1
        return rough[i] if i < len(rough) else (rough[-1] if rough else 0)

    G["estimate_messages_tokens_rough"] = estimate
    G["forget_message_token_memo"] = lambda _msgs: None
    stub_module("agent.model_metadata", estimate_messages_tokens_rough=estimate)
    stub_module("agent.model_metadata_token_memo", forget_message_token_memo=lambda _m: None)

    f = r.get("func")
    if f == "safe_int":
        return G["_safe_int"](r.get("value"))
    if f == "compression_method":
        return fake._compression_method()
    if f == "note_aux_fallback":
        fake._note_aux_route_fallback(r.get("route") or {})
        tel = fake._active_compression_telemetry
        return {"telemetry": tel,
                "failure_model": getattr(fake, "_last_aux_model_failure_model", None),
                "failure_error": getattr(fake, "_last_aux_model_failure_error", None)}
    if f == "record_aux_call":
        fake._record_aux_compression_call(
            prompt_messages=[{}] * int(r.get("prompt_len") or 0),
            max_tokens=r.get("max_tokens"),
            duration_ms=r.get("duration_ms"),
            aux_provider=r.get("aux_provider"),
            aux_model=r.get("aux_model"),
            effective_aux_context=r.get("effective_aux_context"),
            phase_timings=r.get("phase_timings"),
        )
        return fake._active_compression_telemetry
    if f == "record_effect":
        fake._record_compression_effect(
            r.get("messages_before"),
            [{}] * int(r.get("compressed_len") or 0),
            r.get("tokens_before"),
            r.get("tool_results_pruned"),
            r.get("reasoning_items_pruned"),
        )
        return fake._active_compression_telemetry
    if f == "record_committed_effect":
        fake._record_committed_compression_effect(
            [{}] * int(r.get("before_len") or 0), [{}] * int(r.get("after_len") or 0))
        return fake._active_compression_telemetry
    if f == "record_regions":
        fake._record_compression_regions(
            head_messages=[{}] * int(r.get("head_len") or 0),
            middle_messages=[{}] * int(r.get("middle_len") or 0),
            tail_messages=[{}] * int(r.get("tail_len") or 0),
        )
        return fake._active_compression_telemetry
    if f == "begin_telemetry":
        return fake._begin_compression_telemetry(
            current_tokens=r.get("current_tokens"),
            attempt_id=r.get("attempt_id"),
            session_id=r.get("session_id"),
            trigger_source=r.get("trigger_source"),
        )
    return None


def main():
    cases = mism = 0
    wanted = {"safe_int", "compression_method", "note_aux_fallback", "record_aux_call",
              "record_effect", "record_committed_effect", "record_regions", "begin_telemetry"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        f = r.get("func")
        if f not in wanted:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s args=%r py=raised %r c=%r" % (f, r, exc, r.get("out")),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s args=%r py=%r c=%r" % (f, r, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
