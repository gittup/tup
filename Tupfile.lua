client_objs = tup.var {}
client_objs:insert 'src/tup/vardict.o'
client_objs:insert 'src/tup/send_event.o'
client_objs:insert 'src/tup/flock/fcntl.o'
bang_ar(client_objs, 'libtup_client.a')
bang_cp('src/tup/vardict.h', 'tup_client.h')

srcs = tup.var(tup.glob('src/tup/*.o'))
srcs = srcs .. tup.glob('src/tup/monitor/*.o')
srcs = srcs .. tup.glob('src/tup/flock/*.o')
srcs = srcs .. tup.glob('src/tup/server/*.o')
if tup.getconfig('TUP_USE_SYSTEM_SQLITE') == 'y'
then
	LDFLAGS:insert '-lsqlite3'
else
	srcs = srcs .. tup.glob('src/sqlite3/*.o')
end
if tup.getconfig('TUP_USE_SYSTEM_LUA') == 'y'
then
	LDFLAGS:insert '`pkg-config lua5.2 --libs`'
else
	srcs = srcs .. tup.glob('src/lua-5.2.0/src/*.o')
	LDFLAGS:insert '-lm'
end
srcs = srcs .. tup.glob('src/inih/*.o')
srcs = srcs .. tup.glob('src/compat/*.o')
bang_ar(srcs, 'liba')

suid = ''
if tup.getconfig('TUP_SUDO_SUID') == 'y'
then
	suid = '; chown root:' .. TUP_SUID_GROUP .. ' tup; chmod u+s tup'
end

LDFLAGS:insert '`pkg-config fuse --libs`'

tup.rule({'src/tup/tup/main.o', 'liba'}, '^ LINK tup^ version=`git describe`; echo "const char *tup_version(void) {return \\\"$version\\\";}" | $(CC) -x c -c - -o tup-version.o $(CFLAGS) -Wno-missing-prototypes; $(CC) %f tup-version.o -o tup -lpthread $(LDFLAGS) $(suid)',{'tup', 'tup-version.o'})

if tup.getconfig('TUP_MINGW') ~= ''
then
	tup.rule(tup.glob('src/dllinject/*.omingw'), '^ MINGW32LINK %o ^ @(TUP_MINGW)-gcc -shared %f -lws2_32 -lpsapi -lshlwapi -o %o', 'tup-dllinject.dll')

	mingwsrcs = tup.var(tup.glob('src/tup/*.omingw'))
	mingwsrcs = mingwsrcs .. tup.glob('src/tup/monitor/*.omingw')
	mingwsrcs = mingwsrcs .. tup.glob('src/tup/tup/*.omingw')
	mingwsrcs = mingwsrcs .. tup.glob('src/tup/flock/*.omingw')
	mingwsrcs = mingwsrcs .. tup.glob('src/tup/server/*.omingw')
	mingwsrcs = mingwsrcs .. tup.glob('src/inih/*.omingw')
	mingwsrcs = mingwsrcs .. tup.glob('src/sqlite3/*.omingw')
	mingwsrcs = mingwsrcs .. tup.glob('src/lua-5.2.0/src/*.omingw')
	mingwsrcs = mingwsrcs .. tup.glob('src/compat/*.omingw')
	mingwsrcs = mingwsrcs .. tup.glob('src/compat/win32/*.omingw')
	MINGWLDFLAGS:insert '-lm'
	MINGWLDFLAGS:insert '-Wl,--wrap=open'
	MINGWLDFLAGS:insert '-Wl,--wrap=close'
	MINGWLDFLAGS:insert '-Wl,--wrap=tmpfile'
	MINGWLDFLAGS:insert '-Wl,--wrap=dup'
	MINGWLDFLAGS:insert '-Wl,--wrap=__mingw_vprintf'
	MINGWLDFLAGS:insert '-Wl,--wrap=__mingw_vfprintf'

	tup.rule(mingwsrcs .. {'tup-dllinject.dll'}, '^ MINGW32LINK exe^ version=`git describe`; echo "const char *tup_version(void) {return \\\"$version\\\";}" | @(TUP_MINGW)-gcc -x c -c - -o tup-version.omingw; @(TUP_MINGW)-gcc %f tup-version.omingw $(MINGWLDFLAGS) -o exe', {'exe', 'tup-version.omingw'})
end

