#!/bin/bash --login
SOURCE="${BASH_SOURCE[0]}"
DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"

os=$( uname )

# Linux cc linker does not support cc -lwholefilename.o
# Pass args as cc /path/to/wholefilename.o
ld_nng_wolfssl='-lwolfssl.o'
[[ $os == Linux ]] && \
	[ -f $HOME/install/lib/wolfssl.o && \
	[ ! -f /tmp/wolfssl.o ] && \
	cp -rv $HOME/install/lib/wolfssl.o /tmp/ && \
	ld_nng_wolfssl='/tmp/lwolfssl.o'

HOMEBREW=
[ -d /usr/local/Cellar ] && HOMEBREW=/usr/local/Cellar # macos 11.6+
[ -d /opt/homebrew/Cellar ] && HOMEBREW=/opt/homebrew/Cellar # macos 12+

glib_home=
glib_lib_home=
for path in $HOMEBREW/glib/* ; do
	glib_home=$path
	glib_lib_home=$path/lib
	break
done
if [[ $os == 'Linux' ]]; then
	glib_home=/usr
	glib_lib_home=/usr/lib/x86_64-linux-gnu
fi

hiredis_home=
for path in $HOMEBREW/hiredis/* ; do
	hiredis_home=$path
	break
done
if [[ $os == 'Linux' ]]; then
	hiredis_home=$HOME/install
fi

gcc \
	-I /usr/include \
	-I /usr/local/include \
	-I $HOME/install/include \
	-I $glib_home/include/glib-2.0 \
	-I $glib_lib_home/glib-2.0/include \
	-I $hiredis_home/include/hiredis \
	-L /usr/local/lib \
	-L $HOME/install/lib \
	-L $glib_home/lib \
	-L $hiredis_home/lib \
	-lnng -lpthread -lwolfssl -lmbedx509 -lmbedcrypto -lglib-2.0 -lhiredis $ld_nng_wolfssl \
	$@
