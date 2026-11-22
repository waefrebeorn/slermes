# slerm-Lua games

Small games and demos written entirely in Lua against the slerm-Lua terminal
substrate (`src/lua/slv_ui.c`). C owns the screen buffer; the Lua chunk owns
all game logic and drawing through the `term` table (`clear`, `put`, `fill`,
`get`, `box`, `hline`, `vline`, `scroll`, `row`, `width`, `height`).

## Run a game

From the repo root:

```bash
tests/t_slv_ui_demo games/wubu_snake.lua 60 20
```

The harness loads the Lua file, runs it once, and dumps the resulting frame.
It is also a pass/fail gate for Snake: `bash tests/run_slv_tests.sh` includes
`tests/t_slv_snake_game.c`, which asserts walls, one snake head, body segments,
food, and a score HUD.

## Games

- **`wubu_snake.lua`** — self-playing Snake. A deterministic greedy autopilot
  steers a growing snake around a walled board toward scattered food; the final
  frame shows the board, the snake, the food, a live score/length/steps HUD,
  and an arcade rank + energy bar. The whole match is simulated in Lua.

- **`ui_demo.lua`** — the original TUI-style screen demo (title bar, two panels,
  status line). Shows the `term` API and documents the two parser gaps it
  works around.

- **`gamemode_demo.lua`** — deterministic gamemode used by
  `tests/t_slv_script_test.c` to verify the `wb` host table and `step(dt)`
  contract.

## Writing Lua for slerm-Lua

The interpreter has five known gaps (see `docs/split-plan.md`, "slerm-Lua
parser gaps"); `wubu_snake.lua` is written against all of them:

1. `local function f()` does not parse — use `local f = function()`.
2. Strings have no metatable — use `string.rep(s, n)`, not `s:rep(n)`.
3. Only `\n` and `\t` escapes work — avoid other backslash escapes.
4. Numeric `for` control variables are floats — use `math.floor(i)` for indices.
5. Explicit-iterator `for v in f, s, var` is broken — use `while` or `ipairs`.

No coroutines. Everything else probed (goto, repeat, metatables on tables,
base/string/table/math libs, `pcall`/`error`, patterns, multiple returns) works.
