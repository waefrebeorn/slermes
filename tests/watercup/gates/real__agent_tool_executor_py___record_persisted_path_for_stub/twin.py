"""twin for agent/tool_executor.py:_record_persisted_path_for_stub (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/tool_executor.py"
    PY_FN = "_record_persisted_path_for_stub"
    C_FN = "_record_persisted_path_for_stub"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
