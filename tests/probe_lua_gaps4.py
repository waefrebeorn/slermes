import glob, os, subprocess, tempfile
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HARNESS = os.path.join(tempfile.gettempdir(), "slv_probe4.exe")
PROBES = [
    ("print_int",      "print(1)", "1"),
    ("print_float",    "print(1.0)", "1.0"),
    ("print_2float",   "print(2.5)", "2.5"),
    ("iowrite_int",    "io.write(1)", "1"),
    ("iowrite_2int",   "io.write(1, 2)", "12"),
    ("iowrite_float",  "io.write(1.5)", "1.5"),
    ("iowrite_str",    "io.write('a')", "a"),
    ("ioformat_int",   "io.write(string.format('%d', 7))", "7"),
    ("str_upper_var",  "local s = 'abc' local f = s.upper print(f(s))", "ABC"),
    ("str_upper_idx",  "local s = 'abc' print(s.upper(s))", "ABC"),
    ("str_upper_colon","local s = 'abc' print(s:upper())", "ABC"),
    ("str_lower_colon","local s = 'ABC' print(s:lower())", "abc"),
    ("str_sub_colon",  "local s = 'hello' print(s:sub(2,3))", "el"),
    ("str_len_colon",  "local s = 'abc' print(s:len())", "3"),
    ("num_tostring",   "print(tostring(1))", "1"),
    ("num_tostring2",  "print(tostring(1.0))", "1.0"),
    ("str_format_d",   "print(string.format('%d', 7))", "7"),
    ("str_format_f",   "print(string.format('%f', 1.5))", "1.500000"),
    ("esc_n",          "print(#'a\\nb')", "3"),
    ("esc_sq",         "print#'a\\'b'", "3"),
    ("esc_bs",         "print#'a\\\\b'", "3"),
    ("esc_hex",        "print#'a\\x41b'", "4"),
    ("esc_dec",        "print#'a\\65b'", "4"),
    ("esc_u",          "print#'a\\u{41}b'", "3"),
    ("gf_explicit",    "local it = function(s, i) if i < 3 then return i + 1 end end for i in it, nil, 0 do io.write(i) end", "123"),
    ("gf_ipairs",      "for i, v in ipairs({5, 6}) do io.write(i, v) end", "1526"),
    ("gf_pairs",       "local n = 0 for k, v in pairs({a = 1}) do n = n + v end print(n)", "1"),
]

def build():
    src = os.path.join(REPO, "tests", "slv_probe_main.c")
    lv = os.path.join(REPO, "src", "lv"); lua = os.path.join(REPO, "src", "lua")
    objs = sorted(glob.glob(os.path.join(lv, "slv_*.c")))
    cmd = (["gcc","-O2","-g","-Wall","-Werror=implicit-function-declaration","-I",lua,"-I",lv]
           + objs + [os.path.join(lua,"slv_ui.c"), src, "-o", HARNESS, "-lm"])
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode: print(r.stderr[:1500]); raise SystemExit(2)

def run(code):
    with tempfile.NamedTemporaryFile("w", suffix=".lua", delete=False) as f:
        f.write(code+"\n"); path=f.name
    try:
        r = subprocess.run([HARNESS, path], capture_output=True, text=True, timeout=20)
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT"
    finally:
        os.unlink(path)
    out = r.stdout.strip().replace("\r","")
    if r.returncode:
        lines=(r.stderr or out).strip().splitlines()
        return False, lines[0] if lines else f"rc={r.returncode}"
    return True, out

build()
ok=0; gaps=[]
for name, code, want in PROBES:
    good, detail = run(code)
    if good and detail == want:
        ok+=1; print(f"ok   {name:18s} {detail!r}")
    elif good:
        print(f"WRONG {name:16s} want={want!r:10s} got={detail!r}")
        gaps.append((name, code, f"want {want!r} got {detail!r}"))
    else:
        print(f"GAP  {name:18s} {detail}")
        gaps.append((name, code, detail))
print(f"\n=== {ok}/{len(PROBES)} correct, {len(gaps)} problems ===")
