"""twin for hermes_cli/model_switch.py:_resolve_alias_fallback (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/model_switch.py"
    PY_FN = "_resolve_alias_fallback"
    C_FN = "msw_u_resolve_alias_fallback"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
