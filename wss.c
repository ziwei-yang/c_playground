#ifndef URANUS_WSS
#define URANUS_WSS

#define URN_FATAL(msg, ret) fprintf (stderr, "%s:%d %s(): FATAL %s\n", __FILE__, __LINE__, __FUNCTION__,  msg), exit(ret), ret;
#define URN_FATAL_NNG(ret)  fprintf (stderr, "%s:%d %s(): FATAL %s\n", __FILE__, __LINE__, __FUNCTION__,  nng_strerror(ret)), exit(ret), ret;

struct timeval global_log_tv;
#define URN_DEBUG(msg) gettimeofday(&global_log_tv, NULL), \
	fprintf (stdout, "DBG %02lu:%02lu:%02lu.%06u %16s:%-4d: %s\n", \
			(global_log_tv.tv_sec % 86400)/3600, \
			(global_log_tv.tv_sec % 3600)/60, \
			(global_log_tv.tv_sec % 60), \
			global_log_tv.tv_usec, \
			__FILE__, __LINE__, msg);

// low level aio operation: wait finished and check status.
int nngaio_wait_res(nng_aio *aio, char *start, char *done) {
	if (start != NULL) URN_DEBUG(start);
	nng_aio_wait(aio);
	int rv = nng_aio_result(aio);
	if (done != NULL) URN_DEBUG(done);
	return rv;
}

// low level aio operation: init nng_aio and nng_iov if is NULL, set s as iov_buf
int nngaio_init(char *s, int slen, nng_aio **aiop, nng_iov **iovp) {
	int rv = 0;
	int need_free_aiop = (*aiop == NULL);
	int need_free_iovp = (*iovp == NULL);

	if (need_free_aiop && ((rv = nng_aio_alloc(aiop, NULL, NULL)) != 0))
		return rv;
	if (need_free_iovp) {
		(*iovp) = malloc(sizeof(nng_iov));
		if (*iovp == NULL) return NNG_ENOMEM;
	}

	(*iovp)->iov_buf = s;
	(*iovp)->iov_len = slen;

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
	return 0;
}

// Init dialer and its aio, setup TLS automatically
// open stream asynchronously
int urn_ws_init(char *uri, nng_stream_dialer **d, nng_aio **daio) {
	int rv = 0;

	if ((rv = nng_stream_dialer_alloc(d, uri)) != 0 ||
			(rv = nng_stream_dialer_set_bool(*d, NNG_OPT_WS_SEND_TEXT, true)) != 0 ||
			(rv = nng_stream_dialer_set_bool(*d, NNG_OPT_WS_RECV_TEXT, true)) != 0 ||
			(rv = nng_aio_alloc(daio, NULL, NULL)) != 0)
		return rv;

	if (strncmp("wss", uri, 3) == 0) {
		nng_tls_config *tls_cfg;
		if ((rv = nng_tls_config_alloc(&tls_cfg, NNG_TLS_MODE_CLIENT)) != 0 ||
				((rv = nng_tls_config_auth_mode(tls_cfg, NNG_TLS_AUTH_MODE_NONE)) != 0) ||
				((rv = nng_stream_dialer_set_ptr(*d, NNG_OPT_TLS_CONFIG, tls_cfg)) != 0))
			return rv;
	}
	// asynchronously establish conn
	nng_stream_dialer_dial(*d, *daio);
	return 0;
}

// Wait ws dialer operation finished, get ws stream to send/recv
int urn_ws_wait_stream(nng_aio *daio, nng_stream **stream) {
	int rv = 0;
	nng_aio_wait(daio);
	if ((rv = nng_aio_result(daio)) != 0)
		return rv;

	*stream = nng_aio_get_output(daio, 0);
	return 0;
}

int urn_ws_send_async(nng_stream *strm, char *s, int slen, nng_aio **aiop, nng_iov **iovp) {
	URN_DEBUG("--> init req");
	int rv = 0;
	if ((rv = nngaio_init(s, slen, aiop, iovp)) != 0)
		return rv;
	// asynchronously send
	URN_DEBUG("--> async req");
	URN_DEBUG(s);
	nng_stream_send(strm, *aiop);
	return 0;
}

int urn_ws_send_sync(nng_stream *strm, char *s) {
	nng_aio *aio = NULL;
	nng_iov *iov = NULL;

	int rv = 0;
	if ((rv = urn_ws_send_async(strm, s, strlen(s), &aio, &iov)) != 0)
		return rv;

	if ((rv = nngaio_wait_res(aio, NULL, "--> async req wait done")) != 0)
		return rv;

	if (aio != NULL) nng_aio_free(aio);
	if (iov != NULL) free(iov);
	return rv;
}

int urn_ws_recv_async(nng_stream *strm, char *buf, int maxlen, nng_aio **aiop, nng_iov **iovp) {
	URN_DEBUG("<-- init recv");
	int rv = 0;
	if ((rv = nngaio_init(buf, maxlen, aiop, iovp)) != 0)
		return rv;

	// asynchronously send
	URN_DEBUG("<-- async recv");
	nng_stream_recv(strm, *aiop);
	return 0;
}

int urn_ws_recv_sync(nng_stream *strm, char *buf, int buflen) {
	nng_aio *aio = NULL;
	nng_iov *iov = NULL;

	int rv = 0;
	if ((rv = urn_ws_recv_async(strm, buf, buflen, &aio, &iov)) != 0)
		return rv;

	if ((rv = nngaio_wait_res(aio, "<-- async recv wait", "<-- async recv wait done")) != 0)
		return rv;

	if (aio != NULL) nng_aio_free(aio);
	if (iov != NULL) free(iov);
	return rv;
}

#endif
