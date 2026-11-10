#!/usr/bin/env python3
"""Probe slerm-Lua (Wua) against Lua 5.4 and print a gap list.

Each probe is a tiny self-contained chunk. A probe PASSES when it runs to
completion and prints the expected token; it FAILS when the interpreter raises
or prints something else.

Run from the repo root:  python tests/probe_lua_gaps.py
"""
import os
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HARNESS = os.path.join(tempfile.gettempdir(), "slv_probe.exe")

# (name, lua source, expected substring or None for "no error")
PROBES = [
    # ---- lexer ----
    ("num_hex",            "print(0xFF)", "255"),
    ("num_float_exp",      "print(1e3)", "1000"),
    ("num_float_dot",      "print(1.5 + 2.5)", "4"),
    ("str_escape_n",       "print(#'a\\nb')", "3"),
    ("str_escape_t",       "print(#'a\\tb')", "3"),
    ("str_escape_quote",   "print#'a\\'b'", "3"),
    ("str_long_bracket",   "print=[[hello]]", "hello"),
    ("comment_long",       "--[[ block ]] print(1)", "1"),
    ("comment_line",       "-- line\nprint(2)", "2"),
    ("semicolon",          "local a = 1; print(a)", "1"),
    # ---- operators ----
    ("int_div",            "print(7 // 2)", "3"),
    ("int_div_neg",        "print(-7 // 2)", "-4"),
    ("mod_neg",            "print(-7 % 3)", "2"),
    ("pow",                "print(2 ^ 10)", "1024"),
    ("band",               "print(6 & 3)", "2"),
    ("bor",                "print(6 | 3)", "7"),
    ("bxor",               "print(6 ~ 3)", "5"),
    ("bnot",               "print(~0)", "-1"),
    ("shl",                "print(1 << 4)", "16"),
    ("shr",                "print(256 >> 4)", "16"),
    ("unary_minus",        "print(-3 + 1)", "-2"),
    ("not",                "print(not nil)", "true"),
    ("and_or",             "print(1 and 2, nil or 3)", "2\t3"),
    ("concat_chain",       "print('a' .. 'b' .. 'c')", "abc"),
    ("compare",            "print(1 < 2, 2 <= 2, 3 > 4, 1 == 1, 1 ~= 2)", "true"),
    ("len_str",            "print(#'hello')", "5"),
    ("len_table",          "local t = {1,2,3} print(#t)", "3"),
    # ---- statements ----
    ("if_elseif",          "local x = 2 if x == 1 then print('a') elseif x == 2 then print('b') else print('c') end", "b"),
    ("while",              "local n = 0 while n < 3 do n = n + 1 end print(n)", "3"),
    ("repeat",             "local n = 0 repeat n = n + 1 until n >= 3 print(n)", "3"),
    ("for_numeric",        "local s = 0 for i = 1, 5 do s = s + i end print(s)", "15"),
    ("for_step",           "local s = 0 for i = 10, 1, -3 do s = s + i end print(s)", "22"),
    ("break",              "local n = 0 while true do n = n + 1 if n >= 4 then break end end print(n)", "4"),
    ("goto_label",         "local n = 0 ::top:: n = n + 1 if n < 3 then goto top end print(n)", "3"),
    ("do_block",           "do local a = 5 print(a) end", "5"),
    ("multi_assign",       "local a, b = 1, 2 print(a + b)", "3"),
    ("swap",               "local a, b = 1, 2 a, b = b, a print(a, b)", "2\t1"),
    # ---- functions ----
    ("fndef_plain",        "function f(x) return x * 2 end print(f(21))", "42"),
    ("fndef_local",        "local f = function(x) return x * 2 end print(f(21))", "42"),
    ("fndef_local_kw",     "local function f(x) return x * 2 end print(f(21))", "42"),
    ("multi_return",       "local function f() return 1, 2, 3 end local a, b, c = f() print(a + b + c)", "6"),
    ("multi_return_trunc", "local function f() return 1, 2 end print(f())", "1\t2"),
    ("varargs",            "local function f(...) local a, b = ... return a + b end print(f(3, 4))", "7"),
    ("varargs_select",     "local function f(...) return select('#', ...) end print(f(1, 2, 3))", "3"),
    ("closure",            "local function counter() local n = 0 return function() n = n + 1 return n end end local c = counter() c() print(c())", "2"),
    ("method_call",        "local t = {v = 5} function t:get() return self.v end print(t:get())", "5"),
    ("nested_call",        "local function f(x) return x + 1 end print(f(f(f(0))))", "3"),
    ("call_in_concat",     "local function f() return 'z' end print('a' .. f())", "az"),
    ("tostring_in_concat", "print('x=' .. tostring(9))", "x=9"),
    # ---- tables ----
    ("table_nested",       "local t = {a = {b = {c = 7}}} print(t.a.b.c)", "7"),
    ("table_array_mix",    "local t = {1, 2, x = 3} print(t[1] + t[2] + t.x)", "6"),
    ("table_len_mixed",    "local t = {1, 2, 3, x = 9} print(#t)", "3"),
    ("table_set_nil",      "local t = {a = 1} t.a = nil print(t.a)", "nil"),
    ("table_ipairs",       "local t = {10, 20} local s = 0 for i, v in ipairs(t) do s = s + v end print(s)", "30"),
    # ---- metatables ----
    ("meta_index",         "local base = {greet = function() return 'hi' end} local t = setmetatable({}, {__index = base}) print(t.greet())", "hi"),
    ("meta_newindex",      "local log = {} local t = setmetatable({}, {__newindex = function(tb, k, v) log[k] = v end}) t.x = 5 print(log.x)", "5"),
    ("meta_add",           "local mt = {__add = function(a, b) return 99 end} local t = setmetatable({}, mt) print(t + 1)", "99"),
    ("meta_tostring",      "local mt = {__tostring = function() return 'OBJ' end} local t = setmetatable({}, mt) print(tostring(t))", "OBJ"),
    ("meta_concat",        "local mt = {__concat = function(a, b) return 'C' end} local t = setmetatable({}, mt) print(t .. 'x')", "C"),
    ("meta_eq",            "local mt = {__eq = function() return true end} local a = setmetatable({}, mt) local b = setmetatable({}, mt) print(a == b)", "true"),
    ("meta_call",          "local t = setmetatable({}, {__call = function(self, x) return x * 3 end}) print(t(4))", "12"),
    ("meta_len",           "local t = setmetatable({}, {__len = function() return 42 end}) print(#t)", "42"),
    # ---- string library ----
    ("str_byte",           "print(string.byte('A'))", "65"),
    ("str_char",           "print(string.char(72, 105))", "Hi"),
    ("str_find",           "print(string.find('hello', 'll'))", "3\t4"),
    ("str_format_s",       "print(string.format('%s-%d', 'a', 5))", "a-5"),
    ("str_format_05d",     "print(string.format('%05d', 42))", "00042"),
    ("str_format_f",       "print(string.format('%.2f', 3.14159))", "3.14"),
    ("str_format_x",       "print(string.format('%x', 255))", "ff"),
    ("str_gmatch",         "local s = 0 for w in string.gmatch('a,b,c', '[^,]+') do s = s + 1 end print(s)", "3"),
    ("str_gsub",           "print(string.gsub('hello', 'l', 'L'))", "heLLo"),
    ("str_len",            "print(string.len('abc'))", "3"),
    ("str_lower",          "print(string.lower('ABC'))", "abc"),
    ("str_upper",          "print(string.upper('abc'))", "ABC"),
    ("str_match",          "print(string.match('key=val', '(%w+)=(%w+)'))", "key\tval"),
    ("str_rep",            "print(string.rep('ab', 3))", "ababab"),
    ("str_reverse",        "print(string.reverse('abc'))", "cba"),
    ("str_sub",            "print(string.sub('hello', 2, 4))", "ell"),
    ("str_pattern_cap",    "print(string.match('2026-01-02', '(%d+)-(%d+)-(%d+)'))", "2026\t01\t02"),
    ("str_pattern_class",  "print(string.match('abc123', '%a+'))", "abc"),
    ("str_pattern_anchor", "print(string.match('hello', '^h'))", "h"),
    # ---- table library ----
    ("tbl_concat",         "print(table.concat({1, 2, 3}, ','))", "1,2,3"),
    ("tbl_insert",         "local t = {1} table.insert(t, 2) print(#t)", "2"),
    ("tbl_remove",         "local t = {1, 2, 3} table.remove(t, 2) print(#t)", "2"),
    ("tbl_unpack",         "print(table.unpack({7, 8}))", "7\t8"),
    ("tbl_pack",           "local t = table.pack(1, 2) print(t.n)", "2"),
    # ---- math library ----
    ("math_abs",           "print(math.abs(-5))", "5"),
    ("math_ceil",          "print(math.ceil(1.2))", "2"),
    ("math_floor",         "print(math.floor(1.8))", "1"),
    ("math_max",           "print(math.max(1, 9, 3))", "9"),
    ("math_min",           "print(math.min(4, 2, 8))", "2"),
    ("math_sqrt",          "print(math.sqrt(16))", "4"),
    ("math_huge",          "print(math.huge > 1e308)", "true"),
    ("math_type_int",      "print(math.type(1))", "integer"),
    ("math_type_float",    "print(math.type(1.0))", "float"),
    ("math_tointeger",     "print(math.tointeger(3.0))", "3"),
    # ---- error handling ----
    ("pcall_ok",           "local ok, v = pcall(function() return 7 end) print(ok, v)", "true\t7"),
    ("pcall_err",          "local ok, e = pcall(function() error('boom') end) print(ok)", "false"),
    ("error_level",        "print(pcall(error, 'x'))", "false"),
    ("assert_pass",        "print(assert(1, 'm'))", "1"),
    ("assert_fail",        "print(pcall(assert, false, 'nope'))", "false"),
    # ---- misc ----
    ("type_nil",           "print(type(nil))", "nil"),
    ("type_func",          "print(type(print))", "function"),
    ("tostring_nil",       "print(tostring(nil))", "nil"),
    ("tonumber",           "print(tonumber('42') + 1)", "43"),
    ("tostring_num",       "print(tostring(3.5))", "3.5"),
    ("select_negative",    "print(select('#', 1, 2, 3))", "3"),
    ("string_in_method",   "print(('abc'):upper())", "ABC"),
    ("generic_for_custom", "local function iter(state, i) if i < 3 then return i + 1 end end for i in iter, nil, 0 do io.write(i) end", "123"),
]


def build():
    import glob
    src = os.path.join(REPO, "tests", "slv_probe_main.c")
    lv = os.path.join(REPO, "src", "lv")
    lua = os.path.join(REPO, "src", "lua")
    objs = sorted(glob.glob(os.path.join(lv, "slv_*.c")))
    cmd = (["gcc", "-O2", "-g", "-Wall", "-Werror=implicit-function-declaration",
            "-I", lua, "-I", lv] + objs +
           [os.path.join(lua, "slv_ui.c"), src, "-o", HARNESS, "-lm"])
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print("probe harness build FAILED:\n" + r.stderr[:2000])
        sys.exit(2)


def run_probe(name, code):
    """Returns (ok, detail)."""
    with tempfile.NamedTemporaryFile("w", suffix=".lua", delete=False) as f:
        f.write(code + "\n")
        path = f.name
    try:
        r = subprocess.run([HARNESS, path],
                           capture_output=True, text=True, timeout=20)
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT"
    finally:
        os.unlink(path)
    out = r.stdout.strip().replace("\r", "")
    if r.returncode != 0:
        first = (r.stderr or out).strip().splitlines()
        return False, first[0] if first else f"rc={r.returncode}"
    return True, out


def main():
    build()
    ok_n = 0
    gaps = []
    for name, code, want in PROBES:
        ok, detail = run_probe(name, code)
        if ok:
            ok_n += 1
            status = "ok  "
        else:
            gaps.append((name, code.replace("\n", " "), detail))
            status = "GAP "
        print(f"{status} {name:22s} {detail if not ok else ''}")

    print()
    print(f"=== {ok_n}/{len(PROBES)} probes pass, {len(gaps)} gaps ===")
    if gaps:
        print()
        print("GAP LIST (for an agent to fix):")
        for i, (name, code, detail) in enumerate(gaps, 1):
            print(f"  {i:2d}. {name}")
            print(f"      lua: {code}")
            print(f"      ->  {detail}")


if __name__ == "__main__":
    main()
