#!/bin/bash --login
SOURCE="${BASH_SOURCE[0]}"
DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"

os=$( uname )
HOMEBREW=
[ -d /usr/local/Cellar ] && HOMEBREW=/usr/local/Cellar # macos 11
[ -d /opt/homebrew/Cellar ] && HOMEBREW=/opt/homebrew/Cellar # macos 12+

# compile NNG 1.5.2 at $HOME/Proj/nng/
cd $DIR
if [ ! -f $HOME/install/include/nng/nng.h ]; then
	cd $HOME/Proj/
	rm -rf $HOME/Proj/nng
	wget -O nng1.5.2.tar.gz 'https://github.com/nanomsg/nng/archive/refs/tags/v1.5.2.tar.gz' && \
		tar xf nng1.5.2.tar.gz && \
		mv nng-1.5.2 $HOME/Proj/nng && \
		rm nng1.5.2.tar.gz

	echo "Building nng1.5.2"
	cd $HOME/Proj/nng/
	rm -rf build
	mkdir build
	cd build

	# For NNG v1.5.2.tar.gz on macos m1
	[[ $os == Darwin ]] && brew install cmake mbedTLS
	[[ $os == Linux ]] && echo sudo apt install libmbedtls-dev && sudo apt install libmbedtls-dev

	echo cmake -DNNG_ENABLE_TLS=ON --install-prefix=$HOME/install .. 
	cmake -DNNG_ENABLE_TLS=ON --install-prefix=$HOME/install .. && \
		make && \
		cmake --install . --prefix $HOME/install # make install does not work with prefix on ubuntu
fi
if [ ! -f $HOME/install/include/nng/nng.h ]; then
	echo "nng build failed"
	exit 1
else
	echo "nng.h checked"
fi

# JSMN: get jsmn.h
cd $DIR
if [ ! -f $DIR/jsmn.h ]; then
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

# YYJSON: get yyjson.h yyjson.c
cd $DIR
if [ ! -f $DIR/yyjson.h ]; then
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

# hiredis: brew install hiredis 1.0.2+
cd $DIR
[[ $os == Darwin ]] && \
	[ ! -f $HOMEBREW/hiredis/1.0.2/include/hiredis/hiredis.h ] && \
	brew install hiredis
[[ $os == Linux ]] && [ ! -f $HOME/install/include/hiredis/hiredis.h ] && (
	if [ ! -d $HOME/Proj/hiredis-1.0.2 ]; then
		echo "get hiredis-1.0.2 into $HOME/Proj/ first"
		cd $HOME/Proj
		wget -O hiredis-1.0.2.tar.gz https://github.com/redis/hiredis/archive/refs/tags/v1.0.2.tar.gz
		tar xf hiredis-1.0.2.tar.gz && rm hiredis-1.0.2.tar.gz
		echo "remember to change Makefile: PREFIX?=$HOME/install"
		exit 1
	fi
	cd $HOME/Proj/hiredis-1.0.2

	make && make install
)
if [[ $os == Linux ]] && [[ ! -f $HOME/install/include/hiredis/hiredis.h ]]; then
	echo "hiredis build failed"
	exit 1
elif [[ $os == Darwin ]] && [[ ! -f $HOMEBREW/hiredis/1.0.2/include/hiredis/hiredis.h ]]; then
	echo "brew install hiredis failed"
	exit 1
else
	echo "hiredis.h checked"
fi

# clone c-hashmap -> git patch -> 3rd/map
cd $DIR
if [[ ! -f $DIR/3rd/map.h || ! -f $DIR/3rd/map.c ]]; then
	cd $HOME/Proj
	rmdir c-hashmap && mkdir c-hashmap
	cd c-hashmap
	wget -O map.h 'https://github.com/Mashpoe/c-hashmap/raw/main/map.h'
	wget -O map.c 'https://github.com/Mashpoe/c-hashmap/raw/main/map.c'
	if [[ ! -f ./map.c || ! -f ./map.h ]]; then
		echo "Downloading map.c map.h failed"
		exit 1
	fi
	git init
	git add map.*
	git commit -m 'first commit'
	git apply $DIR/patch/hashmap.diff
	if [[ $? != 0 ]]; then
		echo "Apply patch/hashmap.diff to Proj/c-hashmap failed"
		exit 1
	fi
	mkdir -p $DIR/3rd
	cp -v ./map.* $DIR/3rd
fi
if [[ ! -f $DIR/3rd/map.h || ! -f $DIR/3rd/map.c ]]; then
	echo "3rd/map.c 3rd/map.h not found"
	exit 1
else
	echo "3rd/map.c 3rd/map.h checked"
fi
