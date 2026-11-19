"""twin for hermes_cli/security_advisories.py:ack_advisory (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/security_advisories.py"
    PY_FN = "ack_advisory"
    C_FN = "cli_security_advisories_ack_advisory"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
