"""twin for gateway/channel_directory.py:_normalize_adapter_channels (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/channel_directory.py"
    PY_FN = "_normalize_adapter_channels"
    C_FN = "_normalize_adapter_channels"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
