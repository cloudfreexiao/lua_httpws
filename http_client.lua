
local httpc = require "http.httpc"


local function test_https ()
    print("begin https test")
    local code, res = httpc.request('GET', 'https://baidu.com')
    print("https test:" .. code .. inspect(res))
end


local function test_http ()
    print("begin http test")
    local code, res = httpc.request('GET', 'http://localhost:15672', "/cli/rabbitmqadmin")
    print("http test:" .. code .. inspect(res))
end

return {
    test_https = test_https,
    test_http = test_http,
}


