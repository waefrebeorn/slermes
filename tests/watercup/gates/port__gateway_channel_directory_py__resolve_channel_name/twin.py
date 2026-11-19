"""twin for gateway/channel_directory.py:resolve_channel_name (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/channel_directory.py"
    PY_FN = "resolve_channel_name"
    C_FN = "resolve_channel_name"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
