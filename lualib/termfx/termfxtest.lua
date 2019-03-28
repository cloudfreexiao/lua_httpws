-- sample for termfx
-- Gunnar Zötl <gz@tset.de>, 2014-2015
-- Released under the terms of the MIT license. See file LICENSE for details.

package.path = "samples/?.lua;"..package.path

tfx = require "termfx"
ui = require "simpleui"
screenshot = require "screenshot"


tfx.init()
tfx.inputmode(tfx.input.ALT + tfx.input.MOUSE)
tfx.outputmode(tfx.output.COL256)

rev_keys = {}
for k, v in pairs(tfx.key) do
	if rev_keys[v] then
		rev_keys[v] = rev_keys[v] .. ','..k
	else
		rev_keys[v] = k
	end
end

function find_name(tbl, val)
	for k, v in pairs(tbl) do
		if val == v then return k end
	end
	return nil
end

function tbl_keys(tbl)
	local res = {}
	for k, v in pairs(tbl) do
		res[#res+1] = k
	end
	table.sort(res, function(i, k) return tbl[i] < tbl[k] end)
	return res
end

function pr_event(x, y, evt)
	evt = evt or {}

	tfx.attributes(tfx.color.BLUE, tfx.color.WHITE)
	tfx.printat(x, y, "Event:")
	tfx.printat(x+9, y, evt.type)
	
	tfx.attributes(tfx.color.WHITE, tfx.color.BLACK)

	if evt and evt.type then
		tfx.printat(x, y+1, "elapsed")
		tfx.printat(x+8, y+1, evt.elapsed)
	end
	
	if evt.type == "key" then
		tfx.printat(x, y+2, "mod")
		tfx.printat(x+8, y+2, evt.mod)
		tfx.printat(x, y+3, "key")
		tfx.printat(x+8, y+3, rev_keys[evt.key] or evt.key)
		tfx.printat(x, y+4, "ch")
		tfx.printat(x+8, y+4, evt.ch)
		tfx.printat(x, y+5, "char")
		tfx.printat(x+8, y+5, evt.char)
	elseif evt.type == "resize" then
		tfx.printat(x, y+2, "w")
		tfx.printat(x+8, y+2, evt.w)
		tfx.printat(x, y+3, "h")
		tfx.printat(x+8, y+3, evt.h)
	elseif evt.type == "mouse" then
		tfx.printat(x, y+2, "x")
		tfx.printat(x+8, y+2, evt.x)
		tfx.printat(x, y+3, "y")
		tfx.printat(x+8, y+3, evt.y)
		tfx.printat(x, y+4, "key")
		tfx.printat(x+8, y+4, rev_keys[evt.key] or evt.key)
	end
end

function pr_colors(x, y, w)
	tfx.attributes(tfx.color.WHITE, tfx.color.BLACK)
	tfx.printat(x, y, "BLACK", w)
	tfx.attributes(tfx.color.WHITE, tfx.color.RED)
	tfx.printat(x, y+1, "RED", w)
	tfx.attributes(tfx.color.WHITE, tfx.color.GREEN)
	tfx.printat(x, y+2, "GREEN", w)
	tfx.attributes(tfx.color.BLACK, tfx.color.YELLOW)
	tfx.printat(x, y+3, "YELLOW", w)
	tfx.attributes(tfx.color.WHITE, tfx.color.BLUE)
	tfx.printat(x, y+4, "BLUE", w)
	tfx.attributes(tfx.color.BLACK, tfx.color.MAGENTA)
	tfx.printat(x, y+5, "MAGENTA", w)
	tfx.attributes(tfx.color.BLACK, tfx.color.CYAN)
	tfx.printat(x, y+6, "CYAN", w)
	tfx.attributes(tfx.color.BLACK, tfx.color.WHITE)
	tfx.printat(x, y+7, "WHITE", w)
end

function pr_stats(x, y)
	local tw, th = tfx.size()
	local im = tfx.inputmode()
	local om = tfx.outputmode()
	
	tfx.attributes(tfx.color.WHITE, tfx.color.BLACK)
	tfx.printat(x, y, "Size:")
	tfx.printat(x+8, y, tw .. " x " .. th)
	tfx.printat(x, y+1, "Input: ")
	tfx.printat(x+8, y+1, find_name(tfx.input, im))
	tfx.printat(x, y+2, "Output: ")
	tfx.printat(x+8, y+2, find_name(tfx.output, om))
end

function pr_coltbl(x, y)
	local i = 0
	local om = tfx.outputmode()
	
	if om == tfx.output.NORMAL or om == tfx.output.COL256 then
		for j=i, i+7 do
			tfx.attributes(tfx.color.WHITE, j)
			tfx.printat(x, y+j, string.format("%02X", j), 2)
			tfx.attributes(tfx.color.WHITE, j+8)
			tfx.printat(x+3, y+j, string.format("%02X", j+8), 2)
		end
		i = 16
		x = x+6
	end
	
	if om == tfx.output.COL216 or om == tfx.output.COL256 then
		for j=0, 11 do
			for k=0, 15 do
				local col = k*12+j+i
				tfx.attributes(tfx.color.WHITE, col)
				tfx.printat(x+k*3, y+j, string.format("%02X", col), 2)
			end
		end
		x = x+48
		i=i+216
	end
	
	if om == tfx.output.GRAYSCALE or om == tfx.output.COL256 then
		for j=0, 11 do
			for k=0, 1 do
				local col = k*12+j+i
				tfx.attributes(tfx.color.WHITE, col)
				tfx.printat(x+k*3, y+j, string.format("%02X", col), 2)
			end
		end
	end
end

function pr_formats(x, y)
	local fg, bg = tfx.attributes()
	
	tfx.printat(x, y, "Normal")
	x = x + 7
	tfx.attributes(fg + tfx.format.BOLD, bg)
	tfx.printat(x, y, "Bold")
	x = x + 5
	tfx.attributes(fg + tfx.format.UNDERLINE, bg)
	tfx.printat(x, y, "Under")
	x = x + 6
	tfx.attributes(fg + tfx.format.REVERSE, bg)
	tfx.printat(x, y, "Reverse")
	
	tfx.attributes(fg, bg)
end

function blit_a_bit(x, y, w, h)
	tfx.attributes(tfx.color.WHITE, tfx.color.BLACK)
	ui.box(x, y, w, h)
	
	local buf = tfx.newbuffer(8, 6)
	buf:clear(tfx.color.WHITE, tfx.color.BLACK)
	
	local cell = tfx.newcell('#', tfx.color.YELLOW, tfx.color.GREEN)
	local eye = tfx.newcell('O', tfx.color.BLACK, tfx.color.GREEN)
	local mouth = tfx.newcell('X', tfx.color.BLACK, tfx.color.GREEN)
	
	for i=3, 6 do
		buf:setcell(i, 1, cell)
		buf:setcell(i, 6, cell)
	end
	buf:rect(1, 2, 8, 4, cell)
	
	buf:setcell(3, 3, eye)
	buf:setcell(6, 3, eye)
	buf:setcell(2, 4, mouth)
	buf:setcell(7, 4, mouth)
	buf:printat(3, 5, { mouth, mouth, mouth, mouth})
	
	tfx.blit(x, y, buf)
	tfx.blit(x+w-8, y+6, buf)
	tfx.blit(x+w-8, y+h-6, buf)
	tfx.blit(x, y+h-12, buf)
end

function select_inputmode()
	local which = ui.select("select input mode", tbl_keys(tfx.input))
	if which then tfx.inputmode(tfx.input[which]) end
end

function select_outputmode()
	local which = ui.select("select output mode", tbl_keys(tfx.output))
	if which then tfx.outputmode(tfx.output[which]) end
end

ok, err = pcall(function()

	local quit = false
	local evt
	repeat
		
		tfx.clear(tfx.color.WHITE, tfx.color.BLACK)
		tfx.printat(1, tfx.height(), "press I for input mode, O for output mode, S for screenshot, Q to quit")
		
		tfx.printat(1, 1, _VERSION)
		pr_event(1, 3, evt)
		pr_stats(25, 1)
		pr_formats(1, 10)
		pr_colors(50, 1, 10)
		pr_coltbl(1, 12)
		blit_a_bit(62, 2, 18, 21)
		
		tfx.present()
		evt = tfx.pollevent()
		
		tfx.attributes(tfx.color.WHITE, tfx.color.BLUE)
		if evt.char == "q" or evt.char == "Q" then
			quit = ui.ask("Really quit?")
			evt = {}
		elseif evt.char == "i" or evt.char == "I" then
			select_inputmode()
			evt = {}
		elseif evt.char == "o" or evt.char == "O" then
			select_outputmode()
			evt = {}
		elseif evt.char == "s" or evt.char == "S" then
			local f = io.open("screenshot.html", "w")
			if f then
				f:write("<html><body>")
				f:write(screenshot())
				f:write("</body></html>")
				f:close()
				ui.message("Screenshot saved to screenshot.html")
			else
				ui.message("Could not save screenshot.")
			end
			evt = {}
		end

	until quit

end)
tfx.shutdown()
if not ok then print("Error: "..err) end
