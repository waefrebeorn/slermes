"""twin for cc_should_compress_guards.

Traces the LIVE agent/context_compressor.py::should_compress_info +
_compression_block_reason with the SAME rung order as the C harness:
  B1 below-threshold / cooldown / ineffective / fire.
The block-reason rungs are evaluated by the LIVE _compression_block_reason
method on an in-memory compressor snapshot (pure — no durable DB refresh).
"""
import os, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup
import importlib.util as ilu

spec = ilu.spec_from_file_location(
    "wc_live_ctxcomp", os.path.join(DEV, "agent", "context_compressor.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_ctxcomp"] = mod
spec.loader.exec_module(mod)

ContextCompressor = mod.ContextCompressor


def _make(threshold, cooldown_remaining, ineff, streak):
    comp = object.__new__(ContextCompressor)
    comp.threshold_tokens = threshold
    comp.last_prompt_tokens = 0
    comp._summary_failure_cooldown_until = (
        time.monotonic() + cooldown_remaining if cooldown_remaining > 0 else 0.0)
    comp._ineffective_compression_count = ineff
    comp._fallback_compression_streak = streak
    comp._anti_thrash_recovery_deadline = 0.0
    comp._structural_no_op_backoff_until = 0.0
    comp.quiet_mode = True
    return comp


def run(inputs):
    cups = []
    for probe in inputs:
        parts = probe.split("|")
        tokens = int(parts[0])
        threshold = int(parts[1])
        cooldown_remaining = float(parts[2])
        ineff = int(parts[3])
        streak = int(parts[4])
        comp = _make(threshold, cooldown_remaining, ineff, streak)

        c = Cup("cc_should_compress_guards")
        # LIVE outer gate: tokens < threshold -> (False, None)
        reason_cls = None
        if tokens < threshold:
            c.branch(True)
            c.valstr("no-compress")
            assert comp.should_compress(tokens) is False
        else:
            c.branch(False)
            # LIVE block-reason ladder via the real method
            live_reason = ContextCompressor._compression_block_reason(comp)
            if live_reason is not None:
                c.branch(True)
                reason_cls = "cooldown" if live_reason.startswith("cooldown") \
                    else "ineffective"
                c.valstr(reason_cls)
            else:
                c.branch(False)
                c.valstr("fire")
            assert comp.should_compress(tokens) == (live_reason is None), \
                "twin drift"
        cups.append(c.finalize())
    return cups
