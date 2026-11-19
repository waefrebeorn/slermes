"""twin for gateway/relay/adapter.py:supports_draft_streaming (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/relay/adapter.py"
    PY_FN = "supports_draft_streaming"
    C_FN = "relay_adapter_supports_draft_streaming"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
