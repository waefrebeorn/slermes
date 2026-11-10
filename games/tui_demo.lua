-- tui_demo.lua -- a chat-style TUI drawn entirely by Lua through term.*
--
-- This is the point of the WWW/Wua direction: the UI substrate is Lua, not
-- curses and not C#. C owns the screen buffer and hands Lua a `term` table;
-- Lua owns layout, panes, and framing.
--
-- Run:  tests/t_slv_ui_demo games/tui_demo.lua 64 20
-- (or any host that exposes the same term surface)

local W = term.width
local H = term.height

term.clear()

-- Title bar across the top, with the screen size filled in by Lua.
term.put(0, 0, " slermes ")
term.hline(9, 0, W - 9)
term.put(W - 12, 0, "Lua TUI")

-- Two panes: a message log and a state panel, sized from the terminal.
local log_w = 40
if log_w > W - 12 then log_w = W - 12 end
term.box(0, 1, log_w, H - 3, "messages")
term.box(log_w, 1, W - log_w, H - 3, "state")

-- The message pane: a few lines of a conversation.
local msgs = {
    "user:   port the TUI to Lua",
    "agent:  done - this file is the TUI",
    "user:   and the GUI?",
    "agent:  same substrate, different host",
}
for i = 1, #msgs do
    if i + 2 < H - 3 then
        term.put(2, 1 + i, msgs[i])
    end
end

-- The state pane: key/value rows, composed by Lua.
local state = {
    { "model",   "longcat-2.5" },
    { "ctx",     "12%" },
    { "session", "641099789" },
    { "lang",    "lua" },
    { "host",    "slv_ui" },
}
for i = 1, #state do
    local row = state[i]
    if i + 2 < H - 3 then
        term.put(log_w + 2, 1 + i, row[1])
        term.put(log_w + 10, 1 + i, row[2])
    end
end

-- Status line on the bottom row.
term.hline(0, H - 2, W)
term.put(0, H - 1, " /help  |  /model  |  /quit")
term.put(W - 16, H - 1, "buf " .. W .. "x" .. H)

term.refresh()
