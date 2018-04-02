#! /bin/sh -e
CFLAGS="-g" TUP_SERVER="ldpreload" ./build.sh

if [ ! -d .tup ]; then
	./build/tup init
fi
./build/tup upd
echo "Build complete. If ./tup works, you can remove the 'build' directory."
if ! grep "CONFIG_TUP_SERVER=ldpreload" tup.config > /dev/null 2>&1; then
	echo "Warning: CONFIG_TUP_SERVER=ldpreload not found in tup.config. This script builds the bootstrapped tup with the ldpreload server, but does not automatically configure the full build to use this server. You will need to do this manually by editing tup.config." 1>&2
fi
