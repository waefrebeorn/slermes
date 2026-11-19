"""twin for hermes_cli/credential_lifecycle.py:_scrub_config_yaml_mirrors (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/credential_lifecycle.py"
    PY_FN = "_scrub_config_yaml_mirrors"
    C_FN = "hermes_cli_credential_lifecycl_u_scrub_config_yaml_mirrors"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
