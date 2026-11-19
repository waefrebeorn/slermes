"""twin for hermes_cli/runtime_provider.py:_anthropic_base_url_override_ok (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/runtime_provider.py"
    PY_FN = "_anthropic_base_url_override_ok"
    C_FN = "provider_anthropic_base_url_override_ok"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
