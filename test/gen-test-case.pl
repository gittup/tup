#! /usr/bin/perl

use strict;

my ($num_files, $num_deps, $hier_depth_min, $hier_depth_max, %path_names, $x, $y);
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
	if($x == 0) {
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
print MAKEFILE "\$(progs): %: ; \$Qld -r -o \$@ \$^\n";
print MAKEFILE "%.o: %.c\n\t\$Qgcc -MMD -I. -c \$< -o \$@\n";
print MAKEFILE "clean: ; \@rm -rf \$(objs) \$(deps) \$(progs)\n";
print MAKEFILE ".PHONY: clean all\n";

close MAKEFILE;

open BUILDFILE, ">ttup/build" or die "Can't open build file.\n";
print BUILDFILE <<ENDBUILD
#! /usr/bin/perl
use strict;

my (\$type, \$object, \$name, \$o1, \$o2);

if(\$#ARGV < 1) {
        die "Usage: \$0 type object\\n";
}

\$type = \$ARGV[0];
\$object = \$ARGV[1];
\$o1 = substr(\$object, 0, 2);
\$o2 = substr(\$object, 2);
if(!open NAME, ".tup/object/\$o1/\$o2/.name") {
        if(\$type & 2) {
                exit 0;
        } else {
                die "Object \$object has no associated name file!\\n";
        }
}
\$name = <NAME>;
chomp(\$name);
close NAME;

if(\$type & 1) {
        print "Create \$name\\n";
        if(\$name =~ /\\.c\$/) {
                my (\$output);
                \$output = \$name;
                \$output =~ s/\\.c\$/.o/;
                print "create dep \$name \$output\\n";
                if(system("create_dep \$name \$output") != 0) {
                        exit 1;
                }
                exit 0;
        } elsif(\$name =~ /\\.o\$/) {
		my (\$prog);
		\$prog = \$name;
		if(\$prog =~ /\\//) {
			\$prog =~ s/\\/[^\\/]*\$//;
		} else {
			\$prog = "";
		}
		\$prog =~ s#/#_#g;
		print "Create '\$name' -> 'prog_\$prog'\\n";
                if(system("create_dep \$name prog_\$prog") != 0) {
                        exit 1;
                }
        } else {
                print "  ignore  \$name\\n";
        }
}

if(\$type & 2) {
	die "Delete unsupported.\\n";
}
if(\$type & 4) {
        print "Modify \$name\\n";
        if(\$name =~ /\\.c\$/) {
                print "  ignore \$name\\n";
        } elsif(\$name =~ /\\.o\$/) {
                my (\$input);
                \$input = \$name;
                \$input =~ s/\\.o\$/.c/;
                print "  CC      \$input\\n";
                if(system("wrapper gcc -I. -c \$input -o \$name") != 0) {
                        exit 1;
                }
                exit 0;
        } elsif(\$name =~ /^prog_/) {
		my (\$dir);
		\$dir = \$name;
		\$dir =~ s/prog_(.*)/\\1/;
		\$dir =~ s/_/\\//g;
		if(\$dir ne "") {
			\$dir .= "/";
		}
                print "  LD-r      \$name\\n";
                if(system("wrapper ld -r -o \$name \$dir*.o") != 0) {
                        exit 1;
                }
                exit 0;
        }
}
ENDBUILD
;
close BUILDFILE;

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
