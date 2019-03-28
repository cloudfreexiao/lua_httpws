-- sample for termfx
-- Gunnar Zötl <gz@tset.de>, 2014-2015
-- Released under the terms of the MIT license. See file LICENSE for details.

tfx = require "termfx"

tfx.init()

function makespr(s, fg, bg)
	local c = "-:+=%ZXH#"
	local spr = tfx.newbuffer(s, s/2)
	spr:attributes(fg, bg)
	for y=1, s/2 do
		for x=1, s do
			local lx, ly = (x-0.5)/s, (2*y-1)/s
			local v = math.floor((math.sin(lx*math.pi) + math.sin(ly*math.pi)) / 2 * #c)
			local ch = string.sub(c, v, v)
			spr:setcell(x, y, ch)
		end
	end
	return spr
end

tfx.outputmode(tfx.output.NORMAL)

ok, err = pcall(function()
	local sprites = {}
	local blit2screen = true
	
	for i=1, 5 do
		sprites[i] = makespr(2^(i+2), i+1, tfx.color.BLACK)
	end
	
	local snum = 1
	local spr = sprites[snum]
	local sw, sh = spr:width(), spr:height()
	local x, y = 1-sw, 1-sh
	local xo, yo = 1, 1
	local fw, fh = tfx.width(), tfx.height()
	local target = tfx.newbuffer(fw - 2, fh - 2)
	local w, h = target:width(), target:height()

	repeat
	
		if blit2screen then
			tfx.clear(tfx.color.WHITE, tfx.color.BLACK)
			tfx.blit(x, y, spr)
		else
			tfx.clear(tfx.color.WHITE, tfx.color.RED)
			target:clear(tfx.color.WHITE, tfx.color.BLACK)
			target:blit(x, y, spr)
			tfx.blit(2, 2, target)
		end

		x = x + xo
		if x > w or x < 1-sw then xo = -xo end
		y = y + yo
		if y > h or y < 1-sh then yo = -yo end

		tfx.printat(1, tfx.height(), "print 1.."..#sprites.." for sprite size, t to toggle blit to screen or buffer, q to quit")
		tfx.printat(1, 1, "Current size: "..snum.." ("..spr:width().."x"..spr:height()..")")

		tfx.present()
		evt = tfx.pollevent(333)
		snum = evt and tonumber(evt.char) or snum
		if snum >= 1 and snum <= #sprites then
			spr = sprites[snum]
			sw, sh = spr:width(), spr:height()
			if x < 1-sw then x = 1-sw xo = 1 end
			if y < 1-sh then y = 1-sh yo = 1 end
		end
		
		if evt and evt.char == 't' then
			blit2screen = not blit2screen
		end
		
	until evt and evt.type == "key" and evt.char == "q"
end)

tfx.shutdown()
if not ok then print("Error: "..err) end
