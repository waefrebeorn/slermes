"""twin for (policy — consumer of tool_classify_error; self-contained parity gate):dispatch_with_retry (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "(policy — consumer of tool_classify_error; self-contained parity gate)"
    PY_FN = "dispatch_with_retry"
    C_FN = "dispatch_with_retry"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
