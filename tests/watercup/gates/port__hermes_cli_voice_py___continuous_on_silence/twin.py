"""twin for hermes_cli/voice.py:_continuous_on_silence (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/voice.py"
    PY_FN = "_continuous_on_silence"
    C_FN = "hcli_voice_continuous_on_silence"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
