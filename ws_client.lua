local wsc = require "websocket.wsc"

local ws_client = {}


local function connect()
    ws_client.ws = wsc()

    ws_client.ws:on_connected(function ()
        print("ws connect is suss")
        ws_client.ws:send("hello world ")
    end)

    local num = 1
    ws_client.ws:on_message(function(message, opcode)
        print("Receiving opcode:", opcode, "message:", inspect(message))
        ws_client.ws:send("hello world " .. num)
        num = num + 1
    end)
    
    local url = "ws://echo.websocket.org"
    print("Connecting to " .. url)
    return ws_client.ws:connect(url)
end

local function loop()
    ws_client.ws:loop()
end

local function shutdown()
    if ws_client.ws  then
        ws_client.ws:close()
        ws_client.ws = nil
    end
end

return {
    connect = connect,
    loop = loop,
    shutdown = shutdown,
}