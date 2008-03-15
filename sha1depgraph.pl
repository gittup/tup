#! /usr/bin/perl

use strict;

my (@files, $file, %name_hash, %incoming_hash, $from, $to, %color_hash, @circ_list, %visited, %stack, %ino, %secondary_ino);

open GRAPH, "| dot -Tpng | xv -" or die "Can't open graph pipe\n";

print GRAPH "digraph g {\n";

@files = `ls .tup/object/*/*/.name 2>/dev/null`;
foreach $file (@files) {
	my (@stats, $tmp);
	chomp($file);
	$from = $file;
	$from =~ s#\.tup/object/([0-9a-f]*)/([0-9a-f]*)/.name#\1\2#;
	open FILE, "$file" or die "Can't open $file\n";
	@stats = stat FILE;
	$color_hash{$from} = 0x000000;
	$incoming_hash{$from} = $stats[3] - 1; # num hard links
	$ino{$from} = $stats[1]; # inode
	$tmp = $file;
	$tmp =~ s/\.name/.secondary/;
	@stats = stat $tmp;
	$secondary_ino{$from} = $stats[1]; # inode
	$name_hash{$from} = <FILE>;
	chomp($name_hash{$from});
	close FILE;
}

@files = `ls .tup/object/*/*/* 2>/dev/null`;
foreach $file (@files) {
	my ($from2, $color, @stats);
	chomp($file);
	@stats = stat $file;
	($from, $from2, $to) = $file =~ m#\.tup/object/([0-9a-f]*)/([0-9a-f]*)/([0-9a-f]*)#;
	$from .= $from2;
	if($stats[1] == $ino{$to}) {
		$color = "000000";
	} elsif($stats[1] == $secondary_ino{$to}) {
		$color = "888888";
	} else {
		$color = "ff0000";
	}
	print GRAPH "tup$to -> tup$from [dir=back,color=\"#$color\"];\n";
}

&tup_directory("modify", 0x0000ff);
&tup_directory("create", 0x00ff00);
&tup_directory("delete", 0xff0000);

foreach $from (keys %name_hash) {
	printf GRAPH "tup$from [label=\"$name_hash{$from} ($incoming_hash{$from})\" color=\"#%06x\"];\n", $color_hash{$from};
}

print GRAPH "}\n";
close GRAPH;

sub tup_directory
{
	@files = `ls .tup/$_[0]/* 2>/dev/null`;
	foreach $file (@files) {
		chomp($file);
		($from) = $file =~ m#\.tup/....../([0-9a-f]*)#;
		%visited = ();
		%stack = ();
		@circ_list = ();
		&follow_chain($from, $_[1]);
	}
}

sub follow_chain
{
	my ($f, @list, $dep, $c, $f1, $f2);

	$f = $_[0];
	$c = $_[1];
	$f1 = substr($f, 0, 2);
	$f2 = substr($f, 2);
	push(@circ_list, $f);

	if($stack{$f} == 1) {
		print STDERR "Error: Circular dependency detected:\n";
		foreach $f (@circ_list) {
			if(exists $name_hash{$f}) {
				print STDERR "  $name_hash{$f}\n";
			} else {
				print STDERR "  $f\n";
			}
		}
		die;
	}
	if($visited{$f} == 1) {
		return;
	}
	$visited{$f} = 1;
	$stack{$f} = 1;

	$color_hash{$f} |= $c;
	@list = `ls .tup/object/$f1/$f2/* 2>/dev/null`;
	foreach $dep (@list) {
		($dep) = $dep =~ m#\.tup/object/$f1/$f2/([0-9a-f]*)#;
		&follow_chain($dep, $c);
	}
	$stack{$f} = 0;
}
