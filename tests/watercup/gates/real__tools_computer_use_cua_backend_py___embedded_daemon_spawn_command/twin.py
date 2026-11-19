"""twin for tools/computer_use/cua_backend.py:_embedded_daemon_spawn_command (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/cua_backend.py"
    PY_FN = "_embedded_daemon_spawn_command"
    C_FN = "_embedded_daemon_spawn_command"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
