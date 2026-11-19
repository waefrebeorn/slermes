"""twin for cli.py:_stream_reasoning_delta (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_stream_reasoning_delta"
    C_FN = "hcli_stream_reasoning_delta"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
