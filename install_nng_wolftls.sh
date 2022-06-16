#!/bin/bash --login
SOURCE="${BASH_SOURCE[0]}"
DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"

cd $HOME/Proj
os=$( uname )

################################################
# Step 0 Build WolfSSL.
################################################

if [ ! -f $HOME/install/include/wolfssl/ssl.h ]; then
	[ ! -d $HOME/Proj/wolfssl-5.3.0-stable ] && \
		echo "Download wolfssl-5.3.0 to ~/Proj" && \
		wget -O wolfssl5.3.0.tar.gz 'https://github.com/wolfSSL/wolfssl/archive/refs/tags/v5.3.0-stable.tar.gz' && \
		tar xf wolfssl5.3.0.tar.gz && \
		rm wolfssl5.3.0.tar.gz
	[ ! -d $HOME/Proj/wolfssl-5.3.0-stable ] && \
		echo "Failed in creating wolfssl dir" && exit 1

	cd $HOME/Proj/wolfssl-5.3.0-stable && \
		./autogen.sh && \
		./configure --enable-all --enable-tls13 --prefix=$HOME/install && \
		make && make install

	[[ $? != 0 ]] && echo "nng build failed" && exit 1
fi
[ ! -f $HOME/install/include/wolfssl/ssl.h ] && \
	echo "Failed in installing wolfssl" && exit 1
[ -f $HOME/install/include/wolfssl/ssl.h ] && \
	echo "wolfssl checked"

################################################
# Now build NNG + WolfSSL TLS adapter.
################################################

cd $DIR
if [ ! -f $HOME/install/include/nng/nng.h ]; then
	cd $HOME/Proj/
	rm -rf $HOME/Proj/nng
	[ ! -d $HOME/Proj/nng ] && \
		echo "Download nng-1.5.2 to ~/Proj" && \
		wget -O nng1.5.2.tar.gz 'https://github.com/nanomsg/nng/archive/refs/tags/v1.5.2.tar.gz' && \
		tar xf nng1.5.2.tar.gz && \
		mv nng-1.5.2 $HOME/Proj/nng && \
		rm nng1.5.2.tar.gz
	[ ! -d $HOME/Proj/nng ] && \
		echo "Failed to create ~/Proj/nng" && exit 1

	################################################
	# Step 1 Build local NNG without TLS
	################################################
	echo "Building nng1.5.2 without ssl first to build wolfssl"
	cd $HOME/Proj/nng/
	rm -rf build && mkdir build && cd build && \
		echo cmake --install-prefix=$HOME/install .. && \
		cmake --install-prefix=$HOME/install .. && \
		make && \
		cmake --install . --prefix $HOME/install # make install does not work with prefix on ubuntu

	################################################
	# Step 2 Build nng/extern/nng-wolfssl TLS adapter
	################################################
	echo "Building extern/nng-wolfssl with SNI patch"
	# Only wolfssl supports TLS1.3, not the default mbedTLS
	cd $HOME/Proj/nng/extern/ && \
		rm -rf ./nng-wolfssl/ && \
		git clone 'https://github.com/staysail/nng-wolfssl.git' && \
		cd nng-wolfssl && \
		git apply $DIR/patch/nng_extern_wolfssl.diff && \
		cmake -DWOLFSSL_ROOT_DIR=$HOME/install . &&
		make && \
		cp -v CMakeFiles/nng-wolfssl.dir/wolfssl.o $HOME/install/lib/
	[ ! -f $HOME/install/lib/wolfssl.o ] && \
		echo "Failed to create ~/Proj/install/lib/wolfssl.o" && exit 1

	################################################
	# Step 3 Apply patch to add LDFLAG of WolfTLS
	################################################
	cd $HOME/Proj/nng/ && \
		git init && \
		git add CMakeLists.txt src/supplemental/tls/wolfssl/CMakeLists.txt && \
		git apply $DIR/patch/nng_cmakelists.$os.diff
	[[ $os == Linux ]] && cp -rv $HOME/install/lib/wolfssl.o /tmp/

	################################################
	# Step 4 Build NNG with WolfTLS
	################################################
	echo "Building nng1.5.2 with wolfssl"
	cd $HOME/Proj/nng/
	rm -rf build && mkdir build && cd build

	echo cmake -DNNG_ENABLE_TLS=ON -DNNG_TLS_ENGINE=wolf \
		-DWOLFSSL_ROOT_DIR=$HOME/install \
		--install-prefix=$HOME/install ..
	(
	
	cmake -DNNG_ENABLE_TLS=ON -DNNG_TLS_ENGINE=wolf \
		-DWOLFSSL_ROOT_DIR=$HOME/install \
		--install-prefix=$HOME/install .. || \
	cmake -DNNG_ENABLE_TLS=ON -DNNG_TLS_ENGINE=wolf \
		-DWOLFSSL_ROOT_DIR=$HOME/install \
		--install-prefix=$HOME/install .. 
	) && make && cmake --install . --prefix $HOME/install

	[[ $? != 0 ]] && echo "nng build failed" && exit 1
fi
if [ ! -f $HOME/install/include/nng/nng.h ]; then
	echo "nng build failed"
	exit 1
else
	echo "nng.h checked"
fi

