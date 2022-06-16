#!/bin/bash --login
SOURCE="${BASH_SOURCE[0]}"
DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"

os=$( uname )

# Linux cc linker does not support cc -lwholefilename.o
# Pass args as cc /path/to/wholefilename.o
ld_nng_wolfssl='-lwolfssl.o'
nng_wolfssl_o=
[[ $os == Linux ]] && \
	ld_nng_wolfssl='' && \
	nng_wolfssl_o='/tmp/wolfssl.o' && \
	[ -f $HOME/install/lib/wolfssl.o ] && \
	[ ! -f /tmp/wolfssl.o ] && \
	cp -rv $HOME/install/lib/wolfssl.o /tmp/

HOMEBREW=
[ -d /usr/local/Cellar ] && HOMEBREW=/usr/local/Cellar # macos 11.6+
[ -d /opt/homebrew/Cellar ] && HOMEBREW=/opt/homebrew/Cellar # macos 12+

glib_include=
glib_lib=
glib_ext_home=
for path in $HOMEBREW/glib/* ; do
	glib_include=$path/include/glib-2.0
	glib_lib=$path/lib
	glib_ext_home=$path/lib/glib-2.0
	break
done
if [[ $os == 'Linux' ]]; then
	glib_include=/usr/include/glib-2.0
	glib_lib=/usr/lib/x86_64-linux-gnu
	glib_ext_home=/usr/lib/x86_64-linux-gnu/glib-2.0
fi

hiredis_home=
for path in $HOMEBREW/hiredis/* ; do
	hiredis_home=$path
	break
done
if [[ $os == 'Linux' ]]; then
	hiredis_home=$HOME/install
fi

# ubuntu must put -largs at last
echo gcc \
	-I/usr/include \
	-I/usr/local/include \
	-I$HOME/install/include \
	-I$glib_include \
	-I$glib_ext_home/include \
	-I$hiredis_home/include/hiredis \
	\
	-L /usr/local/lib \
	-L $HOME/install/lib \
	-L $glib_lib \
	-L $hiredis_home/lib \
	\
	$nng_wolfssl_o \
	$@ \
	\
	-lnng -lpthread -lwolfssl -lmbedx509 -lmbedcrypto -lglib-2.0 -lhiredis $ld_nng_wolfssl \
