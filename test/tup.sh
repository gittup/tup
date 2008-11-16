tupdir=$PWD
check_empty_tupdirs()
{
	if tup flags_exists; then
		:
	else
		echo "Nodes shouldn't have flags set" 1>&2
		exit 1
	fi
}

sym_check()
{
	f=$1
	shift
	if [ ! -f $f ]; then
		echo "Object file does not exist: $f" 1>&2
		exit 1
	fi
	while [ $# -gt 0 ]; do
		sym=$1
		if echo $sym | grep '^~' > /dev/null; then
			sym=`echo $sym | sed 's/^~//'`
			if nm $f | grep $sym > /dev/null; then
				echo "'$sym' shouldn't exist in '$f'" 1>&2
				exit 1
			fi
		else
			if nm $f | grep $sym > /dev/null; then
				:
			else
				echo "No symbol '$sym' in object '$f'" 1>&2
				exit 1
			fi
		fi
		shift
	done
}

check_exist()
{
	while [ $# -gt 0 ]; do
		if [ ! -f $1 ]; then
			echo "File '$1' does not exist when it should" 1>&2
			exit 1
		fi
		shift
	done
}

check_not_exist()
{
	while [ $# -gt 0 ]; do
		if [ -f $1 ]; then
			echo "File '$1' exists when it shouldn't" 1>&2
			exit 1
		fi
		shift
	done
}

tup_object_exist()
{
	while [ $# -gt 0 ]; do
		if tup node_exists $1; then
			:
		else
			echo "Missing node $1 from .tup/db" 1>&2
			exit 1
		fi
		shift
	done
}

tup_object_no_exist()
{
	while [ $# -gt 0 ]; do
		if tup node_exists $1; then
			echo "Node $1 exists in .tup/db when it shouldn't" 1>&2
			exit 1
		fi
		shift
	done
}

tup_dep_exist()
{
	if tup link_exists "$1" "$2"; then
		:
	else
		echo "Dependency from $1 -> $2 does not exist" 1>&2
		exit 1
	fi
}

tup_dep_no_exist()
{
	if tup link_exists "$1" "$2"; then
		echo "Dependency from $1 -> $2 exists when it shouldn't" 1>&2
		exit 1
	fi
}

tup_create_exist()
{
	if tup get_flags $1 2; then
		:
	else
		echo "$1 doesn't have create flags" 1>&2
		exit 1
	fi
}

update()
{
	if tup upd; then
		:
	else
		echo "Failed to update!" 1>&2
		exit 1
	fi
	check_empty_tupdirs
}

update_fail()
{
	if tup upd 2>/dev/null; then
		echo "Expected update to fail, but didn't" 1>&2
		exit 1
	else
		:
	fi
}

check_same_link()
{
	if stat $* | grep Inode | awk 'BEGIN{x=-1} {if(x == -1) {x=$4} if(x != $4) {exit 1}}'; then
		:
	else
		echo "Files '$*' are not the same inode." 1>&2
		exit 1
	fi
}
