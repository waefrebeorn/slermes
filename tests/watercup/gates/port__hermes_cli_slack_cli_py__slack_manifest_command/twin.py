"""twin for hermes_cli/slack_cli.py:slack_manifest_command (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/slack_cli.py"
    PY_FN = "slack_manifest_command"
    C_FN = "hermes_cli_slack_cli_slack_manifest_command"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
