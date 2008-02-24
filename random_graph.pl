#! /usr/bin/perl

use strict;

my ($N, $E, @words, $x, @nodes, %deps);

if($#ARGV < 1) {
	print STDERR "Usage $0 num_nodes num_edges\n";
	exit 1;
}

$N = $ARGV[0];
$E = $ARGV[1];

open DICTIONARY, "/usr/share/dict/words" or die "Can't open dictionary!\n";
@words = <DICTIONARY>;
close DICTIONARY;

for($x=0; $x<$N; $x++) {
	my ($node);
	$node = $words[int(rand($#words + 1))];
	chomp($node);
	print STDERR "touch $node\n";
	system("../../wrapper touch $node");
	push @nodes, $node;
}

for($x=0; $x<$E; $x++) {
	my ($from, $to);
	$from = int(rand($#nodes + 1));
	$to = int(rand($#nodes + 1));
	push(@{$deps{$nodes[$from]}}, $nodes[$to]);
}

foreach $x (keys %deps) {
	print STDERR "../../create_dep @{$deps{$x}} $x\n";
	system("../../wrapper ../../create_dep @{$deps{$x}} $x");
}
