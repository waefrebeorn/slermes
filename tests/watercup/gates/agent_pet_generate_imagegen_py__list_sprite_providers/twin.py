"""twin for agent/pet/generate/imagegen.py:list_sprite_providers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/pet/generate/imagegen.py"
    PY_FN = "list_sprite_providers"
    C_FN = "agent_pet_generate_imagegen_list_sprite_providers"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
