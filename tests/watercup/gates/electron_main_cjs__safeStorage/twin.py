"""twin for electron/main.cjs:safeStorage (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "electron/main.cjs"
    PY_FN = "safeStorage"
    C_FN = "desktop_auth_ticket_set"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
