"""twin for tools/file_tools.py:_get_container_mirror_prefix_for_task (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/file_tools.py"
    PY_FN = "_get_container_mirror_prefix_for_task"
    C_FN = "file_tools_get_container_mirror_prefix_for_task"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
