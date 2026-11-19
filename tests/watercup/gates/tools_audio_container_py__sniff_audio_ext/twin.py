"""twin for tools/audio_container.py:sniff_audio_ext (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/audio_container.py"
    PY_FN = "sniff_audio_ext"
    C_FN = "tools_audio_container_sniff_audio_ext"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
