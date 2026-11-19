"""twin for tools/environments/managed_modal.py:_guard_unsupported_credential_passthrough (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/managed_modal.py"
    PY_FN = "_guard_unsupported_credential_passthrough"
    C_FN = "unsupported"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
