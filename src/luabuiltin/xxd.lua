if #arg ~= 2 then
	error("Usage: xxd.lua input-file output-file")
end
ifile = io.open(arg[1])
if ifile == nil then
	error("Unable to open input file: ", arg[1])
end
ofile = io.open(arg[2], "w")
if ofile == nil then
	error("Unable to open output file: ", arg[2])
end

local output_name = arg[1]:gsub("%.", "_")

local num_bytes = 0
ofile:write("unsigned char " .. output_name .. "[] = {\n")
while true do
	local s = ifile:read(12)
	if s == nil then break end
	num_bytes = num_bytes + #s
	ofile:write(" ")
	string.gsub(s,"(.)", function (c) ofile:write(string.format(" 0x%02x,",string.byte(c))) end)
	ofile:write("\n")
end
ofile:write("};\n")
ofile:write("unsigned int " .. output_name .. "_len = " .. num_bytes .. ";\n")
