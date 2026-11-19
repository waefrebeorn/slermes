"""twin for hermes_cli/nous_account.py:_pool_entry_is_portal_oauth (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/nous_account.py"
    PY_FN = "_pool_entry_is_portal_oauth"
    C_FN = "nous_u_pool_entry_is_portal_oauth"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
