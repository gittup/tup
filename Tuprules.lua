tup.creategitignore()

if not CFLAGS then CFLAGS = {} end
if not CFLAGS_SPECIFIC then CFLAGS_SPECIFIC = {} end
if not LDFLAGS then LDFLAGS = {} end
if not LDFLAGS_SPECIFIC then LDFLAGS_SPECIFIC = {} end
if not MINGWCFLAGS then MINGWCFLAGS = {} end
if not MINGWLDFLAGS then MINGWLDFLAGS = {} end

-- Lua auxiliary definitions
prepare_tup_table = function(input)
	setmetatable(input, { 
		__index = function(table, key) 
			rawset(table, key, {})
			return rawget(table, key) 
		end 
	})
end
for index, specific in ipairs({CFLAGS_SPECIFIC, LDFLAGS_SPECIFIC})
do
	prepare_tup_table(specific)
end

table_concat = function(first, second)
	output = {}
	for index, value in ipairs(first) do table.insert(output, value) end
	for index, value in ipairs(second) do table.insert(output, value) end
	return output
end

CC = 'gcc'

if tup.getconfig('TUP_DEBUG') == 'y'
then
	table.insert(CFLAGS, '-g')
else
	table.insert(CFLAGS, '-Os')
end

table.insert(CFLAGS, '-W')
table.insert(CFLAGS, '-Wall')
if tup.getconfig('TUP_WERROR') == 'y'
then
	table.insert(CFLAGS, '-Werror')
end
table.insert(CFLAGS, '-Wbad-function-cast')
table.insert(CFLAGS, '-Wcast-align')
table.insert(CFLAGS, '-Wcast-qual')
table.insert(CFLAGS, '-Wchar-subscripts')
table.insert(CFLAGS, '-Wmissing-prototypes')
table.insert(CFLAGS, '-Wnested-externs')
table.insert(CFLAGS, '-Wpointer-arith')
table.insert(CFLAGS, '-Wredundant-decls')
table.insert(CFLAGS, '-Wshadow')
table.insert(CFLAGS, '-Wstrict-prototypes')
table.insert(CFLAGS, '-Wwrite-strings')
table.insert(CFLAGS, '-Wswitch-enum')
table.insert(CFLAGS, '-fno-common')
table.insert(CFLAGS, '-I' .. tup.getcwd() .. '/src')

if tup.getconfig('TUP_32_BIT') == 'y'
then
	table.insert(CFLAGS, '-m32')
	table.insert(LDFLAGS, '-m32')
end

tup.export('PKG_CONFIG_PATH')
table.insert(CFLAGS, '`pkg-config fuse --cflags`')
table.insert(CFLAGS, '`pkg-config lua5.2 --cflags`')

-- Compatibility function prototypes and include path for wrapper functions
table.insert(MINGWCFLAGS, '-include ' .. tup.getcwd() .. '/src/compat/win32/mingw.h')
table.insert(MINGWCFLAGS, '-I' .. tup.getcwd() .. '/src/compat/win32')

-- _GNU_SOURCE lets us use the %lli flag correctly
table.insert(MINGWCFLAGS, '-D_GNU_SOURCE')

-- No symlinks on windows
table.insert(MINGWCFLAGS, '-D\'S_ISLNK(a)=0\'')
table.insert(MINGWCFLAGS, '-Dlstat=stat')

-- No sig_atomic_t on windows
table.insert(MINGWCFLAGS, '-Dsig_atomic_t=int')

-- Use the same value as linux here. The logic is in src/compat/unlinkat.c
table.insert(MINGWCFLAGS, '-DAT_REMOVEDIR=0x200')

bang_cc = function(input)
	local output = string.gsub(input, '%.c', '') .. '.o'
	tup.definerule {
		inputs = {input},
		outputs = {output},
		command = '^ CC ' .. input .. '^' ..
			CC .. ' -c ' .. input .. ' -o ' .. output .. 
			' ' .. table.concat(CFLAGS, ' ') .. 
			' ' .. table.concat(CFLAGS_SPECIFIC[input], ' '),
	}
end
bang_ld = function(inputs, output)
	local inputs = table.concat(inputs, ' ')
	tup.definerule {
		inputs = inputs,
		outputs = {output},
		command = '^ LINK ' .. output .. '^' ..
			 CC .. ' ' .. inputs .. ' -o ' .. output ..
			' ' .. table.concat(LDFLAGS, ' ') ..
			' ' .. table.concat(LDFLAGS_SPECIFIC[output], ' '),
	}
end
bang_ar = function(inputs, output)
	tup.definerule {
		inputs = inputs,
		outputs = {output},
		command = '^ AR ' .. output .. '^' ..
			'ar crs ' .. output .. ' ' .. table.concat(inputs, ' '),
	}
end
bang_dot = function(input, output)
	tup.definerule {
		inputs = {input},
		outputs = {output},
		command = '^ DOT ' .. input .. '^' ..
			'dot -Efontname="Vernada, serif" -Nfontname="Vernada, serif" -Efontsize=10 -Nfontsize=10 -Tpng ' .. input .. ' > ' .. output,
	}
end
bang_cp = function(input, output)
	tup.definerule {
		inputs = {input},
		outputs = {output},
		command = '^ CP ' .. input .. ' -> ' .. output .. '^' ..
			'cp ' .. input .. ' ' .. output,
	}
end

if tup.getconfig('TUP_MINGW') == ''
then
	bang_mingwcc = function(input)
	end
else
	bang_mingwcc = function(input)
		local output = string.gsub(input, '%.c', '') .. '.omingw'
		tup.definerule {
			inputs = {input},
			outputs = {output},
			command = '^ MINGW32CC ' .. input .. '^' ..
				tup.getconfig('TUP_MINGW') .. '-gcc -c ' .. input .. ' -o ' .. output .. 
				' ' .. table.concat(CFLAGS, ' ') ..
				' ' .. table.concat(CFLAGS_SPECIFIC[input], ' ') ..
				' ' .. table.concat(MINGWCFLAGS, ' '),
		}
	end
end

TUP_MONITOR = 'null'
TUP_SUID_GROUP = 'root'
tup.dofile(tup.getconfig('TUP_PLATFORM') .. '.tup')

