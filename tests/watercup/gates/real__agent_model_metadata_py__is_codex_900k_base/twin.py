"""twin for agent/model_metadata.py:is_codex_900k_base (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/model_metadata.py"
    PY_FN = "is_codex_900k_base"
    C_FN = "is_codex_900k_base"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
