"""twin for hermes_cli/providers.py:is_official_openai_host (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/providers.py"
    PY_FN = "is_official_openai_host"
    C_FN = "hermes_cli_providers_is_official_openai_host"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
