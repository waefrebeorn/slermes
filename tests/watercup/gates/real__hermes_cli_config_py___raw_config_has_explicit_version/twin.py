"""twin for hermes_cli/config.py:_raw_config_has_explicit_version (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/config.py"
    PY_FN = "_raw_config_has_explicit_version"
    C_FN = "_raw_config_has_explicit_version"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
