#!/bin/bash --login
SOURCE="${BASH_SOURCE[0]}"
DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"

# NNG 1.5.2
if [ ! -f $HOME/install/include/nng/nng.h ]; then
	if [ ! -d $HOME/Proj/nng ]; then
		echo "get nng 1.5.2 into $HOME/Proj/nng first"
		exit 1
	fi
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

# YYJSON
if [ ! -f $DIR/yyjson.h ]; then
	cd $DIR
	wget -O yyjson.tar.gz https://github.com/ibireme/yyjson/archive/refs/tags/0.5.0.tar.gz
	tar xf yyjson.tar.gz
	cp -rv yyjson-0.5.0/src/yyjson.* ./
	rm -rf yyjson-0.5.0 yyjson.tar.gz
fi
if [ ! -f $DIR/yyjson.h ]; then
	echo "yyjson extract failed"
	exit 1
else
	echo "yyjson.h checked"
fi

# hiredis
if [ ! -f $HOME/install/include/hiredis/hiredis.h ]; then
	if [ ! -d $HOME/Proj/hiredis-1.0.2 ]; then
		echo "get hiredis-1.0.2 into $HOME/Proj/ first"
		echo "remember to change Makefile: PREFIX?=$HOME/install"
		exit 1
	fi
	cd $HOME/Proj/hiredis-1.0.2

	make && make install
fi
if [ ! -f $HOME/install/include/hiredis/hiredis.h ]; then
	echo "hiredis build failed"
	exit 1
else
	echo "hiredis.h checked"
fi
