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

int main(int argc, char **argv) {
	int rv = 0;
	nng_url *url = NULL;
	char *uri = argv[1];
	nng_aio *dialer_aio = NULL;
	nng_stream_dialer *dialer = NULL;
	nng_stream *stream = NULL;

	int recv_buflen = 65536;
	char recv_buf[recv_buflen];
	nng_aio *recv_aio = NULL;
	nng_iov *recv_iov = NULL;
	size_t recv_bytes = 0;

	if (argc < 2)
		return URN_FATAL("No URL supplied!", 1);

	if (((rv = nng_url_parse(&url, argv[1])) != 0))
		return URN_FATAL_NNG(rv);

	URN_DEBUG("Init conn");
	if ((rv = urn_ws_init(uri, &dialer, &dialer_aio)) != 0)
		return URN_FATAL_NNG(rv);

	URN_DEBUG("Wait conn stream");
	if ((rv = urn_ws_wait_stream(dialer_aio, &stream)) != 0)
		return URN_FATAL_NNG(rv);
	URN_DEBUG("Conn stream is ready");

//	char *req_s = "hello this is a req";
//	if ((rv = urn_ws_send_sync(stream, req_s)) != 0)
//		return URN_FATAL_NNG(rv);

	// Reuse iov buffer and aio to receive data.
	if ((rv = nngaio_init(recv_buf, recv_buflen, &recv_aio, &recv_iov)) != 0)
		return URN_FATAL_NNG(rv);
	URN_DEBUG("Recv aio and iov ready");

	while(1) {
		if ((rv = nngaio_recv_wait_res(stream, recv_aio, recv_iov, &recv_bytes, "<-- async recv wait", "<-- async recv wait done")) != 0)
			return rv;
		// only 0..recv_bytes is message received this time.
		URN_LOGF("%zu %.*s", recv_bytes, (int)recv_bytes, recv_buf);
	}

	return 0;
}
