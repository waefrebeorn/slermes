"""twin for cron/blueprint_catalog.py:blueprint_deeplink (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/blueprint_catalog.py"
    PY_FN = "blueprint_deeplink"
    C_FN = "blueprint_deeplink"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
