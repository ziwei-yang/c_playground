#!/bin/bash --login
SOURCE="${BASH_SOURCE[0]}"
DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"

cd $DIR

rvm use 3.0
ruby $DIR/extconf.rb && \
	make install && \
	ruby ./test.rb
