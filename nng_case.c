#include <nng/nng.h>
#include <nng/supplemental/http/http.h>
#include <nng/supplemental/tls/tls.h>
#include <nng/supplemental/tls/engine.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#define EPRINT(msg) fprintf (stderr, "%s:%d %s(): %s\n", __FILE__, __LINE__, __FUNCTION__,  msg);
#define DEBUG(msg) fprintf (stdout, "%s:%d %s(): %s\n", __FILE__, __LINE__, __FUNCTION__,  msg);

#include "util.c"
char *load_file(char *fpath); // in util.c

int fatal(int rv) {
	EPRINT(nng_strerror(rv));
	exit(rv);
	return rv;
}

int main() {
	nng_stream_dialer *dialer;
	nng_aio *aio;
	nng_stream *stream;
	int rv;
	nng_tls_config *cfg;
	nng_url *url;

	char *ca;
	char fpath[] = "./ftx-com-chain.pem";
	gsize clen;
	GError *error;
	gboolean ret = g_file_get_contents(fpath, &ca, &clen, &error);
	DEBUG("STEP0");
	printf("CA load, len: %lu\n", clen);

	DEBUG("STEP1");
	if (((rv = nng_url_parse(&url, "wss://ftx.com/ws/")) != 0) ||
		((rv = nng_stream_dialer_alloc_url(&dialer, url)) != 0) ||
		((rv = nng_aio_alloc(&aio, NULL, NULL)) != 0) ||
		((rv = nng_tls_config_alloc(&cfg, NNG_TLS_MODE_CLIENT)) != 0) ||
		((rv = nng_tls_config_ca_chain(cfg, ca, NULL)) != 0) ||
		((rv = nng_tls_config_auth_mode(cfg, NNG_TLS_AUTH_MODE_NONE)) != 0) ||
		((rv = nng_stream_dialer_set_ptr(dialer, NNG_OPT_TLS_CONFIG, cfg)) != 0))
		return fatal(rv);

	DEBUG("STEP2");
	nng_stream_dialer_dial(dialer, aio);
	nng_aio_wait(aio);

	DEBUG("STEP3");
	if ((rv = nng_aio_result(aio)) != 0)
		return fatal(rv);

	DEBUG("STEP4");
	if ((stream = nng_aio_get_output(aio, 0)) == NULL)
		return fatal(3);

	DEBUG("did open stream");

	const size_t buf_len = 1024;
	nng_aio *aio_req;
	nng_iov iov;
	static char read_buf[buf_len];

	if ((rv = nng_aio_alloc(&aio_req, NULL, NULL)) != 0) {
		return fatal(rv);
	}

	nng_aio_set_timeout(aio_req, NNG_DURATION_INFINITE);

	iov.iov_buf = read_buf;
	iov.iov_len = buf_len;

	if ((rv = nng_aio_set_iov(aio_req, 1, &iov)) != 0) {
		return fatal(rv);
	}

	while (true) {
		memset(read_buf, 0, buf_len);

		nng_stream_recv(stream, aio_req);
		nng_aio_wait(aio_req);

		if ((rv = nng_aio_result(aio_req)) != 0) {
			return fatal(rv);
		}

		printf("received %zu bytes\n", nng_aio_count(aio_req));
		printf("data: %s\n", read_buf);
	}
}
