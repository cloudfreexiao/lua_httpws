-- sample for termfx
-- Gunnar Zötl <gz@tset.de>, 2014-2015
-- Released under the terms of the MIT license. See file LICENSE for details.

package.path = "samples/?.lua;"..package.path

tfx = require "termfx"
ui = require "simpleui"

tfx.init()

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

function select_outputmode()
	local which = ui.select("select output mode", tbl_keys(tfx.output))
	if which then tfx.outputmode(tfx.output[which]) end
end

function pr_colormap(xofs, yofs)
	xofs = xofs or 0
	if 36 - math.floor(tfx.width() / 7) < xofs then
		xofs = 36 - math.floor(tfx.width() / 7)
	elseif xofs < 0 then
		xofs = 0
	end
	local omode = tfx.outputmode()
	if omode ~= tfx.output.COL256 and omode ~= tfx.output.COL216 then
		return xofs
	end
	local r, g, b
	for r = 0, 5 do
		for g = 0, 5 do
			for b = 0, 5 do
				local col = tfx.rgb2color(r, g, b)
				local fgcol = tfx.rgb2color(5-r, 5-g, 5-b)
				local x = (g * 6 + r - xofs) * 7 + 1
				local y = b * 2 + 1 + yofs
				tfx.attributes(fgcol, col)
				local value = tfx.colorinfo(col)
				tfx.printat(x, y, r..":"..g..":"..b.."=")
				tfx.printat(x, y+1, string.sub(value, 2))
			end
		end
	end
	return xofs
end

function pr_greymap(yofs)
	local omode = tfx.outputmode()
	if omode ~= tfx.output.COL256 and omode ~= tfx.output.GRAYSCALE then
		return
	end
	local val
	for val = 0, 25 do
		local col = tfx.grey2color(val)
		local fgcol = tfx.grey2color(25-val)
		local x = 10 * (val % 8) + 1
		local y = math.floor(val / 8) + 1 + yofs
		tfx.attributes(fgcol, col)
		local value = tfx.colorinfo(col)
		tfx.printat(x, y, string.format("%02d=%s", val, string.sub(value, 2)))
	end
end

ok, err = pcall(function()

	tfx.outputmode(tfx.output.COL256)

	local quit = false
	local xofs = 0
	local evt, om
	repeat
		
		tfx.clear(tfx.color.WHITE, tfx.color.BLACK)
		tfx.printat(1, tfx.height(), "press O to select output mode, LEFT and RIGHT to scroll color table, Q to quit")
		
		tfx.printat(1, 1, _VERSION)
		om = find_name(tfx.output, tfx.outputmode())
		tfx.printat(tfx.width() - #om, 1, om)
		xofs = pr_colormap(xofs, 2)
		pr_greymap(16)
		
		tfx.present()
		evt = tfx.pollevent()
		
		tfx.attributes(tfx.color.WHITE, tfx.color.BLUE)
		if evt.char == "q" or evt.char == "Q" then
			quit = ui.ask("Really quit?")
		elseif evt.char == "o" or evt.char == "O" then
			select_outputmode()
		elseif evt.key == tfx.key.ARROW_RIGHT then
			xofs = xofs + 1
		elseif evt.key == tfx.key.ARROW_LEFT then
			xofs = xofs - 1
		end

	until quit

end)
tfx.shutdown()
if not ok then print("Error: "..err) end
