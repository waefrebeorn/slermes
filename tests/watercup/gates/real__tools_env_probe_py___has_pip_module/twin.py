"""twin for tools/env_probe.py:_has_pip_module (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/env_probe.py"
    PY_FN = "_has_pip_module"
    C_FN = "cli_tools_env_probe__has_pip_module"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
