"""twin for tools/computer_use/cua_backend.py:_transport_replay_is_safe (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/cua_backend.py"
    PY_FN = "_transport_replay_is_safe"
    C_FN = "_transport_replay_is_safe"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
