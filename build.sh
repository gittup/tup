#! /bin/sh -e

label=${TUP_LABEL:-bootstrap}
os=`uname -s`
plat_cflags="`pkg-config fuse --cflags`"
plat_ldflags="`pkg-config fuse --libs`"
plat_files=""
LDFLAGS="-lm"
CC=gcc
server_files="$server_files ../src/tup/server/fuse_server.c"
server_files="$server_files ../src/tup/server/fuse_fs.c"
server_files="$server_files ../src/tup/server/master_fork.c"
case "$os" in
	Linux)
	plat_files="$plat_files ../src/compat/dummy.c"
	plat_files="$plat_files ../src/compat/utimensat_linux.c"
	;;
	SunOS)
	plat_files="$plat_files ../src/compat/dir_mutex.c"
	plat_files="$plat_files ../src/compat/mkdirat.c"
	plat_files="$plat_files ../src/compat/readlinkat.c"
	plat_ldflags="$plat_ldflags -lsocket"
	plat_cflags="$plat_cflags -D_REENTRANT"
	;;
	Darwin)
	plat_files="$plat_files ../src/compat/clearenv.c "
	plat_files="$plat_files ../src/compat/dir_mutex.c "
	plat_files="$plat_files ../src/compat/open_notify.c "
	plat_files="$plat_files ../src/compat/macosx_open.c "
	plat_files="$plat_files ../src/compat/faccessat.c"
	plat_files="$plat_files ../src/compat/fchmodat.c"
	plat_files="$plat_files ../src/compat/fchownat.c"
	plat_files="$plat_files ../src/compat/fdopendir.c"
	plat_files="$plat_files ../src/compat/fstatat.c"
	plat_files="$plat_files ../src/compat/mkdirat.c"
	plat_files="$plat_files ../src/compat/openat.c"
	plat_files="$plat_files ../src/compat/readlinkat.c"
	plat_files="$plat_files ../src/compat/renameat.c"
	plat_files="$plat_files ../src/compat/symlinkat.c"
	plat_files="$plat_files ../src/compat/unlinkat.c"
	plat_files="$plat_files ../src/compat/utimensat.c"
	plat_cflags=""
	plat_ldflags=""
	plat_cflags="$plat_cflags -include ../src/compat/macosx.h"
	plat_cflags="$plat_cflags -DAT_SYMLINK_NOFOLLOW=0x100"
	plat_cflags="$plat_cflags -DAT_REMOVEDIR=0x200"
	server_files=""
	server_files="$server_files ../src/tup/server/socketpair.c"
	server_files="$server_files ../src/tup/server/process_depfile.c"
	server_files="$server_files ../src/tup/server/master_fork.c"
	CC=clang
	LDPRELOAD=1
	;;
	FreeBSD)
	plat_files="$plat_files ../src/compat/dummy.c"
	plat_files="$plat_files ../src/compat/utimensat_linux.c"
	plat_files="$plat_files ../src/compat/clearenv.c"
	;;
esac

rm -rf build
echo "  mkdir build"
mkdir -p build
echo "  cd build"
cd build

case $LDPRELOAD in
	1)
	for i in ../src/ldpreload/ldpreload.c ../src/tup/access_event/send_event.c; do
		echo "  bootstrap CC $i [ldpreload]"
		$CC $CFLAGS -c -I../src -fPIC -include ../src/compat/macosx.h $i
	done
	echo "  bootstrap CC tup-ldpreload.so"
	$CC $CFLAGS ldpreload.o send_event.o -fPIC -o tup-ldpreload.so -dynamiclib -ldl
	;;
esac

for i in ../src/lua/*.c; do
	echo "  bootstrap CC $CFLAGS $i"
	$CC $CFLAGS -DLUA_USE_MKSTEMP -c $i
done

rm luac.o
echo "  link lua"
$CC *.o -o lua $LDFLAGS
rm lua.o

cp ../src/luabuiltin/builtin.lua builtin.lua
mkdir luabuiltin
./lua ../src/luabuiltin/xxd.lua builtin.lua luabuiltin/luabuiltin.h

for i in ../src/tup/*.c ../src/tup/tup/main.c ../src/tup/monitor/null.c ../src/tup/flock/fcntl.c ../src/inih/ini.c $server_files $plat_files; do
	echo "  bootstrap CC $CFLAGS $i"
	# Put -I. first so we find our new luabuiltin.h file, not one built
	# by a previous 'tup upd'.
	$CC $CFLAGS -c $i -I. -I../src $plat_cflags
done

echo "  bootstrap CC $CFLAGS ../src/sqlite3/sqlite3.c"
$CC $CFLAGS -c ../src/sqlite3/sqlite3.c -DSQLITE_TEMP_STORE=2 -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION $plat_cflags

echo "  bootstrap LD tup $LDFLAGS"
echo "const char *tup_version(void) {return \"$label\";}" | $CC -x c -c - -o tup_version.o
$CC *.o -o tup -lpthread $plat_ldflags $LDFLAGS

cd ..
