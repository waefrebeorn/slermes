"""twin for hermes_cli/voice.py:start_recording (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/voice.py"
    PY_FN = "start_recording"
    C_FN = "start_recording"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
