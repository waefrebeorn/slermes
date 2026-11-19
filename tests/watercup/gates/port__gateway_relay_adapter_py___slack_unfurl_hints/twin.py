"""twin for gateway/relay/adapter.py:_slack_unfurl_hints (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/relay/adapter.py"
    PY_FN = "_slack_unfurl_hints"
    C_FN = "_slack_unfurl_hints"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
