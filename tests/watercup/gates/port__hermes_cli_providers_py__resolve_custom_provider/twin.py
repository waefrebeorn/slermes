"""twin for hermes_cli/providers.py:resolve_custom_provider (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/providers.py"
    PY_FN = "resolve_custom_provider"
    C_FN = "hermes_cli_providers_resolve_custom_provider"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
