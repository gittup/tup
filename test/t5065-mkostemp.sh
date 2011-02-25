#! /bin/sh -e

# Check support for mkostemp

. ./tup.sh
if [ ! "$tupos" = "Linux" ]; then
	echo "mkostemp only checked under linux. Skipping test."
	eotup
fi
cat > ok.c << HERE
#include <stdlib.h>
#include <fcntl.h>

int main(void)
{
	char template[] = "output.XXXXXX";
	int fd;

	fd = mkostemp(template, O_APPEND);
	if(fd < 0) {
		perror("mkostemp");
		return 1;
	}
	write(fd, "text", 4);
	close(fd);
	rename(template, "output");
	return 0;
}
HERE

if gcc ok.c -o tmp 2>/dev/null; then
	rm tmp
else
	echo "[33mmkostemp not supported on this platform? Quitting successfully.[0m" 1>&2
	exit 0
fi

cat > Tupfile << HERE
: ok.c |> gcc %f -o %o |> prog
: prog |> ./prog |> output
HERE
tup touch ok.c Tupfile
update

tup_dep_exist . './prog' . output

eotup
