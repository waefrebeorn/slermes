"""twin for agent/pet/generate/imagegen.py:_forced_provider_from_env (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/pet/generate/imagegen.py"
    PY_FN = "_forced_provider_from_env"
    C_FN = "agent_pet_generate_imagegen_u_forced_provider_from_env"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
