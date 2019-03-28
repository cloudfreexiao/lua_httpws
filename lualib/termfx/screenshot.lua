-- sample for termfx
-- Gunnar Zötl <gz@tset.de>, 2014-2015
-- Released under the terms of the MIT license. See file LICENSE for details.

--[[ screenshot.lua
     a simple screenshot facility for termfx programs, outputs html in
     a string. Cell colors and attributes are preserved. Note, this only
     creates what is needed for a dump of the terminal/buffer's contents,
     nothing else. Just "<pre>...data...</pre>"

     use:

     	screenshot = require "screenshot"

		-- ... draw stuff ...

     	html = screenshot()

     	-- or to dump the contents of a buffer instead of the terminal:

     	html = screenshot(buf)

     	-- then write it to a file, surrounded by a html template as necessary.
--]]

local tfx = require "termfx"

local function to_html(scr)
	local fg, bg
	local res = { "<pre>" }
	for y=1, scr.h do
		for x=1, scr.w do
			local cel = scr[y][x]
			if fg ~= cel.fg or bg ~= cel.bg then
				if fg then
					res[#res+1] = "</span>"
				end
				local fgcol, fgattr = tfx.colorinfo(cel.fg % 256), math.floor(cel.fg / 256)
				local bgcol = tfx.colorinfo(cel.bg % 256)
				local style, weight = "", ""
				if fgattr % 2 == 1 then
					weight = "; font-weight: bold"
				end
				fgattr = fgattr / 2
				if fgattr % 2 == 1 then
					style = "; text-decoration: underline"
				end
				fgattr = fgattr / 2
				if fgattr % 2 == 1 then
					fgcol, bgcol = bgcol, fgcol
				end

				res[#res+1] = string.format('<span style="color: %s; background-color: %s', fgcol, bgcol)
				res[#res+1] = weight .. style
				res[#res+1] = '">'
				fg = cel.fg
				bg = cel.bg
			end
			res[#res+1] = string.format('%c', cel.ch)
		end
		res[#res+1] = "<br>"
	end
	res[#res+1] = "</span></pre>"
	return table.concat(res)
end

local function screenshot(buf)
	local w, h, getcell
	if buf then
		w, h = buf:size()
		getcell = function(x, y) return buf:getcell(x, y) end
	else
		w, h = tfx.size()
		getcell = tfx.getcell
	end

	local res = {w = w, h = h}
	for y=1, h do
		res[y] = {}
		for x=1, w do
			res[y][x] = getcell(x, y)
		end
	end

	return to_html(res)
end

return screenshot