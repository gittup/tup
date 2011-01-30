#! /usr/bin/perl

use strict;

my ($num_files, $num_deps, @path_names, $x, $y, %dir_names, %mains);
my @sample_paths = ("usr", "src", "linux", "mozilla", "marf", "tup", "test", "drivers", "include", "sound");

if($#ARGV < 0) {
	&usage();
}

$num_files = 100;
$num_deps = 7;

while(@ARGV) {
	if($ARGV[0] eq "-n") {
		shift;
		if($#ARGV < 0) {
			&usage();
		}
		$num_files = $ARGV[0];
		shift;
	} elsif($ARGV[0] eq "-d") {
		shift;
		if($#ARGV < 0) {
			&usage();
		}
		$num_deps = $ARGV[0];
		shift;
	} else {
		print STDERR "Unknown argument: $ARGV[0]\n";
		shift;
	}
}

mkdir "tmake";
mkdir "ttup";
open FILE, ">ttup/Tuprules.tup" or die "Can't open ttup/Tuprules.tup for write.\n";
print FILE "TEST_TOP = \$(TUP_CWD)\n";
close FILE;
for($x=0; $x<$num_files; $x++) {
	$path_names[$x] = &generate_path($x);
	my $path_name = $path_names[$x];
	if($path_name ne "") {
		system("mkdir -p tmake/$path_name");
		system("mkdir -p ttup/$path_name");
	}
	if($dir_names{$path_name} != 1) {
		$dir_names{$path_name} = 1;
		$mains{$x} = 1;
		system("cp ../testTupfile.tup ttup/$path_name/Tupfile");
	}
}

open MAKEFILE, ">tmake/Makefile" or die "Can't open Makefile for write\n";

print MAKEFILE "all:\n";
print MAKEFILE "objs :=\n";
print MAKEFILE "progs :=\n";

for($x=0; $x<$num_files; $x++) {
	my $path_name = $path_names[$x];
	print MAKEFILE "objs += $path_name$x.o\n";
	print MAKEFILE "progs += ${path_name}prog\n";
	print MAKEFILE "${path_name}prog: $path_name$x.o\n";
	open FILE, ">ttup/$path_name$x.c" or die "Can't open ttup/$path_name$x.c for write\n";
	for($y=0; $y<$num_deps; $y++) {
		my ($tmp);
		$tmp = ($x + $y) % $num_files;
		print FILE "#include \"$path_names[$tmp]$tmp.h\"\n";
	}
	print FILE "void func_$x(void) {}\n";
	if($mains{$x}) {
		print FILE "int main(void) {return 0;}\n";
	}
	close FILE;
	open FILE, ">ttup/$path_name$x.h" or die "Can't open ttup/$path_name$x.h for write\n";
	print FILE "void func_$x(void);\n";
	close FILE;
	system("cp ttup/$path_name$x.c tmake/$path_name$x.c");
	system("cp ttup/$path_name$x.h tmake/$path_name$x.h");
}

print MAKEFILE "progs := \$(sort \$(progs))\n";
print MAKEFILE "all: \$(progs)\n";
print MAKEFILE "deps := \$(objs:.o=.d)\n";
print MAKEFILE "-include \$(deps)\n";
print MAKEFILE "\$(progs): %: ; gcc -o \$@ \$^\n";
print MAKEFILE "%.o: %.c\n\tgcc -MMD -I. -c \$< -o \$@\n";
print MAKEFILE "clean: ; \@rm -rf \$(objs) \$(deps) \$(progs)\n";
print MAKEFILE ".PHONY: clean all\n";

close MAKEFILE;

sub usage
{
	print "Usage: $0 [-n num files] [-d num deps]\n";
	die;
}

sub generate_path
{
	use integer;
	my ($num, $dirindex, $levelindex, @path_components);
	my ($num_files_per_dir);
	my ($num_subdirs_per_dir);

	$num_files_per_dir = 10;
	$num_subdirs_per_dir = 5;

	$num = $_[0];
	$dirindex = $num / $num_files_per_dir;

	if ($dirindex == 0) {
		return "";
	}

	--$dirindex;

	while (1) {
		$levelindex = $dirindex % $num_subdirs_per_dir;
		unshift(@path_components, $sample_paths[$levelindex]);
		$dirindex /= $num_subdirs_per_dir;
		if ($dirindex < 1) {
			return join("/", @path_components) . "/";
		} else {
			--$dirindex;
		}
	}
}
