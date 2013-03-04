tup.creategitignore()

CFLAGS = tup.var{}
LDFLAGS = tup.var{}
MINGWCFLAGS = tup.var{}
MINGWLDFLAGS = tup.var{}

-- Lua auxiliary definitions
CC = 'gcc'

if tup.getconfig('TUP_DEBUG') == 'y'
then
	CFLAGS:insert '-g'
else
	CFLAGS:insert '-Os'
end

CFLAGS:insert '-W'
CFLAGS:insert '-Wall'
if tup.getconfig('TUP_WERROR') == 'y'
then
	CFLAGS:insert '-Werror'
end
CFLAGS:insert '-Wbad-function-cast'
CFLAGS:insert '-Wcast-align'
CFLAGS:insert '-Wcast-qual'
CFLAGS:insert '-Wchar-subscripts'
CFLAGS:insert '-Wmissing-prototypes'
CFLAGS:insert '-Wnested-externs'
CFLAGS:insert '-Wpointer-arith'
CFLAGS:insert '-Wredundant-decls'
CFLAGS:insert '-Wshadow'
CFLAGS:insert '-Wstrict-prototypes'
CFLAGS:insert '-Wwrite-strings'
CFLAGS:insert '-Wswitch-enum'
CFLAGS:insert '-fno-common'
CFLAGS:insert('-I' .. tup.getcwd() .. '/src')

if tup.getconfig('TUP_32_BIT') == 'y'
then
	CFLAGS:insert '-m32'
	LDFLAGS:insert '-m32'
end

tup.export('PKG_CONFIG_PATH')
CFLAGS:insert '`pkg-config fuse --cflags`'
CFLAGS:insert '`pkg-config lua5.2 --cflags`'

-- Compatibility function prototypes and include path for wrapper functions
MINGWCFLAGS:insert('-include ' .. tup.getcwd() .. '/src/compat/win32/mingw.h')
MINGWCFLAGS:insert('-I' .. tup.getcwd() .. '/src/compat/win32')

-- _GNU_SOURCE lets us use the %lli flag correctly
MINGWCFLAGS:insert '-D_GNU_SOURCE'

-- No symlinks on windows
MINGWCFLAGS:insert '-D\'S_ISLNK(a)=0\''
MINGWCFLAGS:insert '-Dlstat=stat'

-- No sig_atomic_t on windows
MINGWCFLAGS:insert '-Dsig_atomic_t=int'

-- Use the same value as linux here. The logic is in src/compat/unlinkat.c
MINGWCFLAGS:insert '-DAT_REMOVEDIR=0x200'

bang_cc = function(input, extradependency)
	local output = string.gsub(input, '%.c', '') .. '.o'
	local inputs = {input}
	if extradependency then table.insert(inputs, extradependency) end
	tup.definerule {
		inputs = inputs,
		outputs = {output},
		command = '^ CC ' .. input .. '^' ..
			CC .. ' -c ' .. input .. ' -o ' .. output .. 
			' ' .. table.concat(CFLAGS, ' ') .. 
			' ' .. table.concat(CFLAGS[input], ' '),
	}
end

bang_ld = function(inputs, output) tup.rule(inputs, '^ LINK %o ^ $(CC) %f -o %o $(LDFLAGS) ' .. LDFLAGS[output], output) end

bang_ar = function(inputs, output) tup.rule(inputs, '^ AR %o ^ ar crs %o %f', output) end

bang_dot = function(input, output) tup.rule(input, '^ DOT %f ^ dot -Efontname="Vernada, serif" -Nfontname="Vernada, serif" -Efontsize=10 -Nfontsize=10 -Tpng %f > %o', output) end

bang_cp = function(input, output) tup.rule(input, '^ CP %f -> %o^ cp %f %o', output) end

if tup.getconfig('TUP_MINGW') == ''
then
	bang_mingwcc = function(input)
	end
else
	bang_mingwcc = function(input, extradependency)
		local inputs = {input}
		if extradependency then table.insert(inputs, extradependency) end
		local output = base(input) .. '.omingw'
		tup.definerule {
			inputs = inputs, 
			command = '^ MINGW32CC ' .. input .. '^ ' .. tup.getconfig('TUP_MINGW') .. '-gcc ' .. 
				'-c ' .. input .. ' -o ' .. output .. ' ' ..
				' ' .. CFLAGS ..
				' ' .. CFLAGS[input] ..
				' ' .. MINGWCFLAGS, 
			outputs = {output}
		}
	end
end

TUP_MONITOR = 'null'
TUP_SUID_GROUP = 'root'
tup.dofile(tup.getconfig('TUP_PLATFORM') .. '.tup')

