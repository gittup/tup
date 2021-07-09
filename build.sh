#! /bin/sh -e

label=${TUP_LABEL:-bootstrap}
os=`uname -s`
default_server=fuse
case "$os" in
	Linux)
		default_server=fuse3
	;;
esac

server=${TUP_SERVER:-$default_server}
plat_cflags="-Os"
plat_ldflags=""
plat_files=""
if [ "$server" = "fuse" ]; then
	plat_cflags="`pkg-config fuse --cflags`"
	plat_ldflags="`pkg-config fuse --libs`"
	plat_files="$plat_files ../src/tup/server/fuse*.c ../src/tup/server/master_fork.c ../src/tup/server/symlink.c"
elif [ "$server" = "fuse3" ]; then
	plat_cflags="`pkg-config fuse3 --cflags` -DFUSE3 -D_FILE_OFFSET_BITS=64"
	plat_ldflags="`pkg-config fuse3 --libs`"
	plat_files="$plat_files ../src/tup/server/fuse*.c ../src/tup/server/master_fork.c ../src/tup/server/symlink.c"
elif [ "$server" = "ldpreload" ]; then
	plat_files="../src/tup/server/depfile.c ../src/tup/server/privs.c ../src/tup/server/symlink.c"
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
	plat_files="$plat_files ../src/compat/dummy.c"
	plat_files="$plat_files ../src/compat/clearenv.c "
	plat_cflags="$plat_cflags -include ../src/compat/macosx.h"
	CC=clang
	;;
	FreeBSD)
	plat_files="$plat_files ../src/compat/dummy.c"
	plat_files="$plat_files ../src/compat/utimensat_linux.c"
	plat_files="$plat_files ../src/compat/clearenv.c"
	;;
	NetBSD)
	plat_files="$plat_files ../src/compat/dummy.c"
	plat_files="$plat_files ../src/compat/clearenv.c"
	plat_cflags="$plat_cflags -include ../src/compat/netbsd.h"
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
CFLAGS="$CFLAGS -DHAVE_CONFIG_H"

for i in ../src/tup/*.c ../src/tup/tup/main.c ../src/tup/monitor/null.c ../src/tup/flock/fcntl.c ../src/inih/ini.c ../src/pcre/*.c $plat_files; do
	echo "  bootstrap CC $CFLAGS $i"
	# Put -I. first so we find our new luabuiltin.h file, not one built
	# by a previous invocation of 'tup'.
	$CC $CFLAGS -c $i -I. -I../src -I../src/pcre $plat_cflags
done

echo "  bootstrap CC $CFLAGS ../src/sqlite3/sqlite3.c"
$CC $CFLAGS -c ../src/sqlite3/sqlite3.c -DSQLITE_TEMP_STORE=2 -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION $plat_cflags

echo "  bootstrap LD tup $LDFLAGS"
objs="$(echo *.o)"
../src/tup/link.sh "$CC" "$CFLAGS -I../src" "-lpthread $plat_ldflags $LDFLAGS" "tup" "tup-version.o" "$objs" "$label"

if [ "$server" = "ldpreload" ]; then
	mkdir ldpreload
	cd ldpreload
	CFLAGS="$CFLAGS -fpic"
	for i in ../../src/ldpreload/*.c ../../src/tup/flock/fcntl.c ../../src/tup/ccache.c; do
		echo "  bootstrap CC $CFLAGS $i"
		$CC $CFLAGS -c $i -I../../src $plat_cflags -o `basename $i`.64.o -pthread
	done
	echo "  bootstrap LD tup-ldpreload.so"
	$CC *.o -o ../tup-ldpreload.so -fpic -shared -ldl $plat_ldflags $LDFLAGS -pthread
	cd ..
fi

cd ..
