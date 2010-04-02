#! /bin/sh -e
os=`uname -s`
plat_cflags=""
plat_ldflags=""
plat_files=""
plat_preloadcflags="-fpic"
plat_preloadldflags="-shared"
case "$os" in
	SunOS)
	plat_ldflags="$plat_ldflags -lsocket"
	plat_files="$plat_files ../src/compat/dir_mutex.c"
	plat_files="$plat_files ../src/compat/readlinkat.c"
	;;
	Darwin)
	plat_files="$plat_files ../src/compat/dir_mutex.c "
	plat_files="$plat_files ../src/compat/fstatat.c"
	plat_files="$plat_files ../src/compat/openat.c"
	plat_files="$plat_files ../src/compat/readlinkat.c"
	plat_files="$plat_files ../src/compat/unlinkat.c"
	plat_cflags="-DAT_SYMLINK_NOFOLLOW=0x100"
	plat_preloadcflags="-DAT_FDCWD=-100 -fPIC"
	plat_preloadldflags="-dynamiclib"
	;;
esac

rm -rf build
echo "  mkdir build"
mkdir -p build
mkdir -p build/ldpreload
echo "  cd build"
cd build
for i in ../src/linux/*.c ../src/tup/*.c ../src/tup/tup/main.c ../src/tup/monitor/null.c ../src/tup/colors/colors.c $plat_files; do
	echo "  bootstrap CC (unoptimized) $i"
	gcc -c $i -I../src $plat_cflags
done

echo "  bootstrap CC (unoptimized) ../src/sqlite3/sqlite3.c"
gcc -c ../src/sqlite3/sqlite3.c -DSQLITE_TEMP_STORE=2 -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION

echo "  bootstrap CC (unoptimized) ../src/ldpreload/ldpreload.c"
gcc -c ../src/ldpreload/ldpreload.c -o ldpreload/ldpreload.o $plat_preloadcflags -I../src

echo "  bootstrap LD.so tup-ldpreload.so"
gcc $plat_preloadcflags $plat_preloadldflags -o tup-ldpreload.so ldpreload/ldpreload.o -ldl $plat_ldflags

echo "  bootstrap LD tup"
echo "const char *tup_version(void) {return \"bootstrap\";}" | gcc -x c -c - -o tup_version.o
gcc *.o -o tup -lpthread $plat_ldflags

cd ..
# We may be bootstrapping over an already-inited area.
./build/tup init || true
./build/tup upd
echo "Build complete. If ./tup works, you can remove the 'build' directory."
