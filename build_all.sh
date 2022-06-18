#!/bin/bash --login
SOURCE="${BASH_SOURCE[0]}"
DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"

cd $DIR
for f in $DIR/* ; do
	[[ $f != *_ws.c ]] && continue
	[[ $f == nng_ws.c ]] && continue
	f=$( basename $f )
	echo "Building $f"
	$DIR/build.sh $f
	[[ $? != 0 ]] && echo "Failed" && exit 1
done
