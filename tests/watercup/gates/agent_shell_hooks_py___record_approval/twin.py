"""twin: LIVE agent/shell_hooks.py allowlist.

Same sequence as harness.c against a private HERMES_HOME, and the cup carries what
load_allowlist() returns afterwards -- the state Python actually persisted -- plus
the count revoke() reports. A port that prints and returns 0 cannot reproduce this.
"""
import os

from watercup_py import Cup
from watercup_twin_runtime import TwinBase

HOME = ".testbin/shk_py"


class _Twin(TwinBase):
    PY_MODULE = "agent/shell_hooks.py"
    PY_FN = "_record_approval"
    C_FN = "wc_shellhook"

    def trace(self, inp, inputs, idx=0):
        import shutil

        os.environ["HERMES_HOME"] = HOME
        if idx == 0:
            # The harness clears its home once, before the first probe: the inputs
            # are a sequence, so resetting per input would hide every accumulation.
            shutil.rmtree(HOME, ignore_errors=True)
            os.makedirs(HOME, exist_ok=True)
        import agent.shell_hooks as sh

        label = getattr(self, "_cup_label", None) or "wc_shellhook"
        c = Cup(label)
        text = "" if inp is None else str(inp)
        action, _, rest = text.partition("|")
        event, sep, command = rest.partition("|")
        if not sep:
            # "<action>|<command>" with no second bar: the field is the command.
            event, command = "", event
        c.branch(bool(command))                       # B1

        removed = 0
        if action == "record":
            sh._record_approval(event, command)
        elif action == "revoke":
            removed = sh.revoke(command)

        data = sh.load_allowlist()
        pairs = sorted(
            "%s|%s" % (e.get("event", ""), e.get("command", ""))
            for e in data.get("approvals", [])
            if isinstance(e, dict)
        )
        c.valstr(";".join(pairs))                     # V1
        c.valstr(str(int(removed)))                   # V2
        return c.finalize()

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
