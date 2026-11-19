"""twin for agent/context_compressor.py:_is_hygiene_preagent_only_cooldown (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_compressor.py"
    PY_FN = "_is_hygiene_preagent_only_cooldown"
    C_FN = "_is_hygiene_preagent_only_cooldown"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
