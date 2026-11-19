"""twin for hermes_cli/models.py:_base_url_looks_like_anthropic_messages (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/models.py"
    PY_FN = "_base_url_looks_like_anthropic_messages"
    C_FN = "models_base_url_looks_like_anthropic_messages"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
