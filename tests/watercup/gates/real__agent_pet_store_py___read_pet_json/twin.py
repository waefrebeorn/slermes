"""twin for agent/pet/store.py:_read_pet_json (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/pet/store.py"
    PY_FN = "_read_pet_json"
    C_FN = "_read_pet_json"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
