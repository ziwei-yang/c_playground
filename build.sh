#!/bin/bash --login
SOURCE="${BASH_SOURCE[0]}"
DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"

os=$( uname )
[[ $os != 'Darwin' ]] && echo "build.sh for macos only"

HOMEBREW=
[ -d /usr/local/Cellar ] && HOMEBREW=/usr/local/Cellar # macos 11.6+
[ -d /opt/homebrew/Cellar ] && HOMEBREW=/opt/homebrew/Cellar # macos 12+

mbedtls_home=
for path in $HOMEBREW/mbedtls/* ; do
	mbedtls_home=$path
	break
done

glib_home=
for path in $HOMEBREW/glib/* ; do
	glib_home=$path
	break
done

hiredis_home=
for path in $HOMEBREW/hiredis/* ; do
	hiredis_home=$path
	break
done

gcc \
	-I /usr/local/include \
	-I $HOME/install/include \
	-I $mbedtls_home/include \
	-I $glib_home/include/glib-2.0 \
	-I $glib_home/lib/glib-2.0/include/\
	-I $hiredis_home/include/hiredis \
	-L /usr/local/lib \
	-L $HOME/install/lib \
	-L $mbedtls_home/lib \
	-L $glib_home/lib \
	-L $hiredis_home/lib \
	-lnng -lpthread -lmbedtls -lmbedx509 -lmbedcrypto -lglib-2.0 -lhiredis \
	$@
