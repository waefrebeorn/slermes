-- ui_demo.lua -- draw a TUI-style screen using only the `term` table.
--
-- This is the whole point of src/lua/slv_ui.c: a Lua chunk, not C, decides
-- what appears on screen. The host owns the buffer; this script only calls
-- term.put / term.clear / term.fill.
--
-- Run from the repo root:
--   tests/t_slv_ui_demo games/ui_demo.lua 60 16
--
-- TWO slerm-Lua gaps shape this file (see docs/split-plan.md, "slerm-Lua
-- parser gaps"). Both are worked around here, not avoided:
--
--   1. `local function f(...)` does NOT parse ("'(' expected"). Only
--      `local f = function(...) ... end` and plain `function f(...)`.
--   2. The `..` operator does not accept a FUNCTION CALL as an operand:
--      `print("x" .. tostring(v))` raises "attempt to concatenate a nil
--      value", while `local s = tostring(v); print("x" .. s)` works.
--      So every call result is bound to a local before it is concatenated.

local W = term.width
local H = term.height

term.clear()

-- A box drawn from box-drawing-ish ASCII so it works in any codepage.
local box = function(x, y, w, h, title)
    local horiz = string.rep("-", w - 2)
    term.put(x, y, "+" .. horiz .. "+")
    local row
    for row = 1, h - 2 do
        local blank = string.rep(" ", w - 2)
        term.put(x, y + row, "|" .. blank .. "|")
    end
    term.put(x, y + h - 1, "+" .. horiz .. "+")
    if title and #title > 0 then
        term.put(x + 2, y, " " .. title .. " ")
    end
end

-- Title bar across the top.
local bar = string.rep("=", W)
term.put(0, 0, bar)
term.put(2, 0, " slermes ")
local tag = " lua ui 0.1 "
term.put(W - #tag, 0, tag)

-- Main panel.
local main_w = math.min(40, W - 2)
local main_h = math.min(10, H - 4)
box(1, 2, main_w, main_h, " messages ")

-- A few message lines inside the panel.
term.put(3, 3, "welcome to the lua-driven ui")
term.put(3, 4, "curses is gone; this is a lua chunk")
term.put(3, 5, "drawing into a host-owned buffer")

-- Status line at the bottom.
local rule = string.rep("-", W)
term.put(0, H - 1, rule)
local status = " ready  |  " .. W .. "x" .. H .. "  |  term.put()"
term.put(1, H - 1, status)

-- A right-hand panel only if there is room for it.
if W >= 56 then
    local side_w = W - 44
    box(43, 2, side_w, main_h, " state ")
    term.put(45, 3, "buffer : host-owned")
    term.put(45, 4, "script : lua 5.4")
    term.put(45, 5, "clipped : yes")
end
