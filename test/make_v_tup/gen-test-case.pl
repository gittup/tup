#! /usr/bin/perl

use strict;

my ($num_files, $num_deps, $hier_depth_min, $hier_depth_max, %path_names, $x, $y, %dir_names, %mains);
my @sample_paths = ("usr", "src", "linux", "mozilla", "marf", "tup", "test", "drivers", "include", "sound");

if($#ARGV < 0) {
	&usage();
}

$num_files = 100;
$num_deps = 7;
$hier_depth_min = 0;
$hier_depth_max = 7;

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
	} elsif($ARGV[0] eq "-hmin") {
		shift;
		if($#ARGV < 0) {
			&usage();
		}
		$hier_depth_min = $ARGV[0];
		shift;
	} elsif($ARGV[0] eq "-hmax") {
		shift;
		if($#ARGV < 0) {
			&usage();
		}
		$hier_depth_max = $ARGV[0];
		shift;
	} else {
		print STDERR "Unknown argument: $ARGV[0]\n";
		shift;
	}
}

if($hier_depth_min > $hier_depth_max) {
	$hier_depth_max = $hier_depth_min;
	print STDERR "Warning: hier_depth_max set to $hier_depth_max\n";
}

mkdir "tmake";
mkdir "ttup";
for($x=0; $x<$num_files; $x++) {
	$path_names{$x} = &generate_path($hier_depth_min, $hier_depth_max);
	if($path_names{$x} ne "") {
		system("mkdir -p tmake/$path_names{$x}");
		system("mkdir -p ttup/$path_names{$x}");
	}
	if($dir_names{$path_names{$x}} != 1) {
		$dir_names{$path_names{$x}} = 1;
		$mains{$x} = 1;
		system("cp ../testMakefile ttup/$path_names{$x}/Makefile");
	}
}

open MAKEFILE, ">tmake/Makefile" or die "Can't open Makefile for write\n";

print MAKEFILE "all:\n";
print MAKEFILE "objs :=\n";
print MAKEFILE "progs :=\n";

for($x=0; $x<$num_files; $x++) {
	my ($tmp_name);
	$tmp_name = $path_names{$x};
	$tmp_name =~ s/\/$//;
	$tmp_name =~ s#/#_#g;
	print MAKEFILE "objs += $path_names{$x}$x.o\n";
	print MAKEFILE "progs += prog_$tmp_name\n";
	print MAKEFILE "prog_$tmp_name: $path_names{$x}$x.o\n";
	open FILE, ">tmake/$path_names{$x}$x.c" or die "Can't open tmake/$path_names{$x}$x.c for write\n";
	for($y=0; $y<$num_deps; $y++) {
		my ($tmp);
		$tmp = ($x + $y) % $num_files;
		print FILE "#include \"$path_names{$tmp}$tmp.h\"\n";
	}
	print FILE "void func_$x(void) {}\n";
	if($mains{$x}) {
		print FILE "int main(void) {return 0;}\n";
	}
	close FILE;
	system("cp tmake/$path_names{$x}$x.c ttup/$path_names{$x}$x.c");
	open FILE, ">tmake/$path_names{$x}$x.h" or die "Can't open tmake/$path_names{$x}$x.h for write\n";
	print FILE "void func_$x(void);\n";
	close FILE;
	system("cp tmake/$path_names{$x}$x.h ttup/$path_names{$x}$x.h");
}

print MAKEFILE "progs := \$(sort \$(progs))\n";
print MAKEFILE "all: \$(progs)\n";
print MAKEFILE "deps := \$(objs:.o=.d)\n";
print MAKEFILE "-include \$(deps)\n";
print MAKEFILE "\$(progs): %: ; \$Qgcc -o \$@ \$^\n";
print MAKEFILE "%.o: %.c\n\t\$Qgcc -MMD -I. -c \$< -o \$@\n";
print MAKEFILE "clean: ; \@rm -rf \$(objs) \$(deps) \$(progs)\n";
print MAKEFILE ".PHONY: clean all\n";

close MAKEFILE;

sub usage
{
	print "Usage: $0 [-n num files] [-d num deps] [-hmin min hierarchy depth] [-hmax max hierarchy depth]\n";
	die;
}

sub generate_path
{
	my ($hier, $x, $path);

	$hier = int(rand($_[1] - $_[0] + 1)) + $_[0];
	for($x=0; $x<$hier; $x++) {
		$path .= $sample_paths[int(rand($#sample_paths + 1))]."/";
	}
	return $path;
}
