"""twin for hermes_cli/azure_detect.py:_looks_like_anthropic_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/azure_detect.py"
    PY_FN = "_looks_like_anthropic_path"
    C_FN = "azure_looks_like_anthropic_path"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
