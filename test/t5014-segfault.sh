#! /bin/sh -e

# Make sure a command that segfaults actually fails (and doesn't hose the DAG).

. ./tup.sh
cat > Tupfile << HERE
: segfault.c |> gcc %f -o %o |> tup_t5014_segfault
: tup_t5014_segfault |> ./%f |>
HERE

cat > segfault.c << HERE
int main(void)
{
	int *x = 0;
	*x = 5;
	return 0;
}
HERE
tup touch segfault.c Tupfile
update_fail_msg "Segmentation fault"
tup_dep_exist . segfault.c . 'gcc segfault.c -o tup_t5014_segfault'
tup_dep_exist . 'gcc segfault.c -o tup_t5014_segfault' . tup_t5014_segfault
tup_dep_exist . tup_t5014_segfault . './tup_t5014_segfault'

# Make sure the command runs and fails again.
update_fail_msg "Segmentation fault"

eotup
