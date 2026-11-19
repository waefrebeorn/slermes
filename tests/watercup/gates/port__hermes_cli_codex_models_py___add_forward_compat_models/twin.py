"""twin for hermes_cli/codex_models.py:_add_forward_compat_models (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/codex_models.py"
    PY_FN = "_add_forward_compat_models"
    C_FN = "hermes_cli_codex_models_u_add_forward_compat_models"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
