#!/usr/bin/env python3
"""Shadow mirror of src/agent/anthropic_convert_flow2.c — a LITERAL Python
transcription of the C algorithms (json presence semantics, seam consumption,
state effects, raise messages, evaluation order), diffed against the LIVE oracle
drive over every fixture case (ret + ordered trace + state). Code-only verification
substitute for the C gate: catches transcription slips without compiling/linking.

C json_t* model: Python None == C NULL pointer (absent / seam-None); the JNULL
singleton == a JSON_NULL node (present null). json_obj_get returns None for an
absent key and JNULL for a present null; json_set/json_append NO-OP on a None
value (C ignores NULL); m_copy deep-copies. Emit converts JNULL -> JSON null.

Run: python tests/shadow_anthropic_convert_flow2.py
"""
import copy
import importlib.util
import json
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("oracle_acv2", HERE / "oracle_anthropic_convert_flow2.py")
oracle = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(oracle)


class _JNull:
    _inst = None
    def __new__(cls):
        if cls._inst is None:
            cls._inst = super().__new__(cls)
        return cls._inst
    def __repr__(self):
        return "JNULL"
    def __deepcopy__(self, memo):
        return self


JNULL = _JNull()


def to_c(v):
    """JSON value -> C model (null -> JNULL)."""
    if v is None:
        return JNULL
    if isinstance(v, dict):
        return {k: to_c(x) for k, x in v.items()}
    if isinstance(v, list):
        return [to_c(x) for x in v]
    return v


def from_c(v):
    """C model -> JSON value (JNULL and None -> null)."""
    if v is None or v is JNULL:
        return None
    if isinstance(v, dict):
        return {k: from_c(x) for k, x in v.items()}
    if isinstance(v, list):
        return [from_c(x) for x in v]
    return v


def m_is_none(v):
    return v is None or v is JNULL


def m_is_obj(v):
    return isinstance(v, dict)


def m_is_arr(v):
    return isinstance(v, list)


def m_is_str(v):
    return isinstance(v, str)


def m_is_num(v):
    return isinstance(v, (int, float)) and not isinstance(v, bool)


def m_truthy(v):
    if v is None or v is JNULL:
        return False
    if isinstance(v, bool):
        return v
    if isinstance(v, (int, float)):
        return v != 0.0
    if isinstance(v, str):
        return len(v) > 0
    if isinstance(v, (list, dict)):
        return len(v) > 0
    return False


def m_get(obj, key):
    if m_is_obj(obj):
        return obj.get(key)  # absent -> None (C NULL); present-null -> JNULL
    return None


def m_get_def(obj, key, default):
    if m_is_obj(obj) and key in obj:
        return obj[key]
    return default


def m_set(obj, key, val):
    # C json_set: NO-OP on NULL val; replace in place (keep position) else append
    if not m_is_obj(obj) or val is None:
        return
    obj[key] = val


def m_append(arr, item):
    # C json_append drops a NULL item
    if m_is_arr(arr) and item is not None:
        arr.append(item)


def m_copy(v):
    return copy.deepcopy(v)


def m_json_eq(a, b):
    if m_is_none(a) and m_is_none(b):
        return True
    if m_is_none(a) or m_is_none(b):
        return False
    if isinstance(a, bool) != isinstance(b, bool):
        return False
    if isinstance(a, bool):
        return a == b
    if m_is_num(a) and m_is_num(b):
        return a == b
    if m_is_num(a) != m_is_num(b):
        return False
    if m_is_str(a) and m_is_str(b):
        return a == b
    if m_is_str(a) != m_is_str(b):
        return False
    if m_is_arr(a) and m_is_arr(b):
        return len(a) == len(b) and all(m_json_eq(x, y) for x, y in zip(a, b))
    if m_is_arr(a) != m_is_arr(b):
        return False
    if m_is_obj(a) and m_is_obj(b):
        if len(a) != len(b):
            return False
        for k in a:
            if k not in b or not m_json_eq(a[k], b[k]):
                return False
        return True
    return True


def m_type_name(v):
    if v is None or v is JNULL:
        return "NoneType"
    if isinstance(v, bool):
        return "bool"
    if isinstance(v, (int, float)):
        d = float(v)
        if d == math.floor(d) and abs(d) < 1e15:
            return "int"
        return "float"
    if isinstance(v, str):
        return "str"
    if isinstance(v, list):
        return "list"
    if isinstance(v, dict):
        return "dict"
    return "object"


def m_float_repr(d):
    # literal transcription of acv2_float_repr
    sci = None
    for prec in range(1, 18):
        sci = format(d, ".%de" % (prec - 1))
        if float(sci) == d:
            break
    p = sci
    neg = False
    if p.startswith("-"):
        neg = True
        p = p[1:]
    dig = ""
    i = 0
    while i < len(p) and p[i] not in "eE":
        if p[i].isdigit():
            dig += p[i]
        i += 1
    E = int(p[i + 1:]) if i < len(p) else 0
    while len(dig) > 1 and dig.endswith("0"):
        dig = dig[:-1]
    if -4 <= E < 16:
        point = E + 1
        if point <= 0:
            zeros = "0" * min(-point, 30)
            out = "%s0.%s%s" % ("-" if neg else "", zeros, dig)
        elif point >= len(dig):
            zeros = "0" * min(point - len(dig), 30)
            out = "%s%s%s.0" % ("-" if neg else "", dig, zeros)
        else:
            out = "%s%s.%s" % ("-" if neg else "", dig[:point], dig[point:])
    else:
        mant = dig[0] + "." + dig[1:] if len(dig) > 1 else dig[0]
        ae = abs(E)
        out = "%s%se%c%02d" % ("-" if neg else "", mant, "-" if E < 0 else "+", ae)
    return out


def m_pystr(v):
    if m_is_none(v):
        return "None"
    if isinstance(v, bool):
        return "True" if v else "False"
    if m_is_num(v):
        d = float(v)
        if d == math.floor(d) and abs(d) < 1e15:
            return "%.0f" % d
        return m_float_repr(d)
    if m_is_str(v):
        return v
    return ""


WS = set([0x20] + list(range(0x09, 0x0D + 1)) + list(range(0x1C, 0x1F + 1)) +
         [0x85, 0xA0, 0x1680] + list(range(0x2000, 0x200A + 1)) +
         [0x2028, 0x2029, 0x202F, 0x205F, 0x3000])


def m_nonws(s):
    return any(ord(c) not in WS for c in s)


def m_pylen(s):
    return len(s)


def m_strip_lower(s):
    a = 0
    b = len(s)
    while a < b and ord(s[a]) in WS:
        a += 1
    while b > a and ord(s[b - 1]) in WS:
        b -= 1
    return "".join(chr(ord(c) + 32) if "A" <= c <= "Z" else c for c in s[a:b])


THINKING = ["thinking", "redacted_thinking"]
CACHEABLE = ["text", "tool_use"]
PLACEHOLDER = "(empty)"


def m_in_thinking(bt):
    return m_is_str(bt) and bt in THINKING


def m_in_cacheable(bt):
    return m_is_str(bt) and bt in CACHEABLE


def m_types_array(types):
    return [types[0], types[1]]


class MCtx:
    def __init__(self, io):
        self.io = io          # C-model scripts dict (borrowed)
        self.trace = []
        self.counters = {}
        self.exc = None


def m_raise(ctx, cls, msg):
    ctx.exc = {"__raise__": cls, "msg": msg}


def m_raise_attr_get(ctx, v):
    m_raise(ctx, "AttributeError", "'%s' object has no attribute 'get'" % m_type_name(v))


def m_raise_attr_method(ctx, v, method):
    m_raise(ctx, "AttributeError", "'%s' object has no attribute '%s'" % (m_type_name(v), method))


def m_raise_not_iterable(ctx, v):
    m_raise(ctx, "TypeError", "'%s' object is not iterable" % m_type_name(v))


def m_raise_not_subscriptable(ctx, v):
    m_raise(ctx, "TypeError", "'%s' object is not subscriptable" % m_type_name(v))


def m_raise_key(ctx, key):
    m_raise(ctx, "KeyError", "'%s'" % key)


def m_pending(ctx):
    return ctx.exc is not None


def m_call(ctx, name, args):
    # literal acv2_call: args ownership transferred; ret NEW (None == None)
    if args is None:
        args = {}
    script = ctx.io.get(name) if m_is_obj(ctx.io) else None
    chosen = None
    fc_marker = None
    if script is None:
        fc_marker = {"__raise__": "RuntimeError", "msg": "no script for " + name}
        chosen = fc_marker
    elif m_is_obj(script):
        seq = script.get("$seq")
        if m_is_arr(seq):
            cur = ctx.counters.get(name, 0)
            idx = int(cur)
            if idx >= len(seq):
                fc_marker = {"__raise__": "RuntimeError", "msg": "script exhausted: " + name}
                chosen = fc_marker
            else:
                ctx.counters[name] = cur + 1
                chosen = seq[idx]
        else:
            chosen = script
    else:
        chosen = script
    ret = None
    raises = False
    if fc_marker is not None:
        ctx.exc = m_copy(fc_marker)
        raises = True
    elif m_is_obj(chosen):
        r = chosen.get("__raise__")
        if m_is_str(r):
            ctx.exc = m_copy(chosen)
            raises = True
        else:
            ret = m_copy(chosen)
    elif chosen is not None and chosen is not JNULL:
        ret = m_copy(chosen)
    ev = {"ev": name, "args": args}
    ev["ret"] = m_copy(ctx.exc) if raises else (m_copy(ret) if ret is not None else JNULL)
    ctx.trace.append(ev)
    return ret


def m_arg_node(args, key, val):
    if not m_is_none(val):
        m_set(args, key, m_copy(val))


def m_arg_str(args, key, val):
    if val is not None:
        m_set(args, key, val)


def m_set_contains(s, v):
    if not m_is_arr(s):
        return False
    return any(m_json_eq(x, v) for x in s)


def m_set_diff(left, right):
    out = []
    if m_is_arr(left):
        for x in left:
            if not m_set_contains(right, x):
                out.append(m_copy(x))
    return out


def m_set_union_into(dest, src):
    if not m_is_arr(src):
        return
    for x in src:
        if not m_set_contains(dest, x):
            dest.append(m_copy(x))


def m_array_insert0(arr, val):
    if m_is_arr(arr) and val is not None:
        arr.insert(0, val)


def m_apply_cc_effect(blocks, cc):
    if not m_is_obj(cc) or not m_is_arr(blocks):
        return
    for k in range(len(blocks) - 1, -1, -1):
        block = blocks[k]
        bt = m_get(block, "type")
        if m_in_cacheable(bt):
            if m_is_obj(block) and "cache_control" not in block:
                m_set(block, "cache_control", m_copy(cc))
            break


def m_apply_cc_seam(ctx, blocks, cc):
    a = {}
    m_arg_node(a, "blocks", blocks)
    m_arg_node(a, "cache_control", cc)
    r = m_call(ctx, "apply_assistant_cache_control", a)
    if not m_pending(ctx):
        m_apply_cc_effect(blocks, cc)


def m_carry_cc_effect(out, b, copy_flag):
    if not m_is_obj(out) or not m_is_obj(b):
        return
    cc = m_get(b, "cache_control")
    if not m_is_obj(cc):
        return
    m_set(out, "cache_control", m_copy(cc))


def m_carry_cc_seam(ctx, out, b, copy_flag):
    a = {}
    m_arg_node(a, "out", out)
    m_arg_node(a, "b", b)
    m_set(a, "copy", copy_flag)
    r = m_call(ctx, "carry_cache_control", a)
    if not m_pending(ctx):
        m_carry_cc_effect(out, b, copy_flag)


def m_log_warning(ctx, fmt, args):
    a = {}
    m_set(a, "fmt", fmt)
    m_set(a, "args", args if args is not None else [])
    m_call(ctx, "logger.warning", a)


def m_abl_seam(ctx, result):
    a = {}
    m_arg_node(a, "result", result)
    return m_call(ctx, "assistant_block_lists", a)


def m_abl_rebind(ctx, result, pair, idx_out):
    if not m_is_arr(pair) or len(pair) < 1 or not m_is_num(pair[0]):
        m_raise(ctx, "TypeError", "invalid assistant_block_lists script pair")
        return None
    di = pair[0]
    if di < 0 or di >= len(result) or di != math.floor(di):
        m_raise(ctx, "IndexError", "list index out of range")
        return None
    idx_out[0] = int(di)
    return result[int(di)]


def m_unpack_ok(ctx, r, n):
    if not m_is_arr(r):
        m_raise_not_iterable(ctx, r)
        return False
    if len(r) < n:
        m_raise(ctx, "ValueError", "not enough values to unpack (expected %d)" % n)
        return False
    if len(r) > n:
        m_raise(ctx, "ValueError", "too many values to unpack (expected %d)" % n)
        return False
    return True


# ── ported functions (literal C transcription) ──────────────────────────────

def m_fix_blank(ctx, blocks, placeholder_text, msg_index, role, location):
    a = {}
    m_arg_node(a, "blocks", blocks)
    r = m_call(ctx, "split_blank_text_blocks", a)
    if m_pending(ctx):
        return None
    if not m_unpack_ok(ctx, r, 3):
        return None
    kept = m_copy(r[0])
    relocated = m_copy(r[1]) if m_is_obj(r[1]) else None
    dropped = r[2]
    if not m_is_arr(dropped):
        m_raise_not_iterable(ctx, dropped)
        return None
    for bi in dropped:
        aa = [msg_index, m_copy(role) if role is not None else JNULL, location, m_copy(bi)]
        m_log_warning(ctx,
                      "Pre-call sanitizer: dropped blank text content block "
                      "(message_index=%d role=%s location=%s block_index=%d block_type=text)",
                      aa)
        if m_pending(ctx):
            return None
    if not m_truthy(kept):
        ta = {}
        m_arg_str(ta, "text", placeholder_text)
        tb = m_call(ctx, "text_block", ta)
        if m_pending(ctx):
            return None
        kept = []
        m_append(kept, tb if tb is not None else JNULL)
    m_apply_cc_seam(ctx, kept, relocated)
    if m_pending(ctx):
        return None
    return kept


def m_extract(ctx, message):
    if not m_is_obj(message):
        m_raise_attr_get(ctx, message)
        return None
    raw = m_get(message, "reasoning_details")
    if not m_is_arr(raw):
        return []
    a = {}
    m_arg_node(a, "message", message)
    minted = m_call(ctx, "reasoning_minted_outside_claude", a)
    if m_pending(ctx):
        return None
    foreign = m_truthy(minted)
    if foreign:
        return []
    empty_str = ""
    out = []
    for d in raw:
        if not m_is_obj(d):
            continue
        tv = m_get_def(d, "type", empty_str)
        orv = tv if m_truthy(tv) else empty_str
        s = m_pystr(orv)
        sl = m_strip_lower(s)
        if sl in THINKING:
            out.append(m_copy(d))
    return out


def m_convert_content(ctx, content):
    if not m_is_arr(content):
        return m_copy(content) if content is not None else JNULL
    out = []
    for part in content:
        a = {}
        m_arg_node(a, "part", part)
        b = m_call(ctx, "convert_content_part_to_anthropic", a)
        if m_pending(ctx):
            return None
        if b is not None and b is not JNULL:
            m_append(out, b)
    return out


def m_content_parts(ctx, parts):
    out = []
    if not m_is_arr(parts):
        return out
    for part in parts:
        a = {}
        m_arg_node(a, "part", part)
        block = m_call(ctx, "convert_content_part_to_anthropic", a)
        if m_pending(ctx):
            return None
        if not m_truthy(block):
            continue
        if not m_is_obj(block):
            m_raise_attr_get(ctx, block)
            return None
        btype = m_get(block, "type")
        text_val = m_get(block, "text")
        src = m_get(block, "source")
        if m_is_str(btype) and btype == "text" and m_is_str(text_val) and m_truthy(text_val):
            ta = {}
            m_arg_node(ta, "text", text_val)
            tb = m_call(ctx, "text_block", ta)
            if m_pending(ctx):
                return None
            m_append(out, tb if tb is not None else JNULL)
        elif m_is_str(btype) and btype == "image" and m_is_obj(src) and m_truthy(src):
            ib = {}
            m_set(ib, "type", "image")
            m_set(ib, "source", m_copy(src))
            m_append(out, ib)
    return out


def m_convert_user(ctx, content):
    if m_is_arr(content):
        converted = m_convert_content(ctx, content)
        if m_pending(ctx):
            return None
        role = "user"
        c = m_fix_blank(ctx, converted, "(empty message)", -1, role, "_convert_user_message")
        if m_pending(ctx):
            return None
    elif not m_truthy(content) or (m_is_str(content) and not m_nonws(content)):
        c = "(empty message)"
    else:
        c = m_copy(content) if content is not None else JNULL
    o = {}
    m_set(o, "role", "user")
    m_set(o, "content", c)
    return o


def m_convert_tools(ctx, tools):
    result = []
    seen = []
    empty_obj = {}
    empty_str = ""
    iter_ = tools if m_truthy(tools) else None
    if iter_ is not None and not m_is_arr(iter_):
        m_raise_not_iterable(ctx, iter_)
        return None
    for t in (iter_ or []):
        if not m_is_obj(t):
            m_raise_attr_get(ctx, t)
            return None
        fn = m_get_def(t, "function", empty_obj)
        if not m_is_obj(fn):
            m_raise_attr_get(ctx, fn)
            return None
        name = m_get_def(fn, "name", empty_str)
        if m_truthy(name) and m_set_contains(seen, name):
            aa = [m_copy(name)]
            m_log_warning(ctx,
                          "convert_tools_to_anthropic: duplicate tool name '%s' "
                          "\u2014 dropping second occurrence",
                          aa)
            if m_pending(ctx):
                return None
            continue
        if m_truthy(name):
            m_append(seen, m_copy(name))
        tool = {}
        m_set(tool, "name", m_copy(name))
        m_set(tool, "description", m_copy(m_get_def(fn, "description", empty_str)))
        params = m_get(fn, "parameters")
        schema_arg = params if m_truthy(params) else empty_obj
        na = {}
        m_arg_node(na, "schema", schema_arg)
        ns = m_call(ctx, "normalize_tool_input_schema", na)
        if m_pending(ctx):
            return None
        m_set(tool, "input_schema", ns if ns is not None else JNULL)
        m_carry_cc_seam(ctx, tool, t, True)
        if m_pending(ctx):
            return None
        m_append(result, tool)
    return result


def m_convert_tool_msg(ctx, result, m):
    if not m_is_obj(m):
        m_raise_attr_get(ctx, m)
        return
    empty_str = ""
    tr = {}
    m_set(tr, "type", "tool_result")
    tid = m_get_def(m, "tool_call_id", empty_str)
    sa = {}
    m_arg_node(sa, "tool_id", tid)
    sid = m_call(ctx, "sanitize_tool_id", sa)
    if m_pending(ctx):
        return
    m_set(tr, "tool_use_id", sid if sid is not None else JNULL)
    ca = {}
    m_arg_node(ca, "m", m)
    content = m_call(ctx, "tool_result_content", ca)
    if m_pending(ctx):
        return
    m_set(tr, "content", content if content is not None else JNULL)
    m_carry_cc_seam(ctx, tr, m, True)
    if m_pending(ctx):
        return
    last = result[-1] if (m_is_arr(result) and len(result) > 0) else None
    last_content = None
    if last is not None:
        if not m_is_obj(last):
            m_raise_attr_get(ctx, last)
            return
        role = m_get(last, "role")
        if m_is_str(role) and role == "user":
            last_content = m_get(last, "content")
    if m_is_arr(last_content) and len(last_content) > 0:
        first = last_content[0]
        if not m_is_obj(first):
            m_raise_attr_get(ctx, first)
            return
        ft = m_get(first, "type")
        if m_is_str(ft) and ft == "tool_result":
            m_append(last_content, tr)
            return
    um = {}
    m_set(um, "role", "user")
    ua = []
    m_append(ua, tr)
    m_set(um, "content", ua)
    m_append(result, um)


def m_convert_assistant(ctx, m):
    if not m_is_obj(m):
        m_raise_attr_get(ctx, m)
        return None
    empty_str = ""
    empty_obj = {}
    empty_arr = []
    content = m_get_def(m, "content", empty_str)
    ca = {}
    m_arg_node(ca, "m", m)
    carrier = m_call(ctx, "assistant_replay_carrier", ca)
    if m_pending(ctx):
        return None
    if not m_unpack_ok(ctx, carrier, 2):
        return None
    ordered = m_truthy(carrier[0])
    blocks = m_copy(carrier[1])
    if ordered:
        o = {}
        m_set(o, "role", "assistant")
        m_set(o, "content", blocks)
        return o
    relocated = None
    if m_is_arr(content):
        converted = m_convert_content(ctx, content)
        if m_pending(ctx):
            return None
        sa = {}
        m_arg_node(sa, "blocks", converted)
        sr = m_call(ctx, "split_blank_text_blocks", sa)
        if m_pending(ctx):
            return None
        if not m_unpack_ok(ctx, sr, 3):
            return None
        kept = sr[0]
        relocated = m_copy(sr[1]) if m_is_obj(sr[1]) else None
        if not m_is_arr(blocks):
            m_raise_attr_method(ctx, blocks, "extend")
            return None
        if not m_is_arr(kept):
            m_raise_not_iterable(ctx, kept)
            return None
        for k in kept:
            m_append(blocks, m_copy(k))
    elif m_truthy(content):
        s = m_pystr(content)
        if m_nonws(s):
            if not m_is_arr(blocks):
                m_raise_attr_method(ctx, blocks, "append")
                return None
            ta = {}
            m_arg_str(ta, "text", s)
            tb = m_call(ctx, "text_block", ta)
            if m_pending(ctx):
                return None
            m_append(blocks, tb if tb is not None else JNULL)
    tcs = m_get_def(m, "tool_calls", empty_arr)
    if not m_is_arr(tcs):
        m_raise_not_iterable(ctx, tcs)
        return None
    for tc in tcs:
        if not m_truthy(tc) or not m_is_obj(tc):
            continue
        fn = m_get_def(tc, "function", empty_obj)
        if not m_is_obj(fn):
            m_raise_attr_get(ctx, fn)
            return None
        if not m_is_arr(blocks):
            m_raise_attr_method(ctx, blocks, "append")
            return None
        id_v = m_get_def(tc, "id", empty_str)
        name_v = m_get_def(fn, "name", empty_str)
        args_default = "{}"
        args_v = m_get_def(fn, "arguments", args_default)
        pa = {}
        m_arg_node(pa, "raw", args_v)
        parsed = m_call(ctx, "parse_tool_args", pa)
        if m_pending(ctx):
            return None
        tua = {}
        m_arg_node(tua, "tool_id", id_v)
        m_arg_node(tua, "name", name_v)
        m_arg_node(tua, "tool_input", parsed)
        tub = m_call(ctx, "tool_use_block", tua)
        if m_pending(ctx):
            return None
        m_append(blocks, tub if tub is not None else JNULL)
    rc = m_get(m, "reasoning_content")
    if m_is_str(rc):
        ha = {}
        m_arg_node(ha, "blocks", blocks)
        m_set(ha, "types", m_types_array(THINKING))
        hb = m_call(ctx, "has_block_type", ha)
        if m_pending(ctx):
            return None
        has = m_truthy(hb)
        if not has:
            if not m_is_arr(blocks):
                m_raise_attr_method(ctx, blocks, "insert")
                return None
            th = {}
            m_set(th, "type", "thinking")
            m_set(th, "thinking", m_copy(rc))
            m_array_insert0(blocks, th)
    if m_truthy(blocks):
        effective = blocks
    else:
        ta = {}
        m_arg_str(ta, "text", PLACEHOLDER)
        tb = m_call(ctx, "text_block", ta)
        if m_pending(ctx):
            return None
        effective = []
        m_append(effective, tb if tb is not None else JNULL)
    m_apply_cc_seam(ctx, effective, relocated)
    if m_pending(ctx):
        return None
    m_apply_cc_seam(ctx, effective, m_get(m, "cache_control"))
    if m_pending(ctx):
        return None
    o = {}
    m_set(o, "role", "assistant")
    m_set(o, "content", effective)
    return o


def m_strip_orphaned(ctx, result):
    pairs = m_abl_seam(ctx, result)
    if m_pending(ctx):
        return
    if not m_is_arr(pairs):
        m_raise_not_iterable(ctx, pairs)
        return
    for pair in pairs:
        idx_out = [0]
        m = m_abl_rebind(ctx, result, pair, idx_out)
        if m is None:
            return
        idx = idx_out[0]
        mc = m_get(m, "content")
        if mc is None:
            m_raise_key(ctx, "content")
            return
        ba = {}
        m_arg_node(ba, "blocks", mc)
        m_arg_str(ba, "btype", "tool_use")
        m_arg_str(ba, "key", "id")
        ids = m_call(ctx, "block_ids", ba)
        if m_pending(ctx):
            return
        if not m_truthy(ids):
            continue
        adj = []
        if idx + 1 < len(result):
            nxt = result[idx + 1]
            if not m_is_obj(nxt):
                m_raise_attr_get(ctx, nxt)
                return
            nrole = m_get(nxt, "role")
            ncontent = m_get(nxt, "content")
            if m_is_str(nrole) and nrole == "user" and m_is_arr(ncontent):
                aa = {}
                m_arg_node(aa, "blocks", ncontent)
                m_arg_str(aa, "btype", "tool_result")
                m_arg_str(aa, "key", "tool_use_id")
                ar = m_call(ctx, "block_ids", aa)
                if m_pending(ctx):
                    return
                adj = ar if ar is not None else []
        orphaned = m_set_diff(ids, adj)
        if not m_truthy(orphaned):
            continue
        kept = []
        for b in mc:
            bta = {}
            m_arg_node(bta, "b", b)
            bt = m_call(ctx, "block_type", bta)
            if m_pending(ctx):
                return
            is_tu = m_is_str(bt) and bt == "tool_use"
            drop = False
            if is_tu:
                bid = m_get(b, "id")
                drop = m_set_contains(orphaned, bid)
            if not drop:
                m_append(kept, m_copy(b))
        if len(kept) != len(mc):
            ha = {}
            m_arg_node(ha, "blocks", mc)
            m_set(ha, "types", m_types_array(THINKING))
            hb = m_call(ctx, "has_block_type", ha)
            if m_pending(ctx):
                return
            if m_truthy(hb):
                m_set(m, "_thinking_signature_invalidated", True)
        if len(kept) == 0:
            ta = {}
            m_arg_str(ta, "text", "(tool call removed)")
            tb = m_call(ctx, "text_block", ta)
            if m_pending(ctx):
                return
            m_append(kept, tb if tb is not None else JNULL)
        m_set(m, "content", kept)
    surviving = []
    pairs = m_abl_seam(ctx, result)
    if m_pending(ctx):
        return
    if not m_is_arr(pairs):
        m_raise_not_iterable(ctx, pairs)
        return
    for pair in pairs:
        idx_out = [0]
        m = m_abl_rebind(ctx, result, pair, idx_out)
        if m is None:
            return
        mc = m_get(m, "content")
        if mc is None:
            m_raise_key(ctx, "content")
            return
        ba = {}
        m_arg_node(ba, "blocks", mc)
        m_arg_str(ba, "btype", "tool_use")
        m_arg_str(ba, "key", "id")
        ids = m_call(ctx, "block_ids", ba)
        if m_pending(ctx):
            return
        m_set_union_into(surviving, ids)
    for m in result:
        if not m_is_obj(m):
            m_raise_attr_get(ctx, m)
            return
        role = m_get(m, "role")
        if not (m_is_str(role) and role == "user"):
            continue
        mc = m_get(m, "content")
        if not m_is_arr(mc):
            continue
        nc = []
        for b in mc:
            bta = {}
            m_arg_node(bta, "b", b)
            bt = m_call(ctx, "block_type", bta)
            if m_pending(ctx):
                return
            is_tr = m_is_str(bt) and bt == "tool_result"
            keep = True
            if is_tr:
                tid = m_get(b, "tool_use_id")
                keep = m_set_contains(surviving, tid)
            if keep:
                m_append(nc, m_copy(b))
        if len(nc) != len(mc):
            if len(nc) == 0:
                ta = {}
                m_arg_str(ta, "text", "(tool result removed)")
                tb = m_call(ctx, "text_block", ta)
                if m_pending(ctx):
                    return
                m_append(nc, tb if tb is not None else JNULL)
            m_set(m, "content", nc)


def m_merge(ctx, result):
    if not m_is_arr(result):
        m_raise_not_iterable(ctx, result)
        return None
    fx = []
    for m in result:
        same = False
        if len(fx) > 0:
            prev = fx[-1]
            if not m_is_obj(prev):
                m_raise_not_subscriptable(ctx, prev)
                return None
            pr = m_get(prev, "role")
            if pr is None:
                m_raise_key(ctx, "role")
                return None
            if not m_is_obj(m):
                m_raise_not_subscriptable(ctx, m)
                return None
            mr = m_get(m, "role")
            if mr is None:
                m_raise_key(ctx, "role")
                return None
            same = m_json_eq(pr, mr)
        if not same:
            fx.append(m)
            continue
        mr = m_get(m, "role")
        if not (m_is_str(mr) and mr == "user"):
            flag = m_get(m, "_thinking_signature_invalidated")
            if m_truthy(flag):
                m_set(fx[-1], "_thinking_signature_invalidated", True)
            mc = m_get(m, "content")
            if mc is None:
                m_raise_key(ctx, "content")
                return None
            if m_is_arr(mc):
                sa = {}
                m_arg_node(sa, "blocks", mc)
                st = m_call(ctx, "strip_thinking", sa)
                if m_pending(ctx):
                    return None
                m_set(m, "content", st if st is not None else JNULL)
        prev = fx[-1]
        pc = m_get(prev, "content")
        if pc is None:
            m_raise_key(ctx, "content")
            return None
        mc2 = m_get(m, "content")
        if mc2 is None:
            m_raise_key(ctx, "content")
            return None
        cca = {}
        m_arg_node(cca, "prev", pc)
        m_arg_node(cca, "curr", mc2)
        cc = m_call(ctx, "concat_content", cca)
        if m_pending(ctx):
            return None
        m_set(prev, "content", cc if cc is not None else JNULL)
    out = []
    for m in fx:
        m_append(out, m_copy(m))
    return out


def m_manage(ctx, result, base_url, model):
    ra = {}
    m_arg_node(ra, "base_url", base_url)
    m_arg_node(ra, "model", model)
    route = m_call(ctx, "anthropic_thinking_route", ra)
    if m_pending(ctx):
        return
    lsa = {}
    m_arg_node(lsa, "result", result)
    ls = m_call(ctx, "current_tool_loop_start", lsa)
    if m_pending(ctx):
        return
    loop_start = ls if m_is_num(ls) else 0.0
    if not m_is_arr(result):
        m_raise(ctx, "TypeError", "object of type '%s' has no len()" % m_type_name(result))
        return
    last_assistant = float(len(result))
    for k in range(len(result) - 1, -1, -1):
        m = result[k]
        if not m_is_obj(m):
            m_raise_attr_get(ctx, m)
            return
        role = m_get(m, "role")
        if m_is_str(role) and role == "assistant":
            last_assistant = float(k)
            break
    loop = min(loop_start, last_assistant)
    pa = {}
    m_arg_node(pa, "route", route)
    m_arg_node(pa, "model", model)
    pp = m_call(ctx, "route_replays_prior_thinking", pa)
    if m_pending(ctx):
        return
    preserve = m_truthy(pp)
    pairs = m_abl_seam(ctx, result)
    if m_pending(ctx):
        return
    if not m_is_arr(pairs):
        m_raise_not_iterable(ctx, pairs)
        return
    for pair in pairs:
        idx_out = [0]
        m = m_abl_rebind(ctx, result, pair, idx_out)
        if m is None:
            return
        idx = idx_out[0]
        mc = m_get(m, "content")
        if mc is None:
            m_raise_key(ctx, "content")
            return
        is_kimi = m_is_str(route) and route == "kimi"
        is_deepseek = m_is_str(route) and route == "deepseek"
        is_third = m_is_str(route) and route == "third_party"
        if is_kimi:
            pass
        elif is_deepseek:
            nc = []
            for b in mc:
                bta = {}
                m_arg_node(bta, "b", b)
                bt = m_call(ctx, "block_type", bta)
                if m_pending(ctx):
                    return
                thinking = m_in_thinking(bt)
                keep = True
                if thinking:
                    sig = m_get(b, "signature")
                    signed_v = m_truthy(sig)
                    if not signed_v:
                        signed_v = m_truthy(m_get(b, "data"))
                    keep = not signed_v
                if keep:
                    m_append(nc, m_copy(b))
            if len(nc) == 0:
                ta = {}
                m_arg_str(ta, "text", "(empty)")
                tb = m_call(ctx, "text_block", ta)
                if m_pending(ctx):
                    return
                m_append(nc, tb if tb is not None else JNULL)
            m_set(m, "content", nc)
        elif is_third or (float(idx) < loop and not preserve):
            sa = {}
            m_arg_node(sa, "blocks", mc)
            st = m_call(ctx, "strip_thinking", sa)
            if m_pending(ctx):
                return
            if not m_truthy(st):
                ta = {}
                m_arg_str(ta, "text", "(thinking elided)")
                tb = m_call(ctx, "text_block", ta)
                if m_pending(ctx):
                    return
                st = []
                m_append(st, tb if tb is not None else JNULL)
            m_set(m, "content", st)
        else:
            dead = m_truthy(m_get(m, "_thinking_signature_invalidated"))
            ka = {}
            m_arg_node(ka, "content", mc)
            m_set(ka, "signature_dead", dead)
            kv = m_call(ctx, "keep_valid_thinking", ka)
            if m_pending(ctx):
                return
            if not m_truthy(kv):
                ta = {}
                m_arg_str(ta, "text", "(empty)")
                tb = m_call(ctx, "text_block", ta)
                if m_pending(ctx):
                    return
                kv = []
                m_append(kv, tb if tb is not None else JNULL)
            m_set(m, "content", kv)
        newmc = m_get(m, "content")
        if m_is_arr(newmc):
            for b in newmc:
                bta = {}
                m_arg_node(bta, "b", b)
                bt = m_call(ctx, "block_type", bta)
                if m_pending(ctx):
                    return
                if m_in_thinking(bt):
                    if m_is_obj(b):
                        b.pop("cache_control", None)
        if m_is_obj(m):
            m.pop("_thinking_signature_invalidated", None)


def m_evict(ctx, result):
    if not m_is_arr(result):
        m_raise_not_iterable(ctx, result)
        return
    reserved = []  # (block borrowed, data owned)
    for msg in result:
        if not m_is_obj(msg):
            m_raise_attr_get(ctx, msg)
            return
        mc = m_get(msg, "content")
        if not m_is_arr(mc):
            continue
        for b in mc:
            bta = {}
            m_arg_node(bta, "b", b)
            bt = m_call(ctx, "block_type", bta)
            if m_pending(ctx):
                return
            is_img = m_is_str(bt) and bt == "image"
            if not is_img:
                continue
            da = {}
            m_arg_node(da, "block", b)
            data = m_call(ctx, "image_source_data", da)
            if m_pending(ctx):
                return
            reserved.append((b, data))
    carriers = []  # (block borrowed, [(ib, data)])
    for k in range(len(result) - 1, -1, -1):
        msg = result[k]
        mc = m_get(msg, "content")
        if not m_is_arr(mc):
            continue
        for j in range(len(mc) - 1, -1, -1):
            b = mc[j]
            bta = {}
            m_arg_node(bta, "b", b)
            bt = m_call(ctx, "block_type", bta)
            if m_pending(ctx):
                return
            is_tr = m_is_str(bt) and bt == "tool_result"
            if not is_tr:
                continue
            bc = m_get(b, "content")
            if not m_is_arr(bc):
                continue
            ha = {}
            m_arg_node(ha, "blocks", bc)
            m_set(ha, "types", ["image"])
            hb = m_call(ctx, "has_block_type", ha)
            if m_pending(ctx):
                return
            if not m_truthy(hb):
                continue
            images = []
            for ib in bc:
                ita = {}
                m_arg_node(ita, "b", ib)
                ibt = m_call(ctx, "block_type", ita)
                if m_pending(ctx):
                    return
                is_img = m_is_str(ibt) and ibt == "image"
                if not is_img:
                    continue
                da = {}
                m_arg_node(da, "block", ib)
                data = m_call(ctx, "image_source_data", da)
                if m_pending(ctx):
                    return
                images.append((ib, data))
            carriers.append((b, images))
    datas = []
    for _, d in reserved:
        m_append(datas, m_copy(d) if d is not None else JNULL)
    for _, images in carriers:
        for _, d in images:
            m_append(datas, m_copy(d) if d is not None else JNULL)
    ma = {}
    m_arg_node(ma, "datas", datas)
    mis = m_call(ctx, "images_all_within_many_image_limit", ma)
    if m_pending(ctx):
        return
    counts = []
    cbnf = []
    for _, images in carriers:
        m_append(counts, len(images))
        bytes_ = 0
        for _, d in images:
            if m_truthy(d) and m_is_str(d):
                bytes_ += m_pylen(d)
        m_append(cbnf, bytes_)
    reserved_bytes = 0
    for _, d in reserved:
        if m_truthy(d) and m_is_str(d):
            reserved_bytes += m_pylen(d)
    oa = {}
    m_arg_node(oa, "counts", counts)
    m_set(oa, "reserved_count", len(reserved))
    m_arg_node(oa, "carrier_bytes_newest_first", cbnf)
    m_set(oa, "reserved_bytes", reserved_bytes)
    m_arg_node(oa, "many_image_safe", mis)
    retire_v = m_call(ctx, "outbound_image_retire_count", oa)
    if m_pending(ctx):
        return
    retire = retire_v if m_is_num(retire_v) else 0.0
    s = float(len(carriers)) - retire
    start = 0 if s <= 0 else (len(carriers) if s >= len(carriers) else int(s))
    for k in range(start, len(carriers)):
        ta = {}
        m_arg_str(ta, "text", "[screenshot removed to save context]")
        ph = m_call(ctx, "text_block", ta)
        if m_pending(ctx):
            return
        bc = m_get(carriers[k][0], "content")
        nc = []
        for ib in bc:
            ita = {}
            m_arg_node(ita, "b", ib)
            ibt = m_call(ctx, "block_type", ita)
            if m_pending(ctx):
                return
            is_img = m_is_str(ibt) and ibt == "image"
            if is_img:
                m_append(nc, m_copy(ph) if ph is not None else JNULL)
            else:
                m_append(nc, m_copy(ib))
        m_set(carriers[k][0], "content", nc)


def m_ensure_leading(ctx, result):
    if not m_truthy(result):
        return
    if not m_is_arr(result):
        m_raise_not_subscriptable(ctx, result)
        return
    if len(result) == 0:
        return
    first = result[0]
    if not m_is_obj(first):
        m_raise_attr_get(ctx, first)
        return
    role = m_get(first, "role")
    if m_is_str(role) and role == "user":
        return
    ta = {}
    m_arg_str(ta, "text", PLACEHOLDER)
    ph = m_call(ctx, "text_block", ta)
    if m_pending(ctx):
        return
    um = {}
    m_set(um, "role", "user")
    ua = []
    m_append(ua, ph if ph is not None else JNULL)
    m_set(um, "content", ua)
    m_array_insert0(result, um)


def m_scrub(ctx, result):
    if not m_is_arr(result):
        m_raise_not_iterable(ctx, result)
        return
    for mi, msg in enumerate(result):
        if not m_is_obj(msg):
            continue
        role = m_get(msg, "role")
        content = m_get(msg, "content")
        if not m_is_arr(content) or len(content) == 0:
            continue
        ph = PLACEHOLDER if (m_is_str(role) and role == "assistant") else "(empty message)"
        fixed = m_fix_blank(ctx, content, ph, mi, role, "content")
        if m_pending(ctx):
            return
        m_set(msg, "content", fixed)
        for blk in fixed:
            bta = {}
            m_arg_node(bta, "b", blk)
            bt = m_call(ctx, "block_type", bta)
            if m_pending(ctx):
                return
            is_tr = m_is_str(bt) and bt == "tool_result"
            inner = m_get(blk, "content") if is_tr else None
            if m_is_arr(inner) and len(inner) > 0:
                ifixed = m_fix_blank(ctx, inner, "(no output)", mi, role, "tool_result")
                if m_pending(ctx):
                    return
                m_set(blk, "content", ifixed)


def m_convert_messages(ctx, messages, base_url, model):
    if not m_is_arr(messages):
        m_raise_not_iterable(ctx, messages)
        return None
    empty_str = ""
    default_role = "user"
    system = JNULL
    result = []
    for m in messages:
        if not m_is_obj(m):
            m_raise_attr_get(ctx, m)
            return None
        role = m_get_def(m, "role", default_role)
        if m_is_str(role) and role == "system":
            cv = m_get_def(m, "content", empty_str)
            sa = {}
            m_arg_node(sa, "content", cv)
            sr = m_call(ctx, "convert_system_content", sa)
            if m_pending(ctx):
                return None
            system = sr if sr is not None else JNULL
        elif m_is_str(role) and role == "assistant":
            am = m_convert_assistant(ctx, m)
            if m_pending(ctx):
                return None
            m_append(result, am if am is not None else JNULL)
        elif m_is_str(role) and role == "tool":
            m_convert_tool_msg(ctx, result, m)
            if m_pending(ctx):
                return None
        else:
            cv = m_get_def(m, "content", empty_str)
            um = m_convert_user(ctx, cv)
            if m_pending(ctx):
                return None
            m_append(result, um if um is not None else JNULL)
    m_strip_orphaned(ctx, result)
    if m_pending(ctx):
        return None
    merged = m_merge(ctx, result)
    if m_pending(ctx):
        return None
    result = merged
    m_ensure_leading(ctx, result)
    if m_pending(ctx):
        return None
    m_manage(ctx, result, base_url, model)
    if m_pending(ctx):
        return None
    m_evict(ctx, result)
    if m_pending(ctx):
        return None
    m_scrub(ctx, result)
    if m_pending(ctx):
        return None
    out = []
    m_append(out, system)
    m_append(out, result)
    return out


STATEFUL = {
    "_convert_tool_message_to_result", "_strip_orphaned_tool_blocks",
    "_merge_consecutive_roles", "_manage_thinking_signatures",
    "_evict_old_screenshots", "_ensure_leading_user_turn",
    "_scrub_blank_text_blocks",
}


def mirror_run(rec):
    inp = rec["in"]
    io = to_c(inp.get("io") or {})
    ctx = MCtx(io)
    g = lambda k: to_c(inp[k]) if k in inp else None
    state = None
    ret = None
    func = rec["func"]
    if func == "_extract_preserved_thinking_blocks":
        ret = m_extract(ctx, g("message"))
    elif func == "convert_tools_to_anthropic":
        ret = m_convert_tools(ctx, g("tools"))
    elif func == "_convert_content_to_anthropic":
        ret = m_convert_content(ctx, g("content"))
    elif func == "_content_parts_to_anthropic_blocks":
        ret = m_content_parts(ctx, g("parts"))
    elif func == "_convert_assistant_message":
        ret = m_convert_assistant(ctx, g("m"))
    elif func == "_convert_user_message":
        ret = m_convert_user(ctx, g("content"))
    elif func == "_fix_blank_text_blocks_in_list":
        ret = m_fix_blank(ctx, g("blocks"), inp.get("placeholder_text", "(empty)"),
                          inp.get("msg_index", 0), g("role"), inp.get("location", "content"))
    elif func == "convert_messages_to_anthropic":
        ret = m_convert_messages(ctx, g("messages"), g("base_url"), g("model"))
    elif func in STATEFUL:
        result = m_copy(g("result"))
        if result is None:
            result = JNULL
        state = result
        if func == "_convert_tool_message_to_result":
            m_convert_tool_msg(ctx, result, g("m"))
        elif func == "_strip_orphaned_tool_blocks":
            m_strip_orphaned(ctx, result)
        elif func == "_merge_consecutive_roles":
            ret = m_merge(ctx, result)
        elif func == "_manage_thinking_signatures":
            m_manage(ctx, result, g("base_url"), g("model"))
        elif func == "_evict_old_screenshots":
            m_evict(ctx, result)
        elif func == "_ensure_leading_user_turn":
            m_ensure_leading(ctx, result)
        elif func == "_scrub_blank_text_blocks":
            m_scrub(ctx, result)
    else:
        raise KeyError("unknown func " + func)
    if ctx.exc is not None:
        ret_out = ctx.exc
    else:
        ret_out = ret if ret is not None else JNULL
    st = {}
    if state is not None:
        st["result"] = state
    return {"ret": from_c(ret_out), "trace": from_c(ctx.trace), "state": from_c(st)}


def main():
    fx = HERE / "fixtures" / "anthropic_convert_flow2.jsonl"
    recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").split("\n") if l.strip()]
    mism = 0
    shown = 0
    for i, rec in enumerate(recs):
        live = oracle.run_case(rec)
        mir = mirror_run(rec)
        # ordered canon: key order is part of the contract
        if json.dumps(mir, sort_keys=False, ensure_ascii=False) != json.dumps(live, sort_keys=False, ensure_ascii=False):
            mism += 1
            if shown < 10:
                shown += 1
                print(f"SHADOW MISMATCH case {i} {rec['func']}:")
                print(f"  MIRROR={json.dumps(mir, ensure_ascii=False)[:800]}")
                print(f"  LIVE  ={json.dumps(live, ensure_ascii=False)[:800]}")
    print(f"SHADOW anthropic_convert_flow2: {len(recs)} cases, {mism} mismatches")
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()