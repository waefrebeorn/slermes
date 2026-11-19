"""twin for gateway/relay/adapter.py:_inbound_dedupe_key (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/relay/adapter.py"
    PY_FN = "_inbound_dedupe_key"
    C_FN = "_inbound_dedupe_key"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
