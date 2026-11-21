-- wubu_snake.lua -- a SELF-PLAYING Snake arcade game for slerm-Lua.
--
-- The host harness (tests/t_slv_ui_demo) runs this chunk once and dumps the
-- final frame, so the game simulates a full match in Lua and then paints the
-- last board: walls, the snake, the food, and a live score/HUD. It is plain
-- Lua using only the `term` table -- C owns the buffer, Lua owns the game.
--
-- Run from the repo root:
--   tests/t_slv_ui_demo games/wubu_snake.lua 60 20
--
-- Written against the five known slerm-Lua gaps (docs/split-plan.md):
--   1. `local function f()` does not parse -> use `local f = function()`.
--   2. Strings have no metatable -> string.rep/string.sub as globals, no s:sub().
--   3. Only \n and \t escapes -> no backslash escapes in literals at all.
--   4. Numeric `for` control var is a FLOAT -> math.floor(i) before indexing.
--   5. Explicit-iterator `for` is broken -> use `while` and ipairs only.
-- The snake body is a RING BUFFER so no table.remove/insert is needed.

local W = term.width
local H = term.height

-- Playfield: inset one cell for walls, two rows for HUD/title.
local left, right = 1, W - 2
local top, bottom = 2, H - 3
local bw, bh = right - left + 1, bottom - top + 1
local CAP = bw * bh

-- Ring buffer of segments. head is the index of the head cell.
local cells = {}
local i = 1
while i <= CAP do
    cells[i] = { x = left + (i - 1) % bw, y = top + math.floor((i - 1) / bw) }
    i = i + 1
end

local head = math.floor(CAP / 2)
local len = 4
local dir_x, dir_y = 1, 0
local score, steps = 0, 0
local MAX_STEPS = 160

-- Deterministic food sequence (no math.random needed), cycled until a cell
-- is found that is not part of the snake.
local food_seq = 0
local place_food = function()
    local tries = 0
    while tries < CAP do
        food_seq = (food_seq + 137) % CAP
        local fx = left + food_seq % bw
        local fy = top + math.floor(food_seq / bw)
        local on_snake = false
        local k = 0
        while k < len do
            local idx = head - k
            if idx < 1 then idx = idx + CAP end
            if cells[idx].x == fx and cells[idx].y == fy then on_snake = true end
            k = k + 1
        end
        if not on_snake then return { x = fx, y = fy } end
        tries = tries + 1
    end
    return nil
end

local food = place_food()

-- Greedy autopilot: step toward food on the axis with the larger gap, avoid
-- reversing and out-of-bounds. Falls back to any safe direction.
local in_snake = function(x, y, skip_head, skip_tail)
    local k = skip_head
    local stop = len - skip_tail
    while k < stop do
        local idx = head - k
        if idx < 1 then idx = idx + CAP end
        if cells[idx].x == x and cells[idx].y == y then return true end
        k = k + 1
    end
    return false
end

local choose_dir = function()
    local hx, hy = cells[head].x, cells[head].y
    local dx, dy = food.x - hx, food.y - hy
    local sx = (dx >= 0) and 1 or -1
    local sy = (dy >= 0) and 1 or -1
    local opts
    if math.abs(dx) >= math.abs(dy) then
        opts = { {sx,0}, {0,sy}, {0,-sy}, {-sx,0}, {1,0}, {-1,0}, {0,1}, {0,-1} }
    else
        opts = { {0,sy}, {sx,0}, {-sx,0}, {0,-sy}, {0,1}, {0,-1}, {1,0}, {-1,0} }
    end
    -- The tail cell vacates this tick, so entering it is safe (skip_tail=1).
    -- Reversal is allowed when the body is short enough that no segment sits
    -- behind the head -- that is what un-traps the greedy autopilot.
    local oi = 1
    while oi <= #opts do
        local ax, ay = opts[oi][1], opts[oi][2]
        local nx, ny = hx + ax, hy + ay
        local safe = (nx >= left and nx <= right and ny >= top and ny <= bottom)
            and not in_snake(nx, ny, 0, 1)
        if safe then return ax, ay end
        oi = oi + 1
    end
    return dir_x, dir_y
end

-- Simulate the whole match.
local dead = false
while steps < MAX_STEPS and not dead and food do
    local ax, ay = choose_dir()
    dir_x, dir_y = ax, ay
    local nx, ny = cells[head].x + dir_x, cells[head].y + dir_y
    if nx < left or nx > right or ny < top or ny > bottom then
        dead = true
    elseif in_snake(nx, ny, 0, 1) then
        dead = true
    else
        head = head + 1
        if head > CAP then head = 1 end
        cells[head].x, cells[head].y = nx, ny
        if nx == food.x and ny == food.y then
            score = score + 1
            len = len + 1
            if len > CAP then dead = true end
            food = place_food()
        else
            local tail = head - len
            if tail < 1 then tail = tail + CAP end
            -- tail cell stays in the ring, just outside the body window
        end
        steps = steps + 1
    end
end

-- ---- render the final frame ------------------------------------------------
term.clear()

-- Walls.
local wx = left - 1
while wx <= right + 1 do
    term.put(wx, top - 1, "-")
    term.put(wx, bottom + 1, "-")
    wx = wx + 1
end
local wy = top
while wy <= bottom do
    term.put(left - 1, wy, "|")
    term.put(right + 1, wy, "|")
    wy = wy + 1
end

-- Body, then head last so it is visible.
local k = len - 1
while k >= 0 do
    local idx = head - k
    if idx < 1 then idx = idx + CAP end
    local ch = "o"
    if k == 0 then ch = "@" end
    term.put(cells[idx].x, cells[idx].y, ch)
    k = k - 1
end
if food then term.put(food.x, food.y, "*") end

-- HUD. Keep each line inside the width so they never overlap.
term.put(0, 0, " WUBU SNAKE  self-play ")
local r = string.rep("=", W)
term.put(0, H - 1, r)
local score_s = "score " .. score
local len_s = "len " .. len
local s1 = " " .. score_s .. "  " .. len_s
term.put(1, H - 1, s1)
local st = dead and "GAME OVER" or "alive"
local steps_s = "steps " .. steps
local s2 = steps_s .. "  " .. st
if #s1 + #s2 + 3 <= W then
    term.put(W - #s2 - 1, H - 1, s2)
else
    term.put(1, H - 2, s2)
end
