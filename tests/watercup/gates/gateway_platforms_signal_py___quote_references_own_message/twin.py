"""twin for gateway/platforms/signal.py:_quote_references_own_message (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/signal.py"
    PY_FN = "_quote_references_own_message"
    C_FN = "sig_quote_references_own_message"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
