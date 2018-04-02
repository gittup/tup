#! /bin/sh -e
# This is similar to bootstrap.sh, except it uses 'tup generate' to build a
# temporary shell script in case tup needs to be built in an environment that
# doesn't support FUSE. The resulting tup binary will still require FUSE to
# operate (on those platforms where it is used).

CFLAGS="-g" ./build.sh

if [ ! -d .tup ]; then
	./build/tup init
fi
./build/tup generate --verbose build-nofuse.sh
./build-nofuse.sh
echo "Build complete. If ./tup works, you can remove the 'build' directory."
