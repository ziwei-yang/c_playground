#ifndef URANUS_WSS
#define URANUS_WSS

#include "urn.h"

#include <nng/nng.h>
#include <nng/supplemental/http/http.h>
#include <nng/supplemental/tls/tls.h>
#include <nng/supplemental/tls/engine.h>
#include <nng/transport/tls/tls.h>
#include <nng/transport/ws/websocket.h>

// low level aio operation: wait finished and check status.
int nngaio_wait_res(nng_aio *aio, size_t *ct, char *start, char *done) {
	if (start != NULL) URN_DEBUG(start);
	nng_aio_wait(aio);
	int rv = nng_aio_result(aio);
	if (done != NULL) URN_DEBUG(done);
	*ct = nng_aio_count(aio);
	return rv;
}

// low level aio operation: recv, wait finished and check status.
// Use this to recv data if want to reuse aio
int nngaio_recv_wait_res(nng_stream *stream, nng_aio *aio, nng_iov *iov, size_t *ct, char *start, char *done) {
	if (start != NULL) URN_DEBUG(start);
	int rv = 0;
	// reset iov from start pos
	if ((rv = nng_aio_set_iov(aio, 1, iov)) != 0)
		return rv;
	nng_stream_recv(stream, aio);
	nng_aio_wait(aio);
	rv = nng_aio_result(aio);
	*ct = nng_aio_count(aio);
	if (done != NULL) URN_LOGF("%s bytes %zu", done, *ct);
	return rv;
}

// low level aio operation: recv, wait finished and check status.
// Use this to send data if want to reuse aio
int nngaio_send_wait_res(nng_stream *stream, nng_aio *aio, nng_iov *iov, size_t *ct, char *start, char *done) {
	if (start != NULL) URN_DEBUG(start);
	int rv = 0;
	// reset iov from start pos
	if ((rv = nng_aio_set_iov(aio, 1, iov)) != 0)
		return rv;
	nng_stream_send(stream, aio);
	nng_aio_wait(aio);
	rv = nng_aio_result(aio);
	if (done != NULL) URN_DEBUG(done);
	*ct = nng_aio_count(aio);
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
int urn_ws_init_with_headers(char *uri, char **headers,  nng_stream_dialer **d, nng_aio **daio) {
	int rv = 0;

	nng_url *url;
	if (((rv = nng_url_parse(&url, uri)) != 0))
		return URN_FATAL_NNG(rv);

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
		// set server name for wolfssl add SNI extension in TLS handshake
		// Need for Bybit and FTX
		nng_tls_config_server_name(tls_cfg, url->u_hostname);
	}

	// customized headers.
	while (headers != NULL && *headers != NULL) {
		char *key = *(headers++);
		char *val = *(headers++);
		char setkey[64+strlen(key)]; // concat NNG_OPT_WS_REQUEST_HEADER+key
		strcpy(setkey, NNG_OPT_WS_REQUEST_HEADER);
		strcpy(setkey + strlen(NNG_OPT_WS_REQUEST_HEADER), key);
		URN_DEBUGF("wss headers setkey %s", setkey);
		URN_DEBUGF("wss headers val    %s", val);
		rv = nng_stream_dialer_set_string(*d, setkey, val);
		if (rv != 0) {
			URN_WARN("failed in nng_stream_dialer_set_ptr");
			return rv;
		}
	}

	// asynchronously establish conn
	nng_stream_dialer_dial(*d, *daio);
	return 0;
}
int urn_ws_init(char *uri, nng_stream_dialer **d, nng_aio **daio) {
	return urn_ws_init_with_headers(uri, NULL, d, daio);
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
	URN_DEBUGF("--> async req: %s", s);
	nng_stream_send(strm, *aiop);
	return 0;
}

int urn_ws_send_sync(nng_stream *strm, char *s) {
	nng_aio *aio = NULL;
	nng_iov *iov = NULL;

	int rv = 0;
	size_t ct = 0; // useless
	if ((rv = urn_ws_send_async(strm, s, strlen(s), &aio, &iov)) != 0)
		return rv;

	if ((rv = nngaio_wait_res(aio, &ct, NULL, "--> async req wait done")) != 0)
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

int urn_ws_recv_sync(nng_stream *strm, char *buf, int buflen, size_t *ct) {
	nng_aio *aio = NULL;
	nng_iov *iov = NULL;

	int rv = 0;
	if ((rv = urn_ws_recv_async(strm, buf, buflen, &aio, &iov)) != 0)
		return rv;

	if ((rv = nngaio_wait_res(aio, ct, "<-- async recv wait", "<-- async recv wait done")) != 0)
		return rv;

	if (aio != NULL) nng_aio_free(aio);
	if (iov != NULL) free(iov);
	return rv;
}

// nng/https but with HTTP-Chunked-Encoding support
int nng_https_get(char *uri, uint16_t *code, char **response, size_t *reslen) {
	int rv = 0;
	nng_http_client *client = NULL;
	nng_url *        url = NULL;
	nng_http_req *   req = NULL;
	nng_http_res *   res = NULL;
	nng_aio *        aio = NULL;
	const char *     hdr = NULL;
	bool need_free_data = true;
	void *data  = NULL;

	URN_DEBUGF("nng_https %s", uri);
	if (((rv = nng_url_parse(&url, uri)) != 0) ||
			((rv = nng_http_client_alloc(&client, url)) != 0) ||
			((rv = nng_http_req_alloc(&req, url)) != 0) ||
			((rv = nng_http_res_alloc(&res)) != 0) ||
			((rv = nng_aio_alloc(&aio, NULL, NULL)) != 0)) {
		goto final;
	}
	// additional TLS conf.
	if (strncmp("https", url->u_scheme, 5) == 0) {
		nng_tls_config *tls_cfg;
		nng_http_client_get_tls(client, &tls_cfg);
		nng_tls_config_auth_mode(tls_cfg, NNG_TLS_AUTH_MODE_NONE);
		nng_tls_config_free(tls_cfg);
	}

	// connect then wait it done.
	nng_http_client_connect(client, aio);
	nng_aio_wait(aio);
	if ((rv = nng_aio_result(aio)) != 0)
		goto final;

	// add custom headers
	nng_http_req_add_header(req, "User-Agent", "curl/7.79.1");
	nng_http_req_add_header(req, "Accept", "*/*");

	// request then wait it done.
	nng_http_conn *conn = nng_aio_get_output(aio, 0);
	nng_http_conn_write_req(conn, req, aio);
	nng_aio_wait(aio);

	// wait till all res ready.
	nng_http_conn_read_res(conn, res, aio);
	nng_aio_wait(aio);
	if ((rv = nng_aio_result(aio)) != 0)
		goto final;

	// Parse response header.
	int              len;
	int 		 chunk_state = -1; // <0: not chunk mode; 0: chunk len scanning; 1: chunk content scanning; 2: end
	*code = nng_http_res_get_status(res);
	hdr = nng_http_res_get_header(res, "Content-Length");
	if (hdr == NULL) {
		URN_DEBUG("Missing Content-Length header. Use 10 bytes to get 1st chunk");
		len = 10;
		chunk_state = 0; // read len; calculate left size; read left and little bit more; repeat.
	} else {
		len = atoi(hdr); // read once
		URN_DEBUGF("Content-Length %d", len);
		if (len == 0) {
			// Content-Length zero, return
			char *r = malloc(1);
			*r = '\0';
			*response = r;
			goto final;
		}
	}

	nng_iov          iov;
	// Set up a single iov to point to the buffer.
	URN_DEBUGF("nng_https allocates %d buffer to receive the body.", len);
	data  = malloc(len);
	iov.iov_len = len;
	iov.iov_buf = data;
	nng_aio_set_iov(aio, 1, &iov);
	nng_http_conn_read_all(conn, aio);

	int scan_p = 0; // data scan pos
	char prev_c = 0, c = 0; // last scanned char

	int chunk_parsing_char_ct = 0;
	int chunk_len = 0; // current chunk length
	char* chunk_array[8192]; // total chunks
	int chunk_idx = 0; // current chunk index
	char *cur_buffer; // for current chunk_array[chunk_idx]
	int buf_w_offset = 0; // writing pos in cur_buffer
	size_t chunk_len_sum = 0;

	do {
		URN_DEBUG("nng_https wait for data read to complete.");
		nng_aio_wait(aio);

		rv = nng_aio_result(aio);
		URN_DEBUGF("nng_aio_result(aio)=%d", rv);
		if (rv != 0)
			break;

		if (chunk_state < 0) { // Not chunk mode.
			need_free_data = false;
			*response = iov.iov_buf;
			*reslen = iov.iov_len;
			URN_DEBUGF("Not chunk mode, all data got %lu", *reslen);
			chunk_idx = 0; // dont copy data, return response directly.
			rv = 0;
			break;
		}

		URN_DEBUGF("nng_https chunk %d parsing", chunk_idx);
		for (scan_p = 0; scan_p < len; scan_p++) {
			prev_c = c;
			c = *(char*)(data+scan_p);
			if (chunk_state == 0) { // chunk len scannig.
				chunk_parsing_char_ct += 1;
				if (c == 13) {
					URN_DEBUGF("! LEN %d << %d/%d got \\r", chunk_len, scan_p, len);
					continue; // got \r, next to scan \n
				} else if (prev_c == 13 && c == 10) {
					// \r\n got, chunk_len calculated
					URN_DEBUGF("! LEN %d << %d/%d got \\n", chunk_len, scan_p, len);
					if (chunk_len == 0) {
						chunk_state = 2; // -> end
						URN_DEBUGF("ZERO length chunk found, chunk num %d", chunk_idx);
						break;
					} else {
						cur_buffer = malloc(chunk_len+1); // 1 more for \0
						buf_w_offset = 0;
						chunk_state = 1; // -> content scanning
					}
				} else if ((c <= 'f' && c >= 'a') || (c <= '9' && c >= '0') || (c <= 'F' && c >= 'A')) {
					// calculate header length HEX chars and scan for \r\n
					int hex_digit;
					if (c <= 'f' && c >= 'a') hex_digit = (int)c - (int)('a') + 10;
					if (c <= 'F' && c >= 'A') hex_digit = (int)c - (int)('A') + 10;
					if (c <= '9' && c >= '0') hex_digit = (int)c - (int)('0');
					chunk_len = (chunk_len << 4) + hex_digit;
					URN_DEBUGF("? LEN %d << %d/%d %c [%d]", chunk_len, scan_p, len, c, hex_digit);
				} else {
					URN_WARNF("X LEN %d << %d/%d %c(%d)]", chunk_len, scan_p, len, c, (int)c);
					URN_WARN("nng_https chunked-encoding header: unexpected char");
					rv = ERANGE;
					break;
				}
			} else if (chunk_state == 1) { // content scanning and copying.
				if (buf_w_offset < chunk_len) {
					int data_left = len - scan_p;
					int bufr_left = chunk_len - buf_w_offset;
					int copy_ct = (data_left < bufr_left) ? data_left : bufr_left;
					URN_DEBUGF(">>> copy buffer[%d + %d] << data[%d + %d]", buf_w_offset, copy_ct, scan_p, copy_ct);
					memcpy(cur_buffer+buf_w_offset, data+scan_p, copy_ct);
					buf_w_offset += copy_ct;
					scan_p += copy_ct;
				} else {
					*(cur_buffer+buf_w_offset) = '\0'; // add \0 to cur_buffer
					chunk_len_sum += buf_w_offset;
					URN_DEBUGF(">>> copy buffer[%d] finished, idx %d, total len: %lu", buf_w_offset, chunk_idx, chunk_len_sum);
					chunk_array[chunk_idx++] = cur_buffer;
					cur_buffer = 0;
					// finish this chunk copying, ready to start next chunk parsing.
					prev_c = 0;
					chunk_state = 0;
					chunk_len = 0;
					chunk_parsing_char_ct = 0;
				}
			} else {
				URN_WARN("nng_https chunked-encoding wrong chunk state");
				rv = ERANGE;
				break;
			}
		}

		free(data); data = NULL;
		if (chunk_state == 0) {
			if (chunk_parsing_char_ct < 4)
				len = 5-chunk_parsing_char_ct; // min 5: 0\r\n\r\n
			else
				len = 1;
			URN_DEBUGF("Missed char at parsing chunk len, receive %d more byte", len);
		} else if (chunk_state == 1) {
			int missed_ct = chunk_len - buf_w_offset;
			missed_ct += 2; // \r\n
			missed_ct += 5; // ABCD\r\n 6, 0\r\n\r\n 5
			len = missed_ct;
			URN_DEBUGF("Missed chunk char ct %d, allocate %d buffer to receive remained data + next len.", (chunk_len-buf_w_offset), len);
		} else if (chunk_state == 2) {
			URN_DEBUG("nng_https chunked-encoding parsing done");
			break;
		} else {
			URN_WARN("nng_https chunked-encoding wrong chunk state");
			rv = ERANGE;
			break;
		}
		data = malloc(len);
		// Set up a single iov to point to the buffer.
		iov.iov_len = len;
		iov.iov_buf = data;
		nng_aio_set_iov(aio, 1, &iov);

		// Now attempt to receive next chunk.
		nng_http_conn_read_all(conn, aio);
	} while(1);

final:
	if (aio != NULL) nng_aio_free(aio);
	if (res != NULL) nng_http_res_free(res);
	if (req != NULL) nng_http_req_free(req);
	if (client != NULL) nng_http_client_free(client);
	if (need_free_data && (data != NULL)) free(data);
	if (rv != 0)
		return URN_FATAL("Error in nng_https", rv);

	if (chunk_state < 0) {
		// Not chunk mode. Don't copy data, return response directly.
		return rv;
	}

	// merge and print every chunk.
	char *res_s = malloc(chunk_len_sum);
	size_t res_pos = 0;
	for (int idx = 0; idx < chunk_idx; idx++) {
		strcpy(res_s+res_pos, chunk_array[idx]);
		// URN_DEBUGF("chunk %d/%d: len %lu\n%s", idx, chunk_idx-1, strlen(chunk_array[idx]), chunk_array[idx]);
		res_pos += strlen(chunk_array[idx]);
	}
	URN_DEBUGF("size_sum %lu", chunk_len_sum);
	if (res_pos == chunk_len_sum) {
		*response = res_s;
		*reslen = chunk_len_sum;
	} else {
		URN_WARNF("total cp %lu != size_sum %lu", res_pos, chunk_len_sum);
		rv = EINVAL;
	}
	return rv;
}

#endif
