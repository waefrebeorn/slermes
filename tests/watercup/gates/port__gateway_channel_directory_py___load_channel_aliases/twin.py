"""twin for gateway/channel_directory.py:_load_channel_aliases (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase
import json as _j



class _Twin(TwinBase):
    PY_MODULE = "gateway/channel_directory.py"
    PY_FN = "_load_channel_aliases"
    C_FN = "load_channel_aliases"

    def norm(self, c, result):
        c.valstr(_j.dumps(result, sort_keys=True, separators=(",", ":")) if result is not None else "null")


_t = _Twin()
trace = _t.trace
run = _t.run
