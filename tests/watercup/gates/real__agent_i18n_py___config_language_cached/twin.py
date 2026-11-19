"""twin for agent/i18n.py:_config_language_cached (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/i18n.py"
    PY_FN = "_config_language_cached"
    C_FN = "_config_language_cached"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
