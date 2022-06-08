#!/bin/bash --login
SOURCE="${BASH_SOURCE[0]}"
DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"

if [ ! -f $HOME/install/include/nng/nng.h ]; then
	cd $HOME/Proj/nng/
	rm -rf build
	mkdir build
	cd build

	# For NNG v1.5.2.tar.gz on macos m1
	os=$( uname )
	[[ $os == Darwin ]] && brew install mbedTLS
	[[ $os == Linux ]] && echo sudo apt install libmbedtls-dev && sudo apt install libmbedtls-dev

	cmake -DNNG_ENABLE_TLS=ON --install-prefix=$HOME/install  .. && \
		make && \
		make install
fi
if [ ! -f $HOME/install/include/nng/nng.h ]; then
	echo "nng build failed"
	exit 1
else
	echo "nng.h checked"
fi

# JSMN
if [ ! -f $DIR/jsmn.h ]; then
	cd $DIR
	rm -rf $DIR/jsmn
	git clone https://github.com/zserge/jsmn.git
	cp -v $DIR/jsmn/jsmn.h $DIR/
	rm -rf $DIR/jsmn
fi
if [ ! -f $DIR/jsmn.h ]; then
	echo "jsmn clone failed"
	exit 1
else
	echo "jsmn.h checked"
fi
