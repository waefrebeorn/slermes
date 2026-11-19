"""twin for gateway/run.py:_seed_hygiene_system_prompt (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_seed_hygiene_system_prompt"
    C_FN = "gw_seed_hygiene_system_prompt"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
