package Shmoo::Util;

our @EXPORT_OK = qw{
    unique
};
push(@ISA, 'Exporter');
use Exporter qw{import};

sub unique (@) {
    my %seen; grep { ! $seen{$_}++ } @_;
}

1;
