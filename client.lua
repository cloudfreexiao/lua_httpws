#!/usr/bin/env lua53
assert(_VERSION == "Lua 5.3")

package.path  = "lualib/?.lua;./?.lua;lualib/termfx/?.lua;" .. package.path
package.cpath = "luaclib/?.so;"

inspect = require "inspect"

local tfx = require "termfx"
local ui = require "simpleui"

local ws_client = require "ws_client"
local httpc = require "http.httpc"

-- print("begin https test")
-- local code, res = httpc.request('GET', 'https://baidu.com')
-- print("https test:" .. code .. inspect(res))

-- print("begin http test")
-- local code, res = httpc.request('GET', 'http://localhost:15672', "/cli/rabbitmqadmin")
-- print("http test:" .. code .. inspect(res))

-- local sep = '\r\n'
-- local buf = "dsgdgf" .."\r\n" .. "ggg1111"
-- print('bb:', buf)

-- while not buf:find(sep, 1, true) do
-- end
-- local b, e, c = buf:find(sep, 1, true)
-- print('ff:', b, e, buf:sub(1, b))
-- print('cc:', c)

assert(ws_client.connect())
ws_client.loop()

-- ok, err = pcall(function()
--     tfx.init()
--     tfx.inputmode(tfx.input.ALT + tfx.input.MOUSE)
--     tfx.outputmode(tfx.output.COL256)
    
--     ws_client.connect()
-- end)

-- local function do_tfx_evt(evt)
--     if evt.char == "q" or evt.char == "Q" then
--         return ui.ask("Really quit?")
--     elseif evt.char == 'h' or evt.char == "H" then
--         ws_client.send("hello world")
--     end

--     if evt.key == tfx.key.CTRL_C then
--         return true
--     end

--     return false
-- end

-- ok, err = pcall(function()
--     local quit = false
--     local evt = {}

--     repeat
--         tfx.clear(tfx.color.WHITE, tfx.color.BLACK)
--         tfx.printat(1, tfx.height(), "press Q to quit")
        
--         tfx.present()
--         evt = tfx.pollevent()
--         ws_client.update()
        
--         tfx.attributes(tfx.color.WHITE, tfx.color.BLUE)

--         quit = do_tfx_evt(evt)
--     until quit
-- end)
-- ws_client.shutdown()
-- tfx.shutdown()
-- if not ok then print("Error: "..err) end

-- local startTime = os.time()
-- local endTime = startTime+10
-- while os.time() <= endTime do
--     ws_client.update()
-- end

-- ws_client.shutdown()
