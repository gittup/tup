for index, file in ipairs(tup.glob('*.c')) do bang_cc(file) end
for index, file in ipairs(tup.glob('*.c')) do bang_mingwcc(file) end
