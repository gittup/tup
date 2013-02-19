tup.dorulesfile()

-- Has some warnings about casting away const
table.insert(CFLAGS, '-Wno-cast-qual')

-- Don't need multiline support - we just set some ints.
table.insert(CFLAGS, '-DINI_ALLOW_MULTILINE=0')

for index, file in ipairs(tup.glob('*.c'))
do
	bang_cc(file)
	bang_mingwcc(file)
end

