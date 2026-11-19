"""twin for hermes_cli/model_switch.py:_collect_authed_provider_slugs (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/model_switch.py"
    PY_FN = "_collect_authed_provider_slugs"
    C_FN = "_collect_authed_provider_slugs"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
