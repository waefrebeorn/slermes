"""twin for hermes_cli/plugins.py:register_tts_provider (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins.py"
    PY_FN = "register_tts_provider"
    C_FN = "plug_register_tts_provider"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
