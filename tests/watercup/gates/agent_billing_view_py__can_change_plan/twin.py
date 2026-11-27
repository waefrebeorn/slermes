"""twin for agent/billing_view.py:can_change_plan — LIVE OrgRoleCapability.

The generated twin handed the raw input string to a property on a permissive dummy
receiver, so the answer could not depend on the role at all. Here the input is the
same compact encoding the C harness uses: "<raw>|<role>", where an empty raw means
Python's None (the server did not say), and the answer is read off a real object.
"""
from watercup_py import Cup
from watercup_twin_runtime import TwinBase


class _Twin(TwinBase):
    PY_MODULE = "agent/billing_view.py"
    PY_FN = "OrgRoleCapability.can_change_plan"
    C_FN = "agent_billing_view_can_change_plan"

    def trace(self, inp, inputs, idx=0):
        label = getattr(self, "_cup_label", None) or self.C_FN
        c = Cup(label)
        text = "" if inp is None else str(inp)
        raw_s, _, role = text.partition("|")
        cap = self._fn.__self__ if hasattr(self._fn, "__self__") else None
        from agent.billing_view import OrgRoleCapability  # noqa: F401
        obj = OrgRoleCapability()
        obj.role = role if role else None
        obj.can_change_plan_raw = (None if raw_s == ""
                                   else raw_s in ("1", "true", "True"))
        try:
            c.valstr("1" if obj.can_change_plan else "0")
        except Exception as exc:
            c.valstr("ERROR:%s" % type(exc).__name__)
        return c.finalize()

    def norm(self, c, result):
        c.valstr("1" if result else "0")


_t = _Twin()
trace = _t.trace
run = _t.run
