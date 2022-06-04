#include <nng/nng.h>
#include <nng/supplemental/http/http.h>
#include <nng/supplemental/tls/tls.h>
#include <nng/supplemental/tls/engine.h>
#include <nng/transport/tls/tls.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#define FATAL(msg, ret) fprintf (stderr, "%s:%d %s(): FATAL %s\n", __FILE__, __LINE__, __FUNCTION__,  msg), exit(1), ret;

struct timeval global_log_tv;
#define DEBUG(msg) gettimeofday(&global_log_tv, NULL), \
	fprintf (stdout, "DBG %2lu:%2lu:%2lu.%6u %16s:%4d %8s(): %s\n", \
			(global_log_tv.tv_sec % 86400)/3600, \
			(global_log_tv.tv_sec % 3600)/60, \
			(global_log_tv.tv_sec % 60), \
			global_log_tv.tv_usec, \
			__FILE__, __LINE__, __FUNCTION__,  msg);

int main(int argc, char **argv) {
	nng_http_client *client;
	nng_http_conn *  conn;
	nng_url *        url;
	nng_aio *        aio;
	nng_http_req *   req;
	nng_http_res *   res;
	const char *     hdr;
	int              rv;
	int              len;
	void *           data;
	nng_iov          iov;
	int 		 chunk_state = -1; // <0: not chunk mode; 0: chunk len scanning; 1: chunk content scanning; 2: end

	if (argc < 2) {
		fprintf(stderr, "No URL supplied!\n");
		exit(1);
	}

	DEBUG("STEP0 init nng_url, nng_client, nng_http_req, nng_http_res, aio");
	if (((rv = nng_url_parse(&url, argv[1])) != 0) ||
		((rv = nng_http_client_alloc(&client, url)) != 0) ||
		((rv = nng_http_req_alloc(&req, url)) != 0) ||
		((rv = nng_http_res_alloc(&res)) != 0) ||
		((rv = nng_aio_alloc(&aio, NULL, NULL)) != 0)) {
			return FATAL(nng_strerror(rv), rv);
	}

	DEBUG("STEP1 set TLS NNG_TLS_AUTH_MODE_NONE");
	if (strncmp("https", url->u_scheme, 5) == 0) {
		nng_tls_config *tls_cfg;
		nng_http_client_get_tls(client, &tls_cfg);
		nng_tls_config_auth_mode(tls_cfg, NNG_TLS_AUTH_MODE_NONE);
		nng_tls_config_free(tls_cfg);
	}

	DEBUG("STEP2 --> Start connection process...");
	nng_http_client_connect(client, aio);

	DEBUG("STEP3 Wait for it to finish.")
	nng_aio_wait(aio);
	if ((rv = nng_aio_result(aio)) != 0)
		return FATAL(nng_strerror(rv), rv);

	DEBUG("STEP4 Get the connection, at the 0th output.");
	conn = nng_aio_get_output(aio, 0);

	// Request is already set up with URL, and for GET via HTTP/1.1.
	// The Host: header is already set up too.

	DEBUG("STEP5 --> Send the request, and wait for that to finish.");
	nng_http_conn_write_req(conn, req, aio);
	nng_aio_wait(aio);

	DEBUG("STEP6 ... aio is ready");
	if ((rv = nng_aio_result(aio)) != 0) {
		return FATAL(nng_strerror(rv), rv);
	}

	DEBUG("STEP7 <-- Response into aio.");
	nng_http_conn_read_res(conn, res, aio);
	nng_aio_wait(aio);

	DEBUG("STEP6 ... aio is ready");
	if ((rv = nng_aio_result(aio)) != 0) {
		return FATAL(nng_strerror(rv), rv);
	}

	DEBUG("STEP7 checking HTTP CODE");
	if (nng_http_res_get_status(res) != NNG_HTTP_STATUS_OK) {
		fprintf(stderr, "HTTP Server Responded: %d %s\n",
			nng_http_res_get_status(res),
			nng_http_res_get_reason(res));
	}

	// This only supports regular transfer encoding (no Chunked-Encoding,
	// and a Content-Length header is required.)
	DEBUG("STEP8 checking Content-Length from response header");
	if ((hdr = nng_http_res_get_header(res, "Content-Length")) == NULL) {
		DEBUG("\tMissing Content-Length header. Use first 10 bytes to get chunk length");
		len = 10;
		chunk_state = 0;
	} else {
		len = atoi(hdr);
		if (len == 0) {
			DEBUG("STEP8 Content-Length zero, return");
			return (0);
		}
	}

	printf("Allocate %d buffer to receive the body data.\n", len);
	data = malloc(len);
	// Set up a single iov to point to the buffer.
	iov.iov_len = len;
	iov.iov_buf = data;

	DEBUG("STEP9 nng_aio_set_iov(aio, 1, &iov)");
	nng_aio_set_iov(aio, 1, &iov);

	// Now attempt to receive the data.
	DEBUG("STEP10 nng_http_conn_read_all(conn, aio);");
	nng_http_conn_read_all(conn, aio);

	int data_scan_offset = 0; // data scan pos
	char prev_c = 0, c = 0; // last scanned char
	char *cur_buffer; // init current chunk content, content copy to here.
	int buf_w_offset = 0;

	int chunk_parsing_char_ct = 0;
	int chunk_len = 0; // current chunk length
	char* chunk_array[1024]; // total chunks
	int chunk_idx = 0; // current chunk index
	long unsigned chunk_len_sum = 0;

	do {
		DEBUG("STEP11 Wait for it to complete.");
		nng_aio_wait(aio);

		if ((rv = nng_aio_result(aio)) != 0)
			return FATAL(nng_strerror(rv), rv);

		if (chunk_state < 0) { // Not chunk mode.
			fwrite(data, 1, len, stdout);
			break;
		}

		DEBUG("STEP12 chunk mode parsing");
		for (data_scan_offset = 0;data_scan_offset < len; data_scan_offset++) {
			prev_c = c;
			c = *(char*)(data+data_scan_offset);
			if (chunk_state == 0) { // chunk len scannig.
				chunk_parsing_char_ct += 1;
				if (c == 13) {
					printf("!!! LEN %d << len %d %d got \\r\n", chunk_len, len, data_scan_offset);
					continue; // got \r, next to scan \n
				} else if (prev_c == 13 && c == 10) {
					// \r\n got, chunk_len calculated
					printf("!!! LEN %d << len %d %d got \\n\n", chunk_len, len, data_scan_offset);
					if (chunk_len == 0) {
						chunk_state = 2; // -> end
						DEBUG("ZERO length chunk found, finished");
						printf("chunk total %d\n", chunk_idx);
						// print every chunk.
						for (int idx = 0; idx < chunk_idx; idx++) {
							printf("chunk %d/%d: len %lu\n%s\n", idx, chunk_idx-1, strlen(chunk_array[idx]), chunk_array[idx]);
						}
						return (0);
					} else {
						cur_buffer = malloc(chunk_len+1);
						buf_w_offset = 0;
						chunk_state = 1; // -> content scanning
					}
				} else if ((c <= 'f' && c >= 'a') || (c <= '9' && c >= '0') || (c <= 'F' && c >= 'A')) {
					// calculate more length HEX chars and scan for \r\n
					int hex_digit;
					if (c <= 'f' && c >= 'a') hex_digit = (int)c - (int)('a') + 10;
					if (c <= 'F' && c >= 'A') hex_digit = (int)c - (int)('A') + 10;
					if (c <= '9' && c >= '0') hex_digit = (int)c - (int)('0');
					chunk_len = (chunk_len << 4) + hex_digit;
					printf("??? LEN %d << len %d [%d %c %d]\n", chunk_len, len, data_scan_offset, c, hex_digit);
				} else {
					printf("XXX LEN %d << len %d [%d %c(%d)]\n", chunk_len, len, data_scan_offset, c, (int)c);
					return FATAL("Unexpected char", 1);
				}
			} else if (chunk_state == 1) { // content scanning and copying.
				if (buf_w_offset < chunk_len) {
					int data_left = len - data_scan_offset;
					int bufr_left = chunk_len - buf_w_offset;
					int copy_ct = (data_left < bufr_left) ? data_left : bufr_left;
					printf(">>> copy buffer[%d + %d] << data[%d + %d] \n", buf_w_offset, copy_ct, data_scan_offset, copy_ct);
					memcpy(cur_buffer+buf_w_offset, data+data_scan_offset, copy_ct);
					buf_w_offset += copy_ct;
					data_scan_offset += copy_ct;
				} else {
					*(cur_buffer+buf_w_offset) = 0; // add \0 to cur_buffer
					chunk_len_sum += buf_w_offset;
					printf(">>> copy buffer[%d] finished, idx %d, total len: %lu\n", buf_w_offset, chunk_idx, chunk_len_sum);
					if (buf_w_offset < 300)
						printf("[%s]\n", cur_buffer);
					else {
						char head[100];
						memcpy(head, cur_buffer, 99);
						head[99] = EOF;
						printf("[%s...\n%s]\n", head, cur_buffer+buf_w_offset-300);
					}
					chunk_array[chunk_idx++] = cur_buffer;
					cur_buffer = 0;
					// finish this chunk copying, ready to start next chunk parsing.
					prev_c = 0;
					chunk_state = 0;
					chunk_len = 0;
					chunk_parsing_char_ct = 0;
				}
			} else
				return (0);
		}

		free(data);
		if (chunk_state == 0) {
			if (chunk_parsing_char_ct < 4)
				len = 5-chunk_parsing_char_ct; // min 5: 0\r\n\r\n
			else
				len = 1;
			printf("Missed char at parsing chunk len, receive %d more byte\n", len);
		} else if (chunk_state == 1) {
			int missed_ct = chunk_len - buf_w_offset;
			missed_ct += 2; // \r\n
			missed_ct += 5; // ABCD\r\n 6, 0\r\n\r\n 5
			len = missed_ct;
			printf("Missed chunk char ct %d, allocate %d buffer to receive remained data + next len.\n", (chunk_len-buf_w_offset), len);
		} else {
			return FATAL("????", 1);
		}
		data = malloc(len);
		// Set up a single iov to point to the buffer.
		iov.iov_len = len;
		iov.iov_buf = data;

		DEBUG("STEP9 nng_aio_set_iov(aio, 1, &iov)");
		nng_aio_set_iov(aio, 1, &iov);

		// Now attempt to receive the data.
		DEBUG("STEP10 nng_http_conn_read_all(conn, aio);");
		nng_http_conn_read_all(conn, aio);
	} while(1);
	return (0);
}
