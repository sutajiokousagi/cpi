#!/usr/bin/perl

package CPUtil;


use strict;
use warnings;


use Crypt::OpenSSL::Bignum;

require Exporter;

our @ISA = qw(Exporter);
our @EXPORT = qw(error debug_print print_hex_data print_bn print_binary_as_hex_block new_bn_from_hex bn_cmp);


sub error($;$)
{
	my $message = shift;
	my $stack = (@_) ? shift : 0;

	my ($mod, $file, $line, $sub, undef) = caller($stack);
	
	print STDERR "Error: $message (mod=$mod, file=$file line=$line sub=$sub)\n";
	exit 1;
}

sub debug_print($$)
{
    my ($label, $bn) = @_;
    
    print $label . ': ' . $bn->to_hex() . "\n";
}


sub print_hex_data($)
{
    my $hex = shift;
    my @blocks = unpack('(a32)*', $hex);
    for my $b (@blocks)
    {
        print '  ' . $b . "\n";
    }
    
}


sub print_bn($)
{
    my $bn = shift;
    
    print_hex_data($bn->to_hex());
}
    


sub print_binary_as_hex_block($)
{
    my $bin = shift;
    my @blocks = unpack('(H32)*', $bin);
    for my $b (@blocks)
    {
        print $b . "\n";
    }
}


sub new_bn_from_hex($)
{
    my $s = shift;
    error("bad val: $s", 2) if( ! defined($s) || $s !~ /^[a-zA-Z0-9]+$/);
    return Crypt::OpenSSL::Bignum->new_from_hex($s);
}

sub bn_cmp($$$)
{
    my ($msg, $bn1, $bn2) = @_;
    
    if(defined($bn1) && defined($bn2) && (Crypt::OpenSSL::Bignum::cmp($bn1, $bn2) == 0))
    {
        print "OK: cmp: $msg\n";
    }
    else
    {
        print "Error: cmp failed: $msg\n";
    }
}




1;