"""twin for tools/computer_use/tool.py:_canon_key_combo (frozenset -> sorted array)."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/tool.py"
    PY_FN = "_canon_key_combo"
    C_FN = "tools_computer_use_tool_u_canon_key_combo"
    def norm(self, c, result):
        c.valstr(_j.dumps(sorted(result), separators=(",", ":")))
_t = _Twin(); trace = _t.trace; run = _t.run
