"""twin for tools/env_probe.py:_reset_cache_for_tests (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/env_probe.py"
    PY_FN = "_reset_cache_for_tests"
    C_FN = "cli_tools_env_probe__reset_cache_for_tests"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
