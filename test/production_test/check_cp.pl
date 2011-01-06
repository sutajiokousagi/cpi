#!/usr/bin/perl

#
# This script checks the crypto processor except for the encrypted owner key.
# usage:
#  $0 --request=QUERY_FILE --response=RESPONSE_FILE ( --no-keys | --keys=KEY_FILE )
#

use strict;
use warnings;
use Crypt::OpenSSL::Bignum;
use Crypt::OpenSSL::RSA;
use Digest::SHA1;

use lib "$0/..";
use CPParse;
use CPUtil;


*mod_mul = \&Crypt::OpenSSL::Bignum::mod_mul;

my $keys = undef;
my $data = undef;
my $expected = undef;
my $calc = {};
my $verbose = 0;
my $current_key_n = undef;
my $current_key_e = undef;
my $key_num;
my $rn;
my $use_key_from_chumby = 0;


sub verbose_print_bn($$)
{
    if($verbose)
    {
        my ($name, $value) = @_;
        
        print $name, "\n";
        print_bn($value);
    }
}


sub extract_key($)
{
	my $pk = shift;

    my ($unknown1, $chumby_n, $unknown2, $chumby_e) = unpack('a10 a128 a2 a4', $pk);

    $chumby_n = Crypt::OpenSSL::Bignum->new_from_bin($chumby_n);
    $chumby_e = Crypt::OpenSSL::Bignum->new_from_bin($chumby_e);
    
    return ($chumby_n, $chumby_e);
}


sub test_pub_key_from_device($)
{
    my $pk = shift;
    
    my ($chumby_n, $chumby_e) = extract_key($pk);
    
    # compare public keys
    bn_cmp("chumby public key n", $chumby_n, $current_key_n);
    bn_cmp("chumby public key e", $chumby_e, $current_key_e);    
}




# 
# pid, rn, rm, $p_aqs_ok should be hex strings.
#
sub build_plain_text_message($$$$$$)
{
    my ($x, $pid, $rn, $rm, $p_aqs_ok, $version) = @_;

    for my $test (@_)
    {
        error("bad hex string") if($test !~ /^[a-zA-Z0-9]+$/);
    }
    my $message = '';
    
    $message .= pack('H*', $p_aqs_ok);
    $message .= pack('H*', $rn);
    $message .= pack('H*', $rm);
    $message .= pack( 'N', $x);
    $message .= Digest::SHA1::sha1(pack('H*', $pid));
    $message .= pack('H*', $version);
    
    my $message_length = length($message);
    error("message wrong size: actual=$message_length") if($message_length != 316);
    
    return $message;
}



sub handle_arg($)
{
    my $arg = shift;
	if( $arg eq '--')
	{
		last;
	}
	elsif( $arg =~ /^--data=(.+)$/ )
	{
		$data = load_chumby_data($1);
	}
	elsif( $arg =~ /^--keys=(.+)$/ )
	{
		$keys = load_key_file($1);
	}
	elsif( $arg eq '--test' )
	{
	   error("f1") if (clean_string("abc\n\n") ne 'abc');
	   error("f2") if (clean_string("\nab\r") ne 'ab');
	}
	elsif( $arg =~ /^--request=(.+)$/ )
	{
        ($key_num, $rn) = parse_cpi_xml_request($1);
	}
	elsif( $arg =~ /^--response=(.+)$/ )
	{
	   $data = parse_cpi_xml_response($1);
	}
	elsif( $arg eq '--no-keys')
	{
		$use_key_from_chumby = 1;
	}
	elsif( $arg eq '--verbose' )
	{
	   $verbose = 1;
	}
	else
	{
		error("invalid arg: $arg");
	}
}




while( @ARGV && $ARGV[0] =~ /^--/)
{
	my $arg = shift(@ARGV);
    handle_arg($arg);
}

if( $use_key_from_chumby )
{
	print STDERR "Info: using keys from chumby\n";
	($keys->[$key_num]->{PKEY_N_bn}, $keys->[$key_num]->{PKEY_E_bn}) = extract_key($data->{pkey});
}


# process data
error("cdata is missing. Use --data=<file>") if( ! defined($data));
error("keys missing (use --keys=<file>)") if( ! defined($keys) || @{$keys}  == 0);

# setup known data

# constants
my $one = Crypt::OpenSSL::Bignum->one();
my $zero = Crypt::OpenSSL::Bignum->zero(); 
my $scratch = Crypt::OpenSSL::Bignum::CTX->new();


$current_key_n = $keys->[$key_num]->{PKEY_N_bn};
$current_key_e = $keys->[$key_num]->{PKEY_E_bn};

my $key_cp = Crypt::OpenSSL::RSA->new_key_from_parameters($current_key_n, $current_key_e);
$key_cp->use_pkcs1_padding();
$key_cp->use_sha1_hash();



if(defined($data->{pkey}))
{
    test_pub_key_from_device($data->{pkey});
}
else
{
    print "skipping pub key compare\n";
}



print "using key=$key_num\n";
print "using pid=$data->{pid}\n";

my $message = build_plain_text_message($key_num, $data->{pid}, $rn, $data->{rm}, $data->{ok}, $data->{version});
my $message_hash = Digest::SHA1::sha1($message);


my $bn_gcd = Crypt::OpenSSL::Bignum::gcd($data->{rm_bn}, $current_key_n, $scratch);
bn_cmp("gcd(rm) == one", $bn_gcd, $one);
verbose_print_bn("gcd", $bn_gcd);


$calc->{blind} = $data->{rm_bn};
#$calc->blind = mod_exp($data->{rm_bn}, $current_key_e, $current_key_n, $scratch);
$calc->{blind_inv} = Crypt::OpenSSL::Bignum::mod_inverse($calc->{blind}, $current_key_n, $scratch);

verbose_print_bn("rm", $data->{rm_bn});
verbose_print_bn("rm-inv", $calc->{blind_inv});


$calc->{gcd} = Crypt::OpenSSL::Bignum::gcd($calc->{blind_inv}, $current_key_n, $scratch);
bn_cmp("gcd(rm-inv,key_n)== one", $calc->{gcd}, $one);

verbose_print_bn("gcd(rm-inv)", $calc->{gcd});



my $tmp = mod_mul($calc->{blind}, $calc->{blind_inv}, $current_key_n, $scratch);
bn_cmp("rm x rm-inv == one", $tmp, $one);
verbose_print_bn("rm x rm-inv", $tmp);

# unblind
$calc->{sig_unblind} = mod_mul($data->{s_bn}, $calc->{blind_inv}, $current_key_n, $scratch);

verbose_print_bn("sig unblinded", $calc->{sig_unblind});


my $sig_binary = $calc->{sig_unblind}->to_bin();
if($key_cp->verify($message, $sig_binary))
{
    print "\033[0;32mOK: rsa verify ok\033[0m\n";
}
else
{
    print "\033[0;31mError: rsa verify failed\033[0m\n";
}

#decrypt 
$calc->{sig} = $calc->{sig_unblind}->mod_exp($current_key_e, $current_key_n, $scratch);
verbose_print_bn("decrypted sig", $calc->{sig});


my $raw_sig = $calc->{sig}->to_bin();
if(length($raw_sig) == 127)
{
    $raw_sig = chr(0) . $raw_sig;
}

if($raw_sig =~ /^\x00\x01.*?\x00\x30\x21\x30\x09\x06\x05\x2b\x0e\x03\x02\x1a\x05\x00\x04\x14(.*)$/s)
{
    my $sig = $1;
    my $sig_bn = Crypt::OpenSSL::Bignum->new_from_bin($sig);
    my $message_hash_bn = Crypt::OpenSSL::Bignum->new_from_bin($message_hash);

    verbose_print_bn("decrypted sig", $sig_bn);
    verbose_print_bn("message hash", $message_hash_bn);
    bn_cmp("SHA1(msg) == decrypted sig", $sig_bn, $message_hash_bn);
}
else
{
    print "Error: PKCS1 padding is bad\n";
}


print "done\n";



