"""twin for tools/process_registry.py:_env_poller_loop (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/process_registry.py"
    PY_FN = "_env_poller_loop"
    C_FN = "_env_poller_loop"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
