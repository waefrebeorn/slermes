"""twin for hermes_cli/codex_models.py:get_codex_model_ids (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/codex_models.py"
    PY_FN = "get_codex_model_ids"
    C_FN = "get_codex_model_ids"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
