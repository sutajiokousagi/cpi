#!/usr/bin/perl
#
# This script decrypts the owner key.
# Usage:
#  $0 --keys=n:e:d:p:q ENCRYPTED_OWNER_KEY...
#    where n,e,d,p,q are files containing that part of the key.
#

use strict;
use warnings;

use Crypt::OpenSSL::Bignum;
use Crypt::OpenSSL::RSA;
use Crypt::OpenSSL::Bignum::CTX;
use IO::Handle;


my $key;
my $use_pkcs1_padding = 1;

my $looks_ok_count = 0;
my $strict_check_count = 0;

STDOUT->autoflush(1);
STDERR->autoflush(1);

sub print_bn($)
{
	my $bn = shift;
	my $hex = $bn->to_hex();
	print $hex . "\n";
}

sub bn_from_file($)
{
	my $file = shift;
	my $fd = undef;

	open($fd, "<", $file) or die "cannot open: $file";
	local $/ = undef;
	my $data = <$fd>;
	close($fd);
	$fd = undef;
	return Crypt::OpenSSL::Bignum->new_from_bin($data);
}

sub load_file($)
{
	my $fd = undef;
	my $file = shift;

	open($fd, "<", $file) or die "cannot open: $file";
	local $/ = undef;
	my $data = <$fd>;
	close($fd);
	$fd = undef;
	return $data;
}

while(@ARGV)
{
	my $arg = shift(@ARGV);
	
	if( $arg =~ /^--keys=([^:]+):([^:]+):([^:]+):([^:]+):([^:]+)$/ )
	{
		my $n = bn_from_file($1);
		my $e = bn_from_file $2;
		my $d = bn_from_file $3;
		my $p = bn_from_file $4;
		my $q = bn_from_file $5;
		$key = Crypt::OpenSSL::RSA->new_key_from_parameters ( $n, $e, $d, $p, $q);
		$key->check_key() or die "key is bad";
		$key->use_pkcs1_padding();
		#$key->use_no_padding();
		$key->use_sha1_hash();

		my $ctx = Crypt::OpenSSL::Bignum::CTX->new();
		my $temp = $p->mul($q, $ctx);

		#print_bn($n);
		#print_bn($temp);

		if( $n->cmp( $temp) == 0)
		{
			print "key ok\n";
		}
		else
		{
			print "key bad\n";
			exit 1;
		}
	}
	elsif( -f $arg )
	{
		if( $use_pkcs1_padding )
		{
			print STDERR "testing file using pkcs1 padding: $arg\n";
		}
		else
		{
			print STDERR "testing file using no padding: $arg\n";
		}
		my $data = load_file($arg);
		my $pt = $key->decrypt($data);
		my $size = length($pt);

		if( $use_pkcs1_padding )
		{
			if( $size != 16 )
			{
				print STDERR "\033[0;31mError: bad size actual_size=$size\033[0m\n";
				exit 1;
			}
		}
		else
		{
			if( $size != 256 )
			{
				print STDERR "\033[0;31mError: bad size actual_size=$size\033[0m\n";
				exit 1;
			}
			if( $pt =~ m/^\000\002.{237}\000/s )
			{
				++$looks_ok_count;
				print STDERR "looks ok ($looks_ok_count)\n";
			}
			else
			{
				print STDERR "Error: bad block\n";
				exit 1;
			}
			if( $pt =~ m/^\000\002[^\000]{237}\000/s )
			{
				++$strict_check_count;
				print STDERR "strict test ok ($strict_check_count)\n";
			}
			else
			{
				print STDERR "Error: strict check failed\n";
				exit 1;
			}
		}
		print STDERR "\033[0;30mdecrypt ok: "; 
		print unpack('H*', $pt);
		print STDERR "\033[0m\n";
	}
	elsif( $arg eq '--no-padding')
	{
		if(!defined($key))
		{
			print STDERR "Error: padding must be set after key is loaded\n";
			exit 1;
		}
		$key->use_no_padding();
		$use_pkcs1_padding = 0
	}
	else
	{
		print STDERR "bad arg: $arg\n";
		exit 1;
	}
} 


