tup.dorulesfile()
tup.rule('builtin.lua', 'luac -o %o %f', '%B.luac')
tup.rule('builtin.luac', 'xxd -i %f %o', 'luabuiltin.h')
bang_cc('luaparser.c', 'luabuiltin.h')
bang_mingwcc('luaparser.c', 'luabuiltin.h')
for index, file in ipairs(tup.glob('*.c'))
do 
	if file ~= 'luaparser.c' then 
		bang_cc(file)
		bang_mingwcc(file)
	end
end
