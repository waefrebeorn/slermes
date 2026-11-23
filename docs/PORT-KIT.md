# No-C-home port kit (Slermes)

Use this to port an upstream Hermes Python module that has no C home.

## Paths
- Repo (C11 port): `C:/Users/eman5/slermes` — ALL writes go here, ABSOLUTE paths.
- Quarry (live Python): `C:/Users/eman5/hermes-agent` — read-only source of truth.
- Build env: `/c/msys64/usr/bin/bash -lc 'source /c/Users/eman5/slermes/build/winlibs-env.sh; cd /c/Users/eman5/slermes && ...'`

## What to create (8 files per module)
Given Python module `pkg/mod.py` and short gate name `GATE` (lower-case, unique):

1. `include/noc_GATE.h` — public C API. Prefix EVERY exported symbol with `GATE_`
   (e.g. `urlfmt_format_url_host`) to avoid duplicate definitions with existing C.
2. `src/pkg/mod.c` — EXACT mirrored path (the parity scanner only counts `src/<mod>.c`).
   Header comment must contain:
   `PoP: <GATE>_<fn> @ pkg/mod.py:<python_symbol>`
3. `tests/t_port_GATE.c` — JSON-lines harness. Reads stdin fixture lines, emits
   `{"func":<name>,"out":<value>, ...echoed input keys...}`.
4. `tests/oracle_GATE.py` — imports the LIVE module via
   `importlib.util.spec_from_file_location`, compares `out` to live result.
   Re-configure stdout/stderr to utf-8. Exit 1 on any mismatch.
5. `tests/fixtures/GATE.jsonl` — 8+ diverse cases including edge/null/boundary.
6. Add DEPS line to `tests/run_all_oracles.sh` (see below).
7. Add `<src path>.o` to `build/objects.mk` CUPBOARD_PORT_WIRED list (parent does this centrally — DO NOT EDIT build files or run_all_oracles.sh yourself).
8. Nothing else.

## Harness skeleton (copy exactly)
```c
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_GATE.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        /* dispatch: call GATE_* functions, build json_t *out */
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
```

## Pitfalls that already cost a gate
- `json_len(obj)` returns **0 for JSON_OBJECT** — use `json_object_size(obj)` and
  `json_object_get_at(obj,i)` / `json_object_get_key_at(obj,i)` for objects.
- Windows CRT `setenv(name, "", 1)` UNSETS the variable; Python `os.environ[name]=""
  ` keeps it. For env-driven Python behaviour, expose an explicit `_ex(..., env_value)`
  seam and inject from the fixture instead of mutating the process environment.
- Numbers are `double`; pass 64-bit ints as decimal STRINGS and use `strtoll`.
- Builders: `json_null/string/number/bool/array/object`, `json_set`, `json_append`,
  `json_free`, `json_copy`, `json_get_str/num/bool`, `json_obj_get`, `json_dumps`.
- Types are `JSON_NULL/BOOL/NUMBER/STRING/ARRAY/OBJECT`; fields `type`, `bool_val`,
  `num_val`, `str_val`, `c.count`, `c.items`, `c.keys`.

## Gate command (run this yourself to verify, after the parent prebuilds objects)
```
/c/msys64/usr/bin/bash -lc 'source /c/Users/eman5/slermes/build/winlibs-env.sh; \
 cd /c/Users/eman5/slermes && \
 bash tests/run_one_oracle.sh GATE oracle_GATE.py "" "<objlist> lib/libjson/json.o"'
```
Expect `ORACLE GATE: N cases, 0 mismatches`.

## Rules
- Faithful semantics: match Python exactly (str() coercion, None handling, str.strip,
  set ordering -> sorted output, dict.get fallbacks). Gates catch wrong mental models.
- No stubs: every exported function does the real work.
- DO NOT edit `build/*.mk`, `tests/run_all_oracles.sh`, or docs. The parent wires centrally.
- DO NOT run `make` (concurrent subagents race on .o files). Create files only.
- Report: list files created, exported symbols, and any gate output you ran.
