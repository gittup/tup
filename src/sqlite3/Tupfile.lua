-- Remove warnings from CFLAGS
CFLAGS:insert '-w'

-- Use memory by default for temporary tables
CFLAGS:insert '-DSQLITE_TEMP_STORE=2'

-- tup does its own locking of the database
CFLAGS:insert '-DSQLITE_THREADSAFE=0'

-- No need to depend on libdl here.
CFLAGS:insert '-DSQLITE_OMIT_LOAD_EXTENSION'

if tup.getconfig('TUP_USE_SYSTEM_SQLITE') ~= 'y'
then
	for index, file in ipairs(tup.glob('*.c')) do bang_cc(file) end
end
for index, file in ipairs(tup.glob('*.c')) do bang_mingwcc(file) end
