# For NNG v1.5.2.tar.gz on macos m1
cd $HOME/Proj/nng/
rm -rf build
mkdir build
cd build

brew install mbedTLS # sudo apt install libmbedtls-dev

cmake -DNNG_ENABLE_TLS=ON --install-prefix=$HOME/install  .. && \
	make && \
	make install
