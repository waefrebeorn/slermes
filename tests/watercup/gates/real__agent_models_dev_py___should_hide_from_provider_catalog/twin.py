"""twin for agent/models_dev.py:_should_hide_from_provider_catalog (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/models_dev.py"
    PY_FN = "_should_hide_from_provider_catalog"
    C_FN = "mdv_should_hide_from_provider_catalog"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
