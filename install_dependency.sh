#!/bin/bash --login
SOURCE="${BASH_SOURCE[0]}"
DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"

os=$( uname )
HOMEBREW=
[ -d /usr/local/Cellar ] && HOMEBREW=/usr/local/Cellar # macos 11
[ -d /opt/homebrew/Cellar ] && HOMEBREW=/opt/homebrew/Cellar # macos 12+

[[ $os == Darwin ]] && brew install cmake libglib2.0-dev libglib2.9
[[ $os == Linux ]] && echo sudo apt install cmake && sudo apt install cmake

cd $DIR
source $DIR/install_nng_wolftls.sh

# JSMN: get jsmn.h
# cd $DIR
# if [ ! -f $DIR/jsmn.h ]; then
# 	rm -rf $DIR/jsmn
# 	git clone https://github.com/zserge/jsmn.git
# 	cp -v $DIR/jsmn/jsmn.h $DIR/
# 	rm -rf $DIR/jsmn
# fi
# if [ ! -f $DIR/jsmn.h ]; then
# 	echo "jsmn clone failed"
# 	exit 1
# else
# 	echo "jsmn.h checked"
# fi

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
	git init && git add map.* && \
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
