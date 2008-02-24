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

# Get the lock to prevent the touch commands from generating attrib
# notifications.
open LOCK, ".tup/lock" or die "Can't open lock!\n";
flock LOCK, 1;
for($x=0; $x<$N; $x++) {
	my ($node);
	$node = $words[int(rand($#words + 1))];
	chomp($node);
	print STDERR "touch $node\n";
	system("touch $node");
	push @nodes, $node;
}
flock LOCK, 8;
close LOCK;

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
