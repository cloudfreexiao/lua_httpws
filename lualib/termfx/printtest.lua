-- sample for termfx
-- Gunnar Zötl <gz@tset.de>, 2014-2015
-- Released under the terms of the MIT license. See file LICENSE for details.

tfx = require "termfx"

tfx.init()

ok, err = pcall(function()
---------- vvv draw here vvv ----------

tfx.outputmode(tfx.output.COL256)
tfx.clear(tfx.color.WHITE, tfx.color.BLACK)

buf = tfx.newbuffer(16, 15)
buf:clear(tfx.color.BLACK, tfx.color.WHITE)

str = "Hallodradihödel"

-- fixed length for above string, in the absence of utf8 libs
for w = 15, 1, -1 do
	tfx.printat(1, w, str, w)
	tfx.printat(tfx.width()-w+1, w, str)
	
	buf:printat(1, w, str, w)
end

tbl = {
	tfx.newcell('H', 1, 0),
	tfx.newcell('a', 2, 0),
	tfx.newcell('l', 3, 0),
	tfx.newcell('l', 4, 0),
	tfx.newcell('o', 5, 0),
	tfx.newcell('d', 6, 0),
	tfx.newcell('r', 7, 0),
	tfx.newcell('a', 8, 0),
	tfx.newcell('d', 9, 0),
	tfx.newcell('i', 10, 0),
	tfx.newcell('h', 11, 0),
	tfx.newcell('ö', 12, 0),
	tfx.newcell('d', 13, 0),
	tfx.newcell('e', 14, 0),
	tfx.newcell('l', 15, 0),
}

for w = 1, #tbl do
	tfx.printat(1, #str+w, tbl, w)
	tfx.printat(tfx.width()-w+1, #str+w, tbl, w)
	
	buf:printat(1+w, w, tbl, #tbl - w + 1)
end

tfx.blit((tfx.width() - buf:width()) / 2, (30 - buf:height()) / 2, buf)

---------- ^^^ draw here ^^^ ----------
tfx.present()
tfx.pollevent()
end)

tfx.shutdown()
if not ok then print("Error: "..err) end
