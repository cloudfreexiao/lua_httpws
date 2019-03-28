local url = {}

-- -- helpers, and also utility methods for the httpd lib
-- local function urldecode(str)
-- 	str = gsub(str, "+", " ")
-- 	str = gsub(str, "%%(%x%x)",
-- 		function(h) return char(tonumber(h,16)) end)
-- 	str = gsub (str, "\r\n", "\n")
-- 	return str
-- end

-- local function urlencode(str)
-- 	if (str) then
-- 		str = gsub(str, "\n", "\r\n")
-- 		str = gsub(str, "([^%w ])",
-- 			function (c) return format ("%%%02X", byte(c)) end)
-- 		str = gsub (str, " ", "+")
-- 	end
-- 	return str	
-- end

local function decode_func(c)
	return string.char(tonumber(c, 16))
end

local function decode(str)
	local str = str:gsub('+', ' ')
	return str:gsub("%%(..)", decode_func)
end

function url.parse(u)
	local path,query = u:match "([^?]*)%??(.*)"
	if path then
		path = decode(path)
	end
	return path, query
end

function url.parse_query(q)
	local r = {}
	for k,v in q:gmatch "(.-)=([^&]*)&?" do
		r[decode(k)] = decode(v)
	end
	return r
end

return url
