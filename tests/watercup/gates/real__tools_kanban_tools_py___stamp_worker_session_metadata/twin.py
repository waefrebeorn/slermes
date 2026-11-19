"""twin for tools/kanban_tools.py:_stamp_worker_session_metadata (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/kanban_tools.py"
    PY_FN = "_stamp_worker_session_metadata"
    C_FN = "_stamp_worker_session_metadata"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
