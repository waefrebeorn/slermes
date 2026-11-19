"""twin for hermes_cli/runtime_provider.py:_config_base_url_trustworthy_for_bare_custom (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/runtime_provider.py"
    PY_FN = "_config_base_url_trustworthy_for_bare_custom"
    C_FN = "_config_base_url_trustworthy_for_bare_custom"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
