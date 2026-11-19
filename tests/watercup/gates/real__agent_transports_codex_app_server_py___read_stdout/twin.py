"""twin for agent/transports/codex_app_server.py:_read_stdout (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/transports/codex_app_server.py"
    PY_FN = "_read_stdout"
    C_FN = "_read_stdout"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
