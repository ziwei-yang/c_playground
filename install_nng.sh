# For NNG v1.5.2.tar.gz on macos m1
cd $HOME/Proj/nng/
rm -rf build

brew install mbedTLS

mkdir build && \
	cmake -DNNG_ENABLE_TLS=ON --install-prefix=$HOME/install  .. && \
	make && \
	make install
