"""twin for hermes_cli/nous_subscription.py:_toolset_enabled (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/nous_subscription.py"
    PY_FN = "_toolset_enabled"
    C_FN = "nsub_u_toolset_enabled"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
