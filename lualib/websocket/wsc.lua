local frame = require'websocket.frame'
local handshake = require'websocket.handshake'
local tools = require'websocket.tools'
local socket = require "http.sockethelper"
local internal = require "http.internal"

local tinsert = table.insert
local tconcat = table.concat

local sock_receive = function(self)
    return self.interface.read()
end

local sock_send = function(self, content)
  local write = self.interface.write
  return write(content)
end

local receive = function(self)
  if self.state ~= 'OPEN' and not self.is_closing then
    return nil,nil,false,1006,'wrong state'
  end

  local first_opcode
  local frames
  local bytes = 2
  local encoded = ''

  local clean = function(was_clean,code,reason)
    self.sock:close()
    if self.interface.close then
      self.interface.close()
    end

    self.state = 'CLOSED'
    if self.on_close then
      self:on_close()
    end
    return nil, nil, was_clean, code, reason or 'closed'
  end

  while true do
    local chunk = sock_receive(self, bytes)
    if not chunk then
      return clean(false,1006, socket.socket_error)
    end
    encoded = encoded..chunk
    local decoded,fin,opcode,_,masked = frame.decode(encoded)
    if not self.is_server and masked then
      return clean(false,1006,'Websocket receive failed: frame was not masked')
    end
    if decoded then
      if opcode == frame.CLOSE then
        if not self.is_closing then
          local code,reason = frame.decode_close(decoded)
          local msg = frame.encode_close(code)
          local encoded = frame.encode(msg,frame.CLOSE,not self.is_server)
          local ok,err = sock_send(self, encoded)
          if ok then
            return clean(true,code,reason)
          else
            return clean(false,code,err)
          end
        else
          return decoded,opcode
        end
      end
      if not first_opcode then
        first_opcode = opcode
      end
      if not fin then
        if not frames then
          frames = {}
        elseif opcode ~= frame.CONTINUATION then
          return clean(false,1002,'protocol error')
        end
        bytes = 2
        encoded = ''
        tinsert(frames,decoded)
      elseif not frames then
        return decoded,first_opcode
      else
        tinsert(frames,decoded)
        return tconcat(frames),first_opcode
      end
    else
      assert(type(fin) == 'number' and fin > 0)
      bytes = fin
    end
  end
  assert(false,'never reach here')
end

local send = function(self,data,opcode)
  if self.state ~= 'OPEN' then
    return nil, false, 1006, 'wrong state'
  end

  local encoded = frame.encode(data,opcode or frame.TEXT,not self.is_server)
  local ok,err = sock_send(self, encoded)
  if not ok then
    return nil,self:close(1006,err)
  end
  return true
end

local close = function(self,code,reason)
  if self.state ~= 'OPEN' then
    return false,1006,'wrong state'
  end
  if self.state == 'CLOSED' then
    return false,1006,'wrong state'
  end

  local msg = frame.encode_close(code or 1000,reason)
  local encoded = frame.encode(msg,frame.CLOSE,not self.is_server)

  local ok,err = sock_send(self, encoded)
  local was_clean = false
  local code = 1005
  local reason = ''
  if ok then
    self.is_closing = true
    local rmsg,opcode = self:receive()
    if rmsg and opcode == frame.CLOSE then
      code,reason = frame.decode_close(rmsg)
      was_clean = true
    end
  else
    reason = err
  end

  self.sock.close()
  if self.interface.close then
    self.interface.close()
  end

  if self.on_close then
    self:on_close()
  end

  self.state = 'CLOSED'
  return was_clean,code,reason or ''
end

local SSLCTX_CLIENT = nil
local function gen_interface(protocol, fd)
  if protocol == "ws" then
		return {
			init = nil,
			close = nil,
			read = socket.readfunc(fd),
			write = socket.writefunc(fd),
		}
  elseif protocol == "wss" then
		local tls = require "http.tlshelper"
		SSLCTX_CLIENT = SSLCTX_CLIENT or tls.newctx()
		local tls_ctx = tls.newtls("client", SSLCTX_CLIENT)
		return {
			init = tls.init_requestfunc(fd, tls_ctx),
			close = tls.closefunc(tls_ctx),
			read = tls.readfunc(fd, tls_ctx),
			write = tls.writefunc(fd, tls_ctx),
		}
	else
		error(string.format("Invalid protocol: %s", protocol))
	end
end

local connect = function(self, ws_url)
  if self.state ~= 'CLOSED' then
    return nil,'wrong state',nil
  end
  
  local protocol, hostname, port, uri = tools.parse_url(ws_url)
  local host = hostname
  if not hostname:match(".*%d+$") then
    host = socket.resolve(hostname)
  end
  print('host:', inspect(host))
  local fd, err = socket.connect(host, port)
	if not fd then
		error(string.format("%s connect error host:%s, port:%s, err:%s", protocol, host, port, err))
		return
  end

  self.sock = fd

  self.interface = gen_interface(protocol, self.sock)
	if self.interface.init then
		self.interface.init()
  end

  local key = tools.generate_key()
  local req = handshake.upgrade_request {
    key = key,
    host = hostname,
    port = port,
    protocols = {''}, --TODO:增加子协议参数
    uri = uri
  }

  local ok, err = sock_send(self, req)
  if not ok then
    error(err)
    return nil,err,nil
  end

  local resp = {}
	local body = internal.recvheader(self.interface.read, resp, "")
	if not body then
		error(socket.socket_error)
  end
  
  local response = table.concat(resp,'\r\n')
  print("resp:", inspect(resp))

  local headers = handshake.http_headers(response)

  local expected_accept = handshake.sec_websocket_accept(key)
  print('expected_accept:', expected_accept)
  if headers['sec-websocket-accept'] ~= expected_accept then
    local msg = 'Websocket Handshake failed: Invalid sec-websocket-accept (expected %s got %s)'
    return nil,msg:format(expected_accept,headers['sec-websocket-accept'] or 'nil'),headers
  end
  self.state = 'OPEN'
  self.on_connected_fn()
  return true, headers['sec-websocket-protocol'],headers
end

local on_message = function(self, fn)
  self.on_message_fn = fn
end
local on_connected = function(self, fn)
  self.on_connected_fn = fn
end

local loop = function (self)
  while self.sock and self.state == "OPEN" do
    local decoded, opcode, was_clean, code, reason = self:receive()
    if not decoded then
      print("was_clean", was_clean, "code", code, "reason", reason)
      return 
    end
    self.on_message_fn(decoded, opcode)
  end
end

local extend = function(obj)
  if not obj.state then
    obj.state = 'CLOSED'
  end

  obj.receive = receive
  obj.send = send
  obj.close = close
  obj.connect = connect
  obj.on_message = on_message
  obj.on_connected = on_connected
  obj.loop = loop

  return obj
end

local new = function ()
  local self = {}
  self = extend(self)
  return self
end

return new
