"""twin for hermes_cli/session_export.py:iter_user_prompt_records (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/session_export.py"
    PY_FN = "iter_user_prompt_records"
    C_FN = "sexp_iter_user_prompt_records"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
