#!/bin/bash --login
SOURCE="${BASH_SOURCE[0]}"
DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"

cd $DIR
mkdir -p $DIR/tmp
for f in $DIR/* ; do
	[[ $f != *_ws.c ]] && continue
	[[ $f == nng_ws.c ]] && continue
	f=$( basename $f )
	echo ./build.sh urn.c $f -o tmp/$f
	./build.sh urn.c $f -o tmp/$f
	[[ $? != 0 ]] && echo "Failed" && exit 1
done

for f in shmutil.c mkt_viewer.c ; do
	echo ./build.sh urn.c $f -o tmp/$f
	./build.sh urn.c $f -o tmp/$f
	[[ $? != 0 ]] && echo "Failed" && exit 1
done

echo "Checking RVM"
which rvm
if [[ $? == 0 ]]; then
	cd $DIR/ruby_ext_mktdata
	ln -sf ../urn.h ./urn.h
	ln -sf ../urn.c ./urn.c
	echo "rvm use 3.0 ; ruby extconf.rb ; make clean; make"
	rvm use 3.0 && ruby extconf.rb && make clean && make
else
	echo "rvm not exist, skip building ruby ext"
fi
