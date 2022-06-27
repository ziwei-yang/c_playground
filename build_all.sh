#!/bin/bash --login
SOURCE="${BASH_SOURCE[0]}"
DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"

cd $DIR
for f in $DIR/* ; do
	[[ $f != *_ws.c ]] && continue
	[[ $f == nng_ws.c ]] && continue
	f=$( basename $f )
	echo "./build.sh $f -o tmp/$f"
	./build.sh $f -o tmp/$f
	[[ $? != 0 ]] && echo "Failed" && exit 1
done

for f in shmutil.c mkt_viewer.c ; do
	echo "./build.sh $f -o tmp/$f"
	./build.sh $f -o tmp/$f
	[[ $? != 0 ]] && echo "Failed" && exit 1
done

cd $DIR/ruby_ext
echo "rvm use 3.0 ; ruby extconf.rb ; make"
rvm use 3.0 && ruby extconf.rb && make
