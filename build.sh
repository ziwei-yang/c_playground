gcc \
	-I /usr/local/include \
	-I $HOME/install/include \
	-I /opt/homebrew/Cellar/mbedtls/3.1.0/include \
	-I /opt/homebrew/Cellar/glib/2.72.1/include/glib-2.0 \
	-I /opt/homebrew/Cellar/glib/2.72.1/lib/glib-2.0/include/\
	-I /opt/homebrew/Cellar/hiredis/1.0.2/include/hiredis \
	-I /opt/homebrew/Cellar/hiredis/1.0.2/include/hiredis/adapters \
	-L /usr/local/lib \
	-L $HOME/install/lib \
	-L /opt/homebrew/Cellar/mbedtls/3.1.0/lib \
	-L /opt/homebrew/Cellar/glib/2.72.1/lib \
	-L /opt/homebrew/Cellar/hiredis/1.0.2/lib \
	-lnng -lpthread -lmbedtls -lmbedx509 -lmbedcrypto -lglib-2.0 -lhiredis \
	$@
