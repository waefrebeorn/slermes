# Objects deliberately kept out of the link

All 903 formerly-orphaned translation units were added to `build/objects.mk` in one move
(`CUPBOARD_ORPHANS`) and the breakage was fixed afterwards, rather than the list being grown
one object at a time until a link happened to pass. That surfacing pass found **26 files with
122 compile errors**; 23 were fixed and 3 are listed here because fixing them is a rewrite,
not a repair.

| object | why it stays out | what fixing it requires |
|---|---|---|
| `src/agent/port_turn_summary.o` | implements `turn_tally_total_tools()` and friends against an older tally layout (`counts[]` beside `nouns[]`), while `include/port_turn_summary.h` and the already-linked `src/agent/turn_summary.c` own that API | rewriting the .c onto the header's `noun_entry_t` model, then deleting the duplicate implementation |
| `src/tools/computer_use.o` | `make_windows_backend()` assigns `void *state` functions to `cu_backend_t` slots declared `(void)` in `include/hermes_computer_use.h` — an older backend ABI | converting the 12 `win_*` implementations to the header's signatures (and their call sites) |
| `src/skills_hub_catalog.o` | dereferences `hive_t`, which `lib/libhive/hive.h` deliberately keeps opaque; it wants an older, concrete hive struct | rewriting against `hive_count()/hive_get()` accessors |

Everything else from the 903 is in the product.
