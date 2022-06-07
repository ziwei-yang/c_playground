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

#define FATAL(msg, ret) fprintf (stderr, "%s:%d %s(): FATAL %s\n", __FILE__, __LINE__, __FUNCTION__,  msg), exit(ret), ret;
#define FATAL_NNG(ret)  fprintf (stderr, "%s:%d %s(): FATAL %s\n", __FILE__, __LINE__, __FUNCTION__,  nng_strerror(ret)), exit(ret), ret;

struct timeval global_log_tv;
#define DEBUG(msg) gettimeofday(&global_log_tv, NULL), \
	fprintf (stdout, "DBG %02lu:%02lu:%02lu.%06u %16s:%-4d: %s\n", \
			(global_log_tv.tv_sec % 86400)/3600, \
			(global_log_tv.tv_sec % 3600)/60, \
			(global_log_tv.tv_sec % 60), \
			global_log_tv.tv_usec, \
			__FILE__, __LINE__, msg);

// low level aio operation, wait finished and check status.
int ws_wait_aio(nng_aio *aio, char *start, char *done) {
	if (start != NULL) DEBUG(start);
	nng_aio_wait(aio);
	int rv = nng_aio_result(aio);
	if (done != NULL) DEBUG(done);
	return rv;
}

// Init dialer and its aio, open stream asynchronously
int ws_init(char *uri, nng_stream_dialer **d, nng_aio **daio) {
	int rv = 0;

	if ((rv = nng_stream_dialer_alloc(d, uri)) != 0 ||
			(rv = nng_stream_dialer_set_bool(*d, NNG_OPT_WS_SEND_TEXT, true)) != 0 ||
			(rv = nng_stream_dialer_set_bool(*d, NNG_OPT_WS_RECV_TEXT, true)) != 0 ||
			(rv = nng_aio_alloc(daio, NULL, NULL)) != 0)
		return rv;
	// asynchronously establish conn
	nng_stream_dialer_dial(*d, *daio);
	return 0;
}

// Wait ws dialer operation finished, get ws stream to send/recv
int ws_wait_stream(nng_aio *daio, nng_stream **stream) {
	int rv = 0;
	nng_aio_wait(daio);
	if ((rv = nng_aio_result(daio)) != 0)
		return rv;

	*stream = nng_aio_get_output(daio, 0);
	return 0;
}

int ws_send_async(nng_stream *strm, char *s, nng_aio **aiop, nng_iov **iovp) {
	DEBUG("--> init req");
	int rv = 0;
	int need_free_aiop = (*aiop == NULL);
	int need_free_iovp = (*iovp == NULL);

	if (need_free_aiop && ((rv = nng_aio_alloc(aiop, NULL, NULL)) != 0))
		return rv;
	if (need_free_iovp) {
		(*iovp) = malloc(sizeof(nng_iov));
		if (*iovp == NULL)
			return NNG_ENOMEM;
	}

	(*iovp)->iov_buf = s;
	(*iovp)->iov_len = strlen(s);

	if ((rv = nng_aio_set_iov(*aiop, 1, *iovp)) != 0) {
		if (need_free_aiop) {
			nng_aio_free(*aiop);
			*aiop = NULL;
		}
		if (need_free_iovp) {
			free(*iovp);
			*iovp = NULL;
		}
		return rv;
	}
	// asynchronously send
	DEBUG("--> async req");
	DEBUG(s);
	nng_stream_send(strm, *aiop);
	return 0;
}

int ws_send_sync(nng_stream *strm, char *s) {
	nng_aio *aio = NULL;
	nng_iov *iov = NULL;

	int rv = 0;
	rv = ws_send_async(strm, s, &aio, &iov);
	if (rv != 0)
		return rv;

	rv = ws_wait_aio(aio, NULL, "--> async req wait done");

	nng_aio_free(aio);
	free(iov);
	return rv;
}

int ws_recv_async(nng_stream *strm, char *buf, int maxlen, nng_aio **aiop, nng_iov **iovp) {
	DEBUG("<-- init recv");
	int rv = 0;
	int need_free_aiop = (*aiop == NULL);
	int need_free_iovp = (*iovp == NULL);

	if (need_free_aiop && ((rv = nng_aio_alloc(aiop, NULL, NULL)) != 0))
		return rv;
	if (need_free_iovp) {
		(*iovp) = malloc(sizeof(nng_iov));
		if (*iovp == NULL)
			return NNG_ENOMEM;
	}

	(*iovp)->iov_buf = buf;
	(*iovp)->iov_len = maxlen;

	if ((rv = nng_aio_set_iov(*aiop, 1, *iovp)) != 0) {
		if (need_free_aiop) {
			nng_aio_free(*aiop);
			*aiop = NULL;
		}
		if (need_free_iovp) {
			free(*iovp);
			*iovp = NULL;
		}
		return rv;
	}
	// asynchronously send
	DEBUG("<-- async recv");
	nng_stream_recv(strm, *aiop);
	return 0;
}
int ws_recv_sync(nng_stream *strm, char *buf, int buflen) {
	nng_aio *aio = NULL;
	nng_iov *iov = NULL;

	int rv = 0;
	rv = ws_recv_async(strm, buf, buflen, &aio, &iov);
	if (rv != 0)
		return rv;

	rv = ws_wait_aio(aio, "<-- async recv", "<-- async recv wait done");

	nng_aio_free(aio);
	free(iov);
	return rv;
}

int main() {
	char *uri = "ws://localhost:8080/path/";
	nng_aio *dialer_aio = NULL;
	nng_stream_dialer *dialer = NULL;
	nng_stream *stream = NULL;

	int rv = 0;

	DEBUG("Init conn");
	if ((rv = ws_init(uri, &dialer, &dialer_aio)) != 0)
		return FATAL_NNG(rv);

	DEBUG("Wait conn stream");
	if ((rv = ws_wait_stream(dialer_aio, &stream)) != 0)
		return FATAL_NNG(rv);
	DEBUG("Conn stream is ready");

	char *req_s = "hello this is a req";
	DEBUG("Send req async");
	if ((rv = ws_send_sync(stream, req_s)) != 0)
		return FATAL_NNG(rv);

	int buflen = 1024;
	char buf[buflen];
	if ((rv = ws_recv_sync(stream, buf, buflen)) != 0)
		return FATAL_NNG(rv);
	DEBUG(buf);

	return 0;
}
