"""twin for gateway/relay/adapter.py:_note_prompt_resolved (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/relay/adapter.py"
    PY_FN = "_note_prompt_resolved"
    C_FN = "relay_adapter_note_prompt_resolved"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
