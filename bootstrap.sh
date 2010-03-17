#! /bin/sh -e
rm -rf build
echo "  mkdir build"
mkdir -p build
mkdir -p build/ldpreload
echo "  cd build"
cd build
for i in ../src/linux/*.c ../src/tup/*.c ../src/tup/tup/main.c ../src/tup/monitor/null.c ../src/tup/colors/colors.c; do
	echo "  bootstrap CC $i"
	gcc -Os -c $i -I../src
done

echo "  bootstrap CC ../src/sqlite3/sqlite3.c"
gcc -Os -c ../src/sqlite3/sqlite3.c -DSQLITE_TEMP_STORE=2 -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION

echo "  bootstrap CC ../src/ldpreload/ldpreload.c"
gcc -Os -c ../src/ldpreload/ldpreload.c -o ldpreload/ldpreload.o -fpic -I../src

echo "  bootstrap LD.so tup-ldpreload.so"
gcc -fpic -shared -o tup-ldpreload.so ldpreload/ldpreload.o -ldl

echo "  bootstrap LD tup"
echo "const char *tup_version(void) {return \"bootstrap\";}" | gcc -x c -c - -o tup_version.o
gcc *.o -o tup -lpthread

cd ..
# We may be bootstrapping over an already-inited area.
./build/tup init || true
./build/tup upd
echo "Build complete. If ./tup works, you can remove the 'build' directory."
