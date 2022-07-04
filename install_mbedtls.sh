#!/bin/bash --login
SOURCE="${BASH_SOURCE[0]}"
DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"

# Ubuntu 20.04 needs system mbedtls, self compiled does not work, should be a NNG bug
os=$( uname )
if [[ $os == Linux ]]; then
	echo sudo apt -y install libmbedtls*
	sudo apt -y install libmbedtls*
	exit $?
fi

if [ ! -f $HOME/install/include/mbedtls/ssl.h ]; then
	if [ ! -d $HOME/Proj/mbedtls-3.1.0 ]; then
		cd $HOME/Proj
		wget -O mbedtls3.1.0.tar.gz 'https://github.com/Mbed-TLS/mbedtls/archive/refs/tags/v3.1.0.tar.gz' || exit 1
		tar xf mbedtls3.1.0.tar.gz || exit 1
		rm mbedtls3.1.0.tar.gz
	fi

	cd $HOME/Proj/mbedtls-3.1.0
	rm -rf ./build
	mkdir ./build
	src_dir=$HOME/Proj/mbedtls-3.1.0
	cmake $src_dir && \
		make && \
		cmake --install . --prefix $HOME/install
fi
if [ ! -f $HOME/install/include/mbedtls/ssl.h ]; then
	echo "mbedtls build failed"
	exit 1
else
	echo "mbedtls/ssl.h checked"
fi
