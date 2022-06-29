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

//#define URN_MAIN_DEBUG // debug log
#include "wss.h"

int main(int argc, char **argv) {
	char *uri = argv[1];
	uint16_t http_code = 0;
	char *http_res = NULL;
	size_t res_len = 0;
	nng_https_get(uri, &http_code, &http_res, &res_len);
	URN_INFOF("%s\nlen %zu code %hu", http_res, res_len, http_code);
}
