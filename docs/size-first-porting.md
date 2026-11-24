# Size-first porting: the biggest modules with no C code

The wave planner walked `docs/gap-c-homes.json` by *function count* — 1-5, then
6-9 — which is a reasonable proxy for "tractable unit" but silently skipped
every large module, because large modules are large precisely because they have
50-370 methods. That ladder ran dry with a lot of Python still unported.

Measured on the inventory (2026-11-24, `python scripts/wave_size.py --list`):

- 2 090 upstream modules in the gap inventory; **468 have no C code**
  (no `src/<module>.c` over 900 bytes) — 10.0 MB of Python.
- Size histogram of the unported set: **9 modules >100 KB**, 23 at 50-100 KB,
  111 at 20-50 KB, 151 at 10-20 KB, 174 at 5-10 KB, 61 under 5 KB.
- The band that wave porting had been consuming (1-9 functions) is **empty**:
  0 candidates left. Waves 25 and 26 exhausted it.

## The biggest unported modules

| bytes | defs | module |
| ---: | ---: | --- |
| 415 245 | 369 | `plugins/platforms/telegram/adapter` |
| 387 033 | 329 | `plugins/platforms/slack/adapter` |
| 380 091 | 391 | `plugins/platforms/discord/adapter` |
| 177 137 | 178 | `plugins/platforms/matrix/adapter` |
| 122 611 | 103 | `persistence/sqlite/conversation/_store/messages` |
| 109 557 | 119 | `plugins/platforms/buzz/adapter` |
| 104 112 | 77 | `persistence/sqlite/_schema` |
| 101 867 | 96 | `plugins/platforms/whatsapp/adapter` |
| 101 423 | 101 | `plugins/platforms/google_chat/adapter` |
| 97 959 | 122 | `plugins/platforms/photon/adapter` |
| 96 689 | 88 | `gateway/platforms/api_server_openai_routes` |
| 87 092 | 113 | `plugins/kanban/dashboard/plugin_api` |

## What changed

`scripts/wave_size.py` ranks by bytes and decomposes a module into gates:

```
python scripts/wave_size.py --list 20
python scripts/wave_size.py --emit --wave 27 --per-wave 12 --per-gate 30 --per-child 1
```

- **One module = one owner = one `src/<module>.c`.** No two children ever write
  the same port file; that collision is what makes a 400 KB adapter unsafe to
  split across agents.
- A module with more defs than `--per-gate` becomes gates `<gate>_p1`, `_p2`, …
  Each part gets its own header, harness, oracle and fixture, all linked against
  the one port object, so verification granularity stays honest while the port
  file stays single-writer.
- Assignment files are one line per module: `module<TAB>gate,gate,...`. A child
  owns the module and writes every part's four test files plus the port.

Wave 27 is the first size-first wave: the 12 biggest unported modules, 75 gates,
2.2 MB of Python. The small-module plans that had been staged as waves 27/28 are
retired — they were superseded, not lost work (nothing had been dispatched).

## What "ported" means for a 400 KB adapter

These modules are mostly I/O: Bot API calls, sockets, polling loops, SDK clients.
What is portable is the value logic inside them — text/HTML/markdown conversion,
chunking, callback-data encode/decode, addressing and mention parsing, thread
and id keys, retry/backoff math, payload shaping, policy predicates. Everything
else stays a seam and is listed in the port file's header. A gate that cannot
find three genuinely pure functions ports the constant tables plus the closest
value-only helpers and says so, rather than shipping an empty stub with a PoP
comment.

The honesty rule still governs: a module counts as done when its gates are green
and the binary still links — never because a stub grew a PoP line.
