gcc \
	-I /usr/local/include \
	-I /Users/zwyang/install/include/ \
	-I /opt/homebrew/Cellar/mbedtls/3.1.0/include/ \
	-L /usr/local/lib \
	-L /Users/zwyang/install/lib/ \
	-L /usr/local/ \
	-L /opt/homebrew/Cellar/mbedtls/3.1.0/lib \
	-lnng -lpthread -lmbedtls -lmbedx509 -lmbedcrypto \
	$@
