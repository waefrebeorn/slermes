"""twin for tools/environments/path_utils.py:sanitize_task_id_for_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/path_utils.py"
    PY_FN = "sanitize_task_id_for_path"
    C_FN = "sanitize_task_id_for_path"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
