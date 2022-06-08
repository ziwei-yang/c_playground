#include <nng/nng.h>
#include <nng/supplemental/http/http.h>
#include <nng/supplemental/tls/tls.h>
#include <nng/supplemental/tls/engine.h>
#include <nng/transport/tls/tls.h>
#include <nng/transport/ws/websocket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <time.h>
#include <sys/time.h>

#include "wss.c"

int main() {
	char *uri = "ws://localhost:8080/path/";
	nng_aio *dialer_aio = NULL;
	nng_stream_dialer *dialer = NULL;
	nng_stream *stream = NULL;

	int rv = 0;

	URN_DEBUG("Init conn");
	if ((rv = urn_ws_init(uri, &dialer, &dialer_aio)) != 0)
		return URN_FATAL_NNG(rv);

	URN_DEBUG("Wait conn stream");
	if ((rv = urn_ws_wait_stream(dialer_aio, &stream)) != 0)
		return URN_FATAL_NNG(rv);
	URN_DEBUG("Conn stream is ready");

	char *req_s = "hello this is a req";
	if ((rv = urn_ws_send_sync(stream, req_s)) != 0)
		return URN_FATAL_NNG(rv);

	int buflen = 1024;
	char buf[buflen];

	while(1) {
		if ((rv = urn_ws_recv_sync(stream, buf, buflen)) != 0)
			return URN_FATAL_NNG(rv);
		URN_DEBUG(buf);
	}

	return 0;
}
