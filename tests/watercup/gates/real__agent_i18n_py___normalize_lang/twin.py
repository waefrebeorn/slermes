"""twin for agent/i18n.py:_normalize_lang (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/i18n.py"
    PY_FN = "_normalize_lang"
    C_FN = "_normalize_lang"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
