"""twin for agent/agent_runtime_helpers.py:repair_message_sequence (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase, Cup
_SPECS = [('out', 'int', 'count')]
_LENY = __import__('re').compile(
    '(count|len|num|size|length|total|^n$)')


def _outvals(result):
    n = len(_SPECS)
    if isinstance(result, tuple):
        if len(result) == n:
            return list(result)
        if len(result) == n + 1:
            return list(result[1:])
        raise ValueError('arity:' + str(len(result)))
    if n == 1:
        kind, ct, name = _SPECS[0]
        if kind == 'buf':
            return [result]
        if (isinstance(result, (str, bytes, list, dict, set,
                tuple)) and _LENY.search(name or '')):
            return [len(result)]
        return [result]
    raise ValueError('arity:non-tuple')


def _enc_out(sp, v):
    kind, ct, name = sp
    if kind == 'buf':
        if isinstance(v, bytes):
            return v.decode('utf-8', 'replace')
        return v if isinstance(v, str) else str(v)
    if ct == 'bool':
        return 'true' if v else 'false'
    if ct in ('double', 'float'):
        return '%.17g' % float(v)
    return str(int(v))

def _strtol(s):
    m = __import__('re').match('[+-]?[0-9]+', str(s))
    return int(m.group()) if m else 0


def _strtod(s):
    m = __import__('re').match(
        '[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?',
        str(s))
    return float(m.group()) if m else 0.0


def _cvt(p, v):
    a = str(p.annotation) if p.annotation is not p.empty else ''
    al = a.lower()
    if 'path' in al:
        from pathlib import Path
        return Path(v) if isinstance(v, str) else v
    if 'bytes' in al and 'str' not in al:
        return (v.encode('utf-8', 'surrogateescape')
                if isinstance(v, str) else v)
    if 'bool' in al and 'str' not in al:
        return _strtol(v) != 0
    if ('int' in al and 'point' not in al and 'print' not in al
            and 'interface' not in al):
        return _strtol(v)
    if 'float' in al or 'double' in al:
        return _strtod(v)
    if ('dict' in al or 'list' in al or 'map' in al or 'seq' in al
            or 'tuple' in al or 'set' in al or 'json' in al):
        try:
            import json as _j
            return _j.loads(v)
        except Exception:
            return None
    return v


class _Twin(TwinBase):
    # repair_message_sequence lives in session_rules/turns.py, not in the helpers module the
    # generator guessed; with the wrong module the LIVE fn never resolved and every input
    # produced the same ERROR cup, so the gate could never say anything about the port.
    PY_MODULE = "session_rules/turns.py"
    PY_FN = "repair_message_sequence"
    C_FN = "hermes_state_repair_message_sequence"

    def trace(self, inp, inputs, idx=0):
        """The live fn is (agent, messages): call it with the decoded JSON list.

        call_fn's walk feeds the raw string twice (once as ``agent``, once as ``messages``), so
        ``messages`` arrived as a str and the repair passes compared characters. ``agent`` is a
        passthrough in the body, so None is faithful.
        """
        # The cup is seeded with the name the harness passes to WCUP_BEGIN
        # ("repair_message_sequence"), NOT the C symbol: the seed is folded into the hash, so a
        # different name made every comparison mismatch regardless of the values.
        c = Cup("repair_message_sequence")
        try:
            import json as _json
            try:
                msgs = _json.loads(inp) if isinstance(inp, str) else inp
            except Exception:
                msgs = []
            if not isinstance(msgs, list):
                msgs = []
            # Same four pre-call rungs the harness reports, in the same order, so the two
            # cups cover the same events: B1 non-empty, B2 adjacent assistants,
            # B3 has tool rows, B4 adjacent users.
            roles = [(m.get("role") or "") if isinstance(m, dict) else "" for m in msgs]
            adj_asst = any(roles[k - 1] == "assistant" and roles[k] == "assistant"
                           for k in range(1, len(roles)))
            adj_user = any(roles[k - 1] == "user" and roles[k] == "user"
                           for k in range(1, len(roles)))
            has_tool = any(r == "tool" for r in roles)
            c.branch(len(msgs) > 0)
            c.branch(adj_asst)
            c.branch(has_tool)
            c.branch(adj_user)
            self._last_msgs = msgs
            self.norm(c, self._fn(None, msgs))
        except Exception as e:  # noqa: BLE001 - an honest ERROR cup, not a fake MATCH
            c.valstr("ERROR:%s" % type(e).__name__)
        return c.finalize()

    def norm(self, c, result):
        """Emit the same string the harness dumps: "<repairs>|<field;...;interim#>...".

        The generated twin emitted only the int, so its cup could never equal a harness cup that
        carries the repaired sequence too -- every input "mismatched" for a reason that said
        nothing about the port.
        """
        import json as _json
        parts = ["%d|" % int(result)]
        for m in getattr(self, "_last_msgs", []) or []:
            if not isinstance(m, dict):
                m = {}
            role = m.get("role") or ""
            content = m.get("content")
            if isinstance(content, (dict, list)):
                content = _json.dumps(content, separators=(",", ":"))
            content = "" if content is None else str(content)
            tcid = m.get("tool_call_id") or ""
            tcids = m.get("tool_call_ids")
            if isinstance(tcids, (list, tuple)):
                tcids = ",".join(str(x) for x in tcids)
            if not tcids:
                # Mirror the harness projection: a live row carries tool_calls:[{id,...}], and
                # the port/harness consume the flattened "id[,call_id]" list joined by ";".
                calls = m.get("tool_calls")
                if isinstance(calls, list):
                    ents = []
                    for call in calls:
                        if not isinstance(call, dict):
                            continue
                        cid = call.get("id") or ""
                        fn = call.get("function")
                        alt = fn.get("call_id") if isinstance(fn, dict) else None
                        if not cid and not alt:
                            continue
                        ents.append(cid + ("," + str(alt) if alt else ""))
                    tcids = ";".join(ents)
            tcids = tcids or ""
            fr = m.get("finish_reason") or ""
            rc = m.get("reasoning_content") or ""
            interim = 1 if (m.get("codex_message_items")
                            or m.get("codex_reasoning_items")
                            or fr == "incomplete") else 0
            parts.append("%s;%s;%s;%s;%s;%s;%d#" % (role, content, tcid, tcids, fr, rc, interim))
        c.valstr("".join(parts))



_t = _Twin()
trace = _t.trace
run = _t.run
