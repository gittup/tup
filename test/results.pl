#! /usr/bin/perl
&gen_data("initial", 2, 100, 1);
&gen_data("c_file", 9, 107, 9);
&gen_data("h_file", 40, 138, 9);
&gen_data("nothing", 71, 169, 9);

sub gen_data
{
	@arr = (1, 10, 100, 1000, 10000);
	open OUTPUT, ">plot-$_[0].dat" or die "Can't open $_[0].data for write!";
	foreach $val (@arr) {
		open FILE, "out-$val.txt" or die "Can't open out-$val.txt!";
		for($x=1; $x<$_[1]; $x++) {
			$tmp = <FILE>;
		}
		$val1 = 0;
		for($x=0; $x<$_[3]; $x++) {
			$tmp = <FILE>;
			$tmp =~ s/real //;
			$val1 += $tmp;
			$tmp = <FILE>;
			$tmp = <FILE>;
		}
		$val1 /= $_[3];
		close FILE;

		open FILE, "out-$val.txt" or die "Can't open out-$val.txt!";
		for($x=1; $x<$_[2]; $x++) {
			$tmp = <FILE>;
		}
		$val2 = 0;
		for($x=0; $x<$_[3]; $x++) {
			$tmp = <FILE>;
			$tmp =~ s/real //;
			$val2 += $tmp;
			$tmp = <FILE>;
			$tmp = <FILE>;
		}
		$val2 /= $_[3];
		close FILE;
		chomp($val1);
		chomp($val2);
		print OUTPUT "$val $val1 $val2\n";
	}
	close OUTPUT;
}
