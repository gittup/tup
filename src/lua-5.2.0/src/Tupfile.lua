tup.dorulesfile()

luasrcs = {'lapi.c', 'lcode.c', 'lctype.c', 'ldebug.c', 'ldo.c', 'ldump.c', 'lfunc.c', 'lgc.c', 'llex.c', 'lmem.c', 'lobject.c', 'lopcodes.c', 'lparser.c', 'lstate.c', 'lstring.c', 'ltable.c', 'ltm.c', 'lundump.c', 'lvm.c', 'lzio.c', 'lauxlib.c', 'lbaselib.c', 'lbitlib.c', 'lcorolib.c', 'ldblib.c', 'liolib.c', 'lmathlib.c', 'loslib.c', 'lstrlib.c', 'ltablib.c', 'loadlib.c', 'linit.c'}

if tup.getconfig('TUP_MINGW') == ''
then
        CFLAGS:insert '-DLUA_USE_LINUX'
end
        
CFLAGS:insert '-DLUA_COMPAT_ALL'
CFLAGS:insert '-Wno-switch-enum'

if tup.getconfig('TUP_USE_SYSTEM_LUA') ~= 'y'
then
	for index, file in ipairs(luasrcs) do bang_cc(file) end
end
for index, file in ipairs(luasrcs) do bang_mingwcc(file) end
