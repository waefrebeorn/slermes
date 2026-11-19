"""twin for tools/transcription_tools.py:_try_lazy_install_stt (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/transcription_tools.py"
    PY_FN = "_try_lazy_install_stt"
    C_FN = "tsc_try_lazy_install_stt"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
