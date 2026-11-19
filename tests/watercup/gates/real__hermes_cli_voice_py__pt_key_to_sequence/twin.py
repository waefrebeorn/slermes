"""twin for hermes_cli/voice.py:pt_key_to_sequence (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/voice.py"
    PY_FN = "pt_key_to_sequence"
    C_FN = "pt_key_to_sequence"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
