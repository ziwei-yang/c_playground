#!/bin/bash --login
SOURCE="${BASH_SOURCE[0]}"
DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"

[ -z $1 ] && echo "No file given." && exit 1
c_file=$( basename $1 )
shift

cd $DIR
mkdir -p $DIR/tmp
echo $DIR/build.sh urn.c $c_file -o $DIR/tmp/$c_file

[ -f $DIR/env.sh ] && source $DIR/env.sh
$DIR/build.sh urn.c $c_file -o $DIR/tmp/$c_file && echo $DIR/tmp/$c_file $@ && $DIR/tmp/$c_file $@

rm $DIR/tmp/$c_file
