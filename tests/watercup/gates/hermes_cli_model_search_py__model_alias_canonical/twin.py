"""twin for hermes_cli/model_search.py:model_alias_canonical (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/model_search.py"
    PY_FN = "model_alias_canonical"
    C_FN = "hermes_cli_model_search_model_alias_canonical"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
