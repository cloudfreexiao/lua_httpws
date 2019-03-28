local lsocket = require "lsocket"

local sockethelper = {}
local socket_error = setmetatable({} , { __tostring = function() return "[Socket Error]" end })

sockethelper.socket_error = socket_error

sockethelper.readall = function (fd)
	-- local selects = {fd}
    local reply = ""
	repeat
		local str, err = fd:recv()
		if str then
			reply = reply .. str
		elseif err then
			error(err)
        end
	until not str
	return reply
end

local readbytes = function (fd, sz)
	return sockethelper.readall(fd)
    -- if sz == nil then
        
    -- end

	-- -- local selects = {fd}
    -- local reply = ""

	-- while #reply < sz do
	-- 	local str, err = fd:recv()
    --     if str then
	-- 		reply = reply .. str
	-- 	elseif err then
	-- 		error(err)
    --     end
    -- end
    -- return string.sub(reply, 1, sz)
end

local writebytes = function(fd, data)
	local len = #data
	local selects = {fd}
	local sent = 1
	local error = nil

	while sent <= len do
		local rd, wt = lsocket.select(nil, selects)
		local chunk = data:sub(sent)
		local nbytes, err = fd:send(chunk)
		if err then
			error = err
			break
		end

		if nbytes == false then
			lsocket.select(nil, selects, 0.1)
		else
			sent = sent + nbytes
		end
	end
	if error then
		return false, error
	end
	return true, nil
end

local function preread(fd, str)
	return function (sz)
		if str then
			if sz == #str or sz == nil then
				local ret = str
				str = nil
				return ret
			else
				if sz < #str then
					local ret = str:sub(1,sz)
					str = str:sub(sz + 1)
					return ret
				else
					sz = sz - #str
					local ret = readbytes(fd, sz)
					if ret then
						return str .. ret
					else
						error(socket_error)
					end
				end
			end
		else
			local ret = readbytes(fd, sz)
			if ret then
				return ret
			else
				error(socket_error)
			end
		end
	end
end

function sockethelper.readfunc(fd, pre)
	if pre then
		return preread(fd, pre)
	end
    return function (sz)
		local ret = readbytes(fd, sz)
		if ret then
			return ret
		else
			error(socket_error)
		end
	end
end

function sockethelper.writefunc(fd)
	return function(content)
		local ok = writebytes(fd, content)
		if not ok then
			error(socket_error)
		end
		return ok
	end
end

function sockethelper.connect(host, port)
	if type(host) == "table" then
		host = host[1].addr
	end

	local sock, err = lsocket.connect(host, port)
	if not sock then
		error(err)
	end

	-- wait for connect() to succeed or fail
	lsocket.select(nil, {sock})
	local ok, err = sock:status()
	if not ok then
		error(err)
	end

	return sock
end

function sockethelper.close(fd)
	fd:close()
end

function sockethelper.shutdown(fd)
	fd:close()
end

function sockethelper.resolve(hostname)
    return lsocket.resolve(hostname)
end

function sockethelper.select(r, w, t)
	return lsocket.select(r, w, t)
end

return sockethelper
