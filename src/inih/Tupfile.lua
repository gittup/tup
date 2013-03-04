tup.dorulesfile()

-- Has some warnings about casting away const
CFLAGS:insert '-Wno-cast-qual'

-- Don't need multiline support - we just set some ints.
CFLAGS:insert '-DINI_ALLOW_MULTILINE=0'

for index, file in ipairs(tup.glob('*.c'))
do
	bang_cc(file)
	bang_mingwcc(file)
end

