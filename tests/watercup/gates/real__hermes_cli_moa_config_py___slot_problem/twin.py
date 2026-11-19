"""twin for hermes_cli/moa_config.py:_slot_problem (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/moa_config.py"
    PY_FN = "_slot_problem"
    C_FN = "_slot_problem"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
