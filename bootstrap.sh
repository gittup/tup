#! /bin/sh -e
CFLAGS="-g" ./build.sh

if [ ! -d .tup ]; then
	#gdb --args ./build/tup init
	valgrind --sim-hints=fuse-compatible ./build/tup init
fi
#gdb --args ./build/tup upd
valgrind --sim-hints=fuse-compatible ./build/tup upd
echo "Build complete. If ./tup works, you can remove the 'build' directory."
