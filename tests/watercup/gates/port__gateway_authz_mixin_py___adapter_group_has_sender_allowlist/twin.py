"""twin for gateway/authz_mixin.py:_adapter_group_has_sender_allowlist (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/authz_mixin.py"
    PY_FN = "_adapter_group_has_sender_allowlist"
    C_FN = "cli_gateway_authz_mixin__adapter_group_has_sender_allowlist"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
