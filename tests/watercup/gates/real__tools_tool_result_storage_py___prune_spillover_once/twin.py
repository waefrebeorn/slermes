"""twin for tools/tool_result_storage.py:_prune_spillover_once (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tool_result_storage.py"
    PY_FN = "_prune_spillover_once"
    C_FN = "_prune_spillover_once"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
