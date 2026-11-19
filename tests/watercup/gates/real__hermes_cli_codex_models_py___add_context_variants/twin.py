"""twin for hermes_cli/codex_models.py:_add_context_variants (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/codex_models.py"
    PY_FN = "_add_context_variants"
    C_FN = "_add_context_variants"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
