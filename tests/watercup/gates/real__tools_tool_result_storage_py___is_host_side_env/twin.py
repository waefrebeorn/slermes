"""twin for tools/tool_result_storage.py:_is_host_side_env (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tool_result_storage.py"
    PY_FN = "_is_host_side_env"
    C_FN = "_is_host_side_env"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
