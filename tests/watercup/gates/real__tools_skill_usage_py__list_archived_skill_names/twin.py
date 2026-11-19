"""twin for tools/skill_usage.py:list_archived_skill_names (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase
import json as _j



class _Twin(TwinBase):
    PY_MODULE = "tools/skill_usage.py"
    PY_FN = "list_archived_skill_names"
    C_FN = "su_list_archived_names"

    def norm(self, c, result):
        c.valstr(_j.dumps(result, sort_keys=True, separators=(",", ":")) if result is not None else "null")


_t = _Twin()
trace = _t.trace
run = _t.run
