"""twin for gateway/cwd_placeholder.py:_truthy_env (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/cwd_placeholder.py"
    PY_FN = "_truthy_env"
    C_FN = "gateway_cwd_placeholder_u_truthy_env"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
