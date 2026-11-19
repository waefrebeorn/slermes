"""twin for tools/env_probe.py:warm_environment_probe_async (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/env_probe.py"
    PY_FN = "warm_environment_probe_async"
    C_FN = "tools_env_probe_warm_environment_probe_async"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
