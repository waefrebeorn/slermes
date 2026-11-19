"""twin for hermes_state.py:_session_lineage_root_to_tip (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_session_lineage_root_to_tip"
    C_FN = "_session_lineage_root_to_tip"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
