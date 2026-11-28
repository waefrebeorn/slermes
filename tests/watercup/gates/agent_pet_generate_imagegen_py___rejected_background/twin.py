"""twin for agent/pet/generate/imagegen.py:_rejected_background."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/pet/generate/imagegen.py"
    PY_FN = "_rejected_background"
    C_FN = "agent_pet_generate_imagegen_u_rejected_background"
    def norm(self, c, result):
        c.valstr("1" if result else "0")
_t = _Twin(); trace = _t.trace; run = _t.run
