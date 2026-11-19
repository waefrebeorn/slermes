"""twin for agent/pet/constants.py:state_aliases_for (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/pet/constants.py"
    PY_FN = "state_aliases_for"
    C_FN = "pet_state_from_string"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
