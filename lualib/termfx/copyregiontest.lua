-- sample for termfx
-- Gunnar Zötl <gz@tset.de>, 2014-2015
-- Released under the terms of the MIT license. See file LICENSE for details.

package.path = "samples/?.lua;"..package.path

tfx = require "termfx"
ui = require "simpleui"

tfx.init()

local w, h = 16, 12

ok, err = pcall(function()

	tfx.outputmode(tfx.output.COL256)

	local sx = math.floor(tfx.width() / 2) - 4
	local sy = math.floor(tfx.height() / 2) - 4
	local tx, ty = sx, sy
	local x, y

	local quit = false
	local evt
	repeat
		tfx.attributes(tfx.color.WHITE, tfx.color.BLACK)
		tfx.clear()

		for x = 1, w do
			for y = 1, h do
				tfx.setcell(sx - 1 + x, sy - 1 + y, string.format("%X", math.max(x, y) - 1), tfx.color.RED, tfx.color.BLUE)
			end
		end

		tfx.copyregion(tx, ty, sx, sy, w, h)

		tfx.present()
		evt = tfx.pollevent()
		if evt.char == "q" or evt.char == "Q" then
			tfx.attributes(tfx.color.WHITE, tfx.color.BLUE)
			quit = ui.ask("Really quit?")
		elseif evt.key == tfx.key.ARROW_LEFT and tx > 1 - w then
			tx = tx - 1
		elseif evt.key == tfx.key.ARROW_RIGHT and tx <= tfx.width() then
			tx = tx + 1
		elseif evt.key == tfx.key.ARROW_UP and ty > 1 - h then
			ty = ty - 1
		elseif evt.key == tfx.key.ARROW_DOWN and ty <= tfx.height() then
			ty = ty + 1
		end
	until quit

end)
tfx.shutdown()
if not ok then print("Error: "..err) end
