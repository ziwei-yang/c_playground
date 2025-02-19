#!/bin/bash --login
SOURCE="${BASH_SOURCE[0]}"
DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"

os=$( uname )
HOMEBREW=
[ -d /usr/local/Cellar ] && HOMEBREW=/usr/local/Cellar # macos 11
[ -d /opt/homebrew/Cellar ] && HOMEBREW=/opt/homebrew/Cellar # macos 12+

# Change poor 4MB share memory limit on macOS
[[ $os == Darwin ]] && $DIR/macos_chg_shmmax.sh

[[ $os == Darwin ]] && brew install cmake libglib2.0-dev libglib2.9
[[ $os == Linux ]] && echo sudo apt install cmake && sudo apt install cmake

cd $DIR
# NNG does not need mbedtls as TLS module but,
# it deeply depends mbedtls, even engine switched to wolfssl
source $DIR/install_mbedtls.sh
source $DIR/install_nng_wolftls.sh # TLS for NNG

# YYJSON: get yyjson.h yyjson.c
cd $DIR
if [ ! -f $DIR/yyjson.h ]; then
	wget -O yyjson.tar.gz https://github.com/ibireme/yyjson/archive/refs/tags/0.5.1.tar.gz
	tar xf yyjson.tar.gz
	cp -rv yyjson-0.5.1/src/yyjson.* ./
	rm -rf yyjson-0.5.1 yyjson.tar.gz
fi
if [ ! -f $DIR/yyjson.h ]; then
	echo "yyjson extract failed"
	exit 1
else
	echo "yyjson.h checked"
fi

# hiredis: brew install hiredis 1.0.2+
cd $DIR
[[ $os == Darwin ]] && \
	[ ! -f $HOMEBREW/hiredis/1.0.2/include/hiredis/hiredis.h ] && \
	brew install hiredis
[[ $os == Linux ]] && [ ! -f $HOME/install/lib/libhiredis.so.1.0.0 ] && (
	if [ ! -d $HOME/Proj/hiredis-1.0.2 ]; then
		echo "get hiredis-1.0.2 into $HOME/Proj/ first"
		cd $HOME/Proj
		wget -O hiredis-1.0.2.tar.gz https://github.com/redis/hiredis/archive/refs/tags/v1.0.2.tar.gz
		tar xf hiredis-1.0.2.tar.gz && rm hiredis-1.0.2.tar.gz
	fi

	echo "Go to change $HOME/Proj/hiredis-1.0.2/Makefile : PREFIX?=$HOME/install"
	cd $HOME/Proj/hiredis-1.0.2 && \
		git init && \
		git add Makefile && \
		git apply $DIR/patch/hiredis_Makefile.diff

	# make install failed in copying files on ubuntu2004, do it mannually
	make && make install
	[ ! -f $HOME/install/lib/libhiredis.so ] && cp -v libhiredis.so $HOME/install/lib/
	[ ! -f $HOME/install/lib/libhiredis.a ] && cp -v libhiredis.a $HOME/install/lib/
	[ ! -f $HOME/install/lib/pkgconfig/hiredis.pc ] && cp -v hiredis.pc $HOME/install/lib/pkgconfig/
	cd $HOME/install/lib
	ln -sf libhiredis.so libhiredis.so.1.0.0 

	cd $DIR
)
if [[ $os == Linux ]] && [[ ! -f $HOME/install/lib/libhiredis.so.1.0.0 ]]; then
	echo "hiredis build failed"
	exit 1
elif [[ $os == Darwin ]] && [[ ! -f $HOMEBREW/hiredis/1.0.2/include/hiredis/hiredis.h ]]; then
	echo "brew install hiredis failed"
	exit 1
else
	echo "hiredis.h checked"
fi
