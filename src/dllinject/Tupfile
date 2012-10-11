tup.dorulesfile()
table.insert(CFLAGS, '-Wno-missing-prototypes')
table.insert(CFLAGS, '-DNDEBUG')
for index, file in ipairs(tup.glob('*.c')) do bang_mingwcc(file) end
