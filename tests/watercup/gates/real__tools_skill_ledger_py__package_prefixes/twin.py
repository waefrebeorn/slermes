"""twin for tools/skill_ledger.py:package_prefixes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skill_ledger.py"
    PY_FN = "package_prefixes"
    C_FN = "package_prefixes"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
