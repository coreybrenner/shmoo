package Shmoo::Parser::Token;

use strict;
use warnings;

use Scalar::Util qw{blessed};

use overload (
    '""'  => sub { join('', @{(shift)->{txt}}) },
);

sub new {
    my $what = shift;
    my $type = (ref($what) || $what);
    my $orig = (ref($what) && $what);
    
    my %self;
    my @text;
    if ($orig) {
        @text = (@{$orig->{txt} // []}, @_);
        %self = (
            org => $orig->{org},
            str => $orig->{str},
            pos => $orig->{pos},
        );
    } elsif (@_ <= 2) {
        die(__PACKAGE__."::new: expected (origin, whole_string [, pos [, text ...]]); got [@_]\n");
    } else {
        my ($org, $str) = (shift, shift);
        my $pos = (@_ ? shift : pos($str));
        @text = (@{$orig->{txt} // []}, @_);
        %self = (
            org => $org,
            str => $str,
            pos => $pos,
        );
    }

    my $span = 0;
    my $expr = qr{
        (?(DEFINE)
            (?<newline>
                (?: \r\n | [\r\n] )     (?{ ++$span })
            )
        )
        (?: [^\r\n]++
          | (?> (?&newline) )++
        )
    }xo;

    foreach (@text) {
        m{^$expr$}o;
    }
    $self{txt} = \@text;
    $self{spn} = $span;

    return bless(\%self, $type);
}

sub span {
    my $self = shift;
    if (! @_) {
        return $self->{spn};
    } else {
        $self{spn} += shift;
        return $self;
    }
}

sub text {
    my $self = shift;
    if (! @_) {
        return (
            wantarray
                ? @{$self->{txt}}
                : join('', map { "$_" } @{$self->{txt}})
        );
    } else {
        my $span = 0;
        my $expr = qr{
            (?(DEFINE)
                (?<newline>
                    (?: \r\n | [\r\n] )     (?{ ++$span })
                )
            )
            (?: [^\r\n]++
                | (?> (?&newline) )++
            )++
        }xo;
        
        foreach (@_) {
            if (blessed($_) && $_->isa(__PACKAGE__)) {
                $span += $_->span();
            } else {
                m{^$expr$}o;
            }
            push(@{$self->{txt}}, $_);
        }
        $self->{spn} += $span;

        return $self;
    }
}

1;
