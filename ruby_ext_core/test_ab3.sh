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

rvm use 3.0

cd $DIR/../ruby_ext_mktdata/
ruby ./extconf.rb && \
	pwd &&
	make clean && make install && \
	cd $DIR \
	pwd &&
	ruby ./extconf.rb && \
	make clean && make install && \
	ruby ./test_ab3.rb

# rm $DIR/conf/key.sh.gpg
# rm $DIR/conf/env.sh
# rm $DIR/conf/env2.sh
