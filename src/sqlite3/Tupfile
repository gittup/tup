tup.dorulesfile()

-- Remove warnings from CFLAGS
table.insert(CFLAGS, '-w')

-- Use memory by default for temporary tables
table.insert(CFLAGS, '-DSQLITE_TEMP_STORE=2')

-- tup does its own locking of the database
table.insert(CFLAGS, '-DSQLITE_THREADSAFE=0')

-- No need to depend on libdl here.
table.insert(CFLAGS, '-DSQLITE_OMIT_LOAD_EXTENSION')

if tup.getconfig('TUP_USE_SYSTEM_SQLITE') ~= 'y'
then
	for index, file in ipairs(tup.glob('*.c')) do bang_cc(file) end
end
for index, file in ipairs(tup.glob('*.c')) do bang_mingwcc(file) end
