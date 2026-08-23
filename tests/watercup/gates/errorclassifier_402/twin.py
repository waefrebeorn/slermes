"""twin for errorclassifier_402.

Traces the LIVE agent/error_classifier.py::_classify_402 branch ladder with
the SAME rung order as the C harness:
  B1 usage-limit pattern / B2 transient signal; VSTR(outcome reason).
The LIVE fn is called with a capturing result_fn and its FailoverReason is
asserted against the mirrored outcome.
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup

_spec = ilu.spec_from_file_location("wc_live_error_classifier",
                                    os.path.join(DEV, "agent", "error_classifier.py"))
_live = ilu.module_from_spec(_spec)
sys.modules["wc_live_error_classifier"] = _live
_spec.loader.exec_module(_live)


def trace(error_msg):
    c = Cup("errorclassifier_402")

    has_usage = any(p in error_msg for p in _live._USAGE_LIMIT_PATTERNS)
    c.branch(has_usage)                              # B1
    has_transient = any(p in error_msg for p in _live._USAGE_LIMIT_TRANSIENT_SIGNALS)
    c.branch(has_transient)                          # B2

    outcome = ("rate_limit" if (has_usage and has_transient) else "billing")
    c.valstr(outcome)

    captured = {}
    def result_fn(reason, **overrides):
        captured["reason"] = reason
        return reason

    _live._classify_402(error_msg, result_fn)
    live_reason = captured["reason"].value if hasattr(captured.get("reason"), "value") \
        else str(captured.get("reason"))
    assert live_reason == outcome, (live_reason, outcome, error_msg)
    return c.finalize()


def run(inputs):
    return [trace(msg.lower()) for msg in inputs]
