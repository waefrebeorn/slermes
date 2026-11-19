"""twin for hermes_cli/tools_config.py:_configure_simple_requirements (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/tools_config.py"
    PY_FN = "_configure_simple_requirements"
    C_FN = "tools_config_configure_simple_requirements"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
