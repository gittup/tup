CFLAGS:insert '-Wno-missing-prototypes'
CFLAGS:insert '-DNDEBUG'
for index, file in ipairs(tup.glob('*.c')) do bang_mingwcc(file) end
