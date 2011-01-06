#!/usr/bin/perl
#
# extract owner key from response.xml into a separate file
# response.xml.ok
# usage:
#   $0 RESPONSE_FILE...
#

use strict;
use warnings;
use File::Basename;

use lib (dirname($0));

use CPParse;



while(@ARGV)
{
	my $arg = shift(@ARGV);
	if( -f $arg)
	{
		print STDERR "processing: $arg\n";
		my $rec = parse_cpi_xml_response($arg);	
		my $ok = pack('H*', $rec->{ok});
		my $out_file = $arg . '.ok';
		my $fd = undef;
		open($fd, ">", $out_file) or die "open failed: $out_file";
		print $fd $ok;
		close($fd);
		$fd = undef;
	}
	else
	{
		print STDERR "Error: bad file: $arg\n";
		exit 1;
	}
}

