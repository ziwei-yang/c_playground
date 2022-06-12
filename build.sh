#!/bin/bash --login
SOURCE="${BASH_SOURCE[0]}"
DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"

os=$( uname )
[[ $os != 'Darwin' ]] && echo "build.sh for macos only"

HOMEBREW=
[ -d /usr/local/Cellar ] && HOMEBREW=/usr/local/Cellar # macos 11.6+
[ -d /opt/homebrew/Cellar ] && HOMEBREW=/opt/homebrew/Cellar # macos 12+

mbedtls_home=$HOME/install # Use ~/install if no brew installation
for path in $HOMEBREW/mbedtls/* ; do
	mbedtls_home=$path
	break
done

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
	-lnng -lpthread -lwolfssl -lmbedx509 -lmbedcrypto -lglib-2.0 -lhiredis -lwolfssl.o \
	$@
