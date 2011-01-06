#!/usr/bin/perl

package CPParse;

use strict;
use warnings;
use MIME::Base64;
use Crypt::OpenSSL::Bignum;

use CPUtil;
require Exporter;


our @ISA = qw(Exporter);
our @EXPORT = qw(load_key_file  parse_cpi_xml_response parse_cpi_xml_request);


sub batch_hex_to_bn($)
{
    my $r = shift;
    my @keys = keys(%{$r});
    for my $k (@keys)
    {
        my $new_key = "${k}_bn";
        error("name conflict: $k") if(defined($r->{$new_key}));
        my $hex = clean_string($r->{$k});

        if( $r->{$k} =~ /^[a-zA-Z0-9]+$/ )
        {
            $r->{$new_key} = new_bn_from_hex($r->{$k});
        }
    }
}


sub clean_string($)
{
	my $s = shift;
	
	$s =~ s/\s+$//;
	$s =~ s/^\s+//;
	$s =~ s/\s+#(.*)$//;

	return $s;
}


sub load_key_file($)
{
    my $file = shift;
    my $fd = undef;
    my $keys = [];
    
    open( $fd, "<", $file) or error("open failed: $file");
    while( my $line = <$fd> )
    {
        $line = clean_string($line);
        
        my ($key, $index, $value) = split(':', $line);
        
        $value = clean_string($value);
        $keys->[$index]->{$key} = $value;
        $keys->[$index]->{"${key}_bn"} = new_bn_from_hex($value);
    }
    return $keys;
}


sub load_file($)
{
    my $file = shift;
	my $rec = {};
	my $fd = undef;


	error( "invalid file: $file") if( ! -f $file );
	open($fd, "<", $file) or error("open failed: $file");
	
	local $/ = '';
	my $data = <$fd>;
	close($fd);
	
	return $data;
}    


sub parse_cpi_xml_request($)
{
    my $file = shift;
    my $data = load_file($file);
    
    my ($pid) = ( $data =~ m|<query type="pidx" key_id="(\d+)"></query>|i );
    my ($key, $rn) = ( $data =~ m|<query type="chal" key_id="(\d+)" rand_data="([A-F0-9]+)"></query>|i );
    
    return ($key, $rn);
}


sub parse_cpi_xml_response($)
{
    my $file = shift;
	my $rec = {};
	my $fd = undef;

    my $data = load_file($file);
    
    
    my ($pid) = ($data =~ m|<response type="pidx" result="success">([^<>]+)</response>|i );
    $pid =~ s/-//g;

    my ($chal) = ($data =~ m|<response type="chal" result="success">((?:[^<]\|(?:<(?!/response)))+)</response>|i ); 
    my ($enc, $rm, $vers, $sig) = ( $chal =~ m|<enc_owner_key>([A-F0-9]+)</enc_owner_key>\s*<rand_data>([A-F0-9]+)</rand_data>\s*<vers>([A-F0-9]+)</vers>\s*<signature>([A-F0-9]+)</signature>|i );
    
    
    my ($pkey) = ($data =~ m| <response type="pkey" result="success">([^<]+)</response>|i );    
    $pkey =~ s/-----BEGIN PGP PUBLIC KEY BLOCK-----//;
    $pkey =~ s/-----END PGP PUBLIC KEY BLOCK-----//;
    $pkey = clean_string($pkey);
    $pkey = decode_base64($pkey);

    $rec->{pkey} = $pkey;
    $rec->{pid} = $pid;
    $rec->{ok} = $enc;
    $rec->{rm} = $rm;
    $rec->{version} = $vers;
    $rec->{s} = $sig;

    batch_hex_to_bn($rec);
    
    return $rec;
}


1;