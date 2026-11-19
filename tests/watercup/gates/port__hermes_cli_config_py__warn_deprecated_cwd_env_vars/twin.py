"""twin for hermes_cli/config.py:warn_deprecated_cwd_env_vars (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/config.py"
    PY_FN = "warn_deprecated_cwd_env_vars"
    C_FN = "cfg_warn_deprecated_cwd_env_vars"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
