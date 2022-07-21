#!/bin/bash --login
SOURCE="${BASH_SOURCE[0]}"
DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"

cd $DIR

# source $DIR/../../uranus/conf/dummy_env.sh
mkdir $DIR/conf
cp $DIR/../../uranus/conf/key.sh.gpg $DIR/conf/
cp $DIR/../../uranus/conf/env.sh $DIR/conf/
cp $DIR/../../uranus/conf/env2.sh $DIR/conf/
source $DIR/conf/env.sh KEY
export URANUS_DRYRUN=1

cd $DIR
rvm use 3.0
ruby $DIR/extconf.rb && \
	make clean && make install && \
	ruby ./test_ab3.rb

rm $DIR/conf/key.sh.gpg
rm $DIR/conf/env.sh
rm $DIR/conf/env2.sh
