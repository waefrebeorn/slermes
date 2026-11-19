"""twin for tools/tts_tool.py:_command_provider_env_passthrough (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tts_tool.py"
    PY_FN = "_command_provider_env_passthrough"
    C_FN = "_command_provider_env_passthrough"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
