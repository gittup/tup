#! /bin/sh -e

label=${TUP_LABEL:-bootstrap}
server=${TUP_SERVER:-fuse}
os=`uname -s`
plat_cflags=""
plat_ldflags=""
plat_files=""
if [ "$server" = "fuse" ]; then
	plat_cflags="`pkg-config fuse --cflags`"
	plat_ldflags="`pkg-config fuse --libs`"
	plat_files="$plat_files ../src/tup/server/fuse*.c ../src/tup/server/master_fork.c"
elif [ "$server" = "ldpreload" ]; then
	plat_files="../src/tup/server/depfile.c ../src/tup/server/privs.c"
else
	echo "Error: invalid TUP_SERVER \"$server\"" 1>&2
	exit 1
fi
LDFLAGS="$LDFLAGS -lm"
: ${CC:=gcc}
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
	plat_cflags="$plat_cflags -include ../src/compat/macosx.h"
	plat_cflags="$plat_cflags -DAT_SYMLINK_NOFOLLOW=0x100"
	plat_cflags="$plat_cflags -DAT_REMOVEDIR=0x200"
	CC=clang
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

for i in ../src/lua/*.c; do
	echo "  bootstrap CC $CFLAGS $i"
	$CC $CFLAGS -DLUA_USE_POSIX -c $i
done

rm luac.o
echo "  link lua"
$CC *.o -o lua $LDFLAGS
rm lua.o

cp ../src/luabuiltin/builtin.lua builtin.lua
mkdir luabuiltin
./lua ../src/luabuiltin/xxd.lua builtin.lua luabuiltin/luabuiltin.h

CFLAGS="$CFLAGS -DTUP_SERVER=\"$server\""

for i in ../src/tup/*.c ../src/tup/tup/main.c ../src/tup/monitor/null.c ../src/tup/flock/fcntl.c ../src/inih/ini.c $plat_files; do
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

if [ "$server" = "ldpreload" ]; then
	mkdir ldpreload
	cd ldpreload
	CFLAGS="$CFLAGS -fpic"
	for i in ../../src/ldpreload/*.c ../../src/tup/flock/fcntl.c; do
		echo "  bootstrap CC $CFLAGS $i"
		$CC $CFLAGS -c $i -I../../src $plat_cflags -o `basename $i`.64.o
	done
	echo "  bootstrap LD tup-ldpreload.so"
	$CC *.o -o ../tup-ldpreload.so -fpic -shared -ldl $plat_ldflags $LDFLAGS
	cd ..
fi

cd ..
