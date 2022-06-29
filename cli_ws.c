#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <time.h>

#include <nng/nng.h>
#include <nng/supplemental/tls/tls.h>

#include "urn.h"
#include "wss.h"
#include "hmap.h"

int main(int argc, char **argv) {
	URN_INFOF("nng_tls_engine_name %s", nng_tls_engine_name());
	URN_INFOF("nng_tls_engine_description %s", nng_tls_engine_description());

	if (argc < 1)
		return URN_FATAL("No URL given.", EINVAL);

	char wss_uri[1024];
	int rv = 0;
	strcpy(wss_uri, argv[1]);
	// wss_url must ends with / if no path given.
	int url_slen = strlen(wss_uri);
	int slash_ct = 0;
	for (int i=0; i<url_slen; i++)
		if (wss_uri[i] == '/')
			slash_ct ++;
	if (slash_ct < 3)
		return URN_FATAL("Make sure wss_url has a path", EINVAL);

	nng_aio *dialer_aio = NULL;
	nng_stream_dialer *dialer = NULL;
	nng_stream *stream = NULL;

	int recv_buflen = 4*1024*1024;
	char *recv_buf = malloc(recv_buflen);
	if (recv_buf == NULL)
		return URN_FATAL("Not enough memory for wss recv_buf", ENOMEM);

	nng_aio *recv_aio = NULL;
	nng_iov *recv_iov = NULL;
	size_t recv_bytes = 0;

	URN_INFOF("Init wss conn %s", wss_uri);
	if ((rv = urn_ws_init(wss_uri, &dialer, &dialer_aio)) != 0)
		return URN_FATAL_NNG(rv);

	URN_INFO("Wait conn stream");
	if ((rv = urn_ws_wait_stream(dialer_aio, &stream)) != 0)
		return URN_FATAL_NNG(rv);
	URN_INFO("Wss conn stream is ready");

	// Reuse iov buffer and aio to receive data.
	if ((rv = nngaio_init(recv_buf, recv_buflen, &recv_aio, &recv_iov)) != 0)
		return URN_FATAL_NNG(rv);
	URN_INFO("Recv aio and iov ready");

	size_t max_len=8192;
	char *cmd = malloc(max_len);
	while(true) {
		if ((rv = nngaio_recv_wait_res(stream, recv_aio, recv_iov, &recv_bytes, NULL, NULL)) != 0)
		{
			URN_WARN("Error in nngaio_recv_wait_res()");
			return URN_FATAL_NNG(rv);
		}
		recv_buf[recv_bytes] = '\0';
		URN_INFOF("on_wss_msg %lu/%d:\n%s", recv_bytes, recv_buflen, recv_buf);

		printf("Enter new command:");
		size_t cmd_len = getline(&cmd, &max_len, stdin);
		if (strlen(cmd) <= 1) continue;

		URN_INFOF("Get %lu/%lu bytes", cmd_len, max_len);
		URN_INFOF("Sending %lu bytes: %s", strlen(cmd), cmd);
		if ((rv = urn_ws_send_sync(stream, cmd)) != 0)
			return URN_FATAL_NNG(rv);
	}
	URN_INFO("wss_connect finished");
	free(cmd);
	return 0;
}
