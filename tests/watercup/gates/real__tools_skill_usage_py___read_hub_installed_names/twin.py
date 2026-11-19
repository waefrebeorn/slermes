"""twin for tools/skill_usage.py:_read_hub_installed_names (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase
import json as _j



class _Twin(TwinBase):
    PY_MODULE = "tools/skill_usage.py"
    PY_FN = "_read_hub_installed_names"
    C_FN = "su_read_hub_installed_names"

    def norm(self, c, result):
        c.valstr(_j.dumps(result, sort_keys=True, separators=(",", ":")) if result is not None else "null")


_t = _Twin()
trace = _t.trace
run = _t.run
