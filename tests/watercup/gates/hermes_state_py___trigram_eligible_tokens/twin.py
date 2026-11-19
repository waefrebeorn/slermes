"""twin for hermes_state.py:_trigram_eligible_tokens (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_trigram_eligible_tokens"
    C_FN = "hermes_state_trigram_eligible_tokens"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
