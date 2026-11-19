"""twin for agent/pet/store.py:register_local_pet (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/pet/store.py"
    PY_FN = "register_local_pet"
    C_FN = "pet_register_local_pet"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
