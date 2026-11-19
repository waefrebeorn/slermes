"""twin for agent/pet/generate/orchestrate.py:hatch_pet (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/pet/generate/orchestrate.py"
    PY_FN = "hatch_pet"
    C_FN = "agent_pet_generate_orchestrate_hatch_pet"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
