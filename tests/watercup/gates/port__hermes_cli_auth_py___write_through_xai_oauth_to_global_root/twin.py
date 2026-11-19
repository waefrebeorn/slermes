"""twin for hermes_cli/auth.py:_write_through_xai_oauth_to_global_root (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/auth.py"
    PY_FN = "_write_through_xai_oauth_to_global_root"
    C_FN = "ath_write_through_xai_oauth_to_global_root"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
