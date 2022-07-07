#include <stdio.h>
#include <unistd.h>

#include "urn.h"
#include "order.h"
#include "wss.h"
#include "hmap.h"
#include "redis.h"

int pairid = -1;
urn_odbk_mem *shmptr = NULL;
urn_odbk_clients *odbk_clients_shmptr = NULL;

void sig_handler(int signum) {
	urn_odbk_shm_print(shmptr, pairid);
	urn_odbk_clients_print(odbk_clients_shmptr, pairid);
}

int main(int argc, char **argv) {
	if (argc < 3)
		URN_FATAL("Usage: exchange pair1 pair2 ...", EINVAL);
	char *exchange = argv[1];
	int rv = 0;

	rv = urn_odbk_shm_init(false, exchange, &shmptr);
	if (rv != 0)
		URN_FATAL("Error in odbk_shm_init", rv);
	rv = urn_odbk_clients_init(exchange, &odbk_clients_shmptr);
	if (rv != 0)
		return URN_FATAL("Error in odbk_shm_write_index()", rv);

	for (int i=0; i<urn_odbk_mem_cap; i++) {
		if (shmptr->pairs[i][0] == '\0') {
			URN_INFOF("%d pair end", i);
			break;
		}
		URN_INFOF("%d pair %s", i, shmptr->pairs[i]);
		if (strcasecmp(argv[2], shmptr->pairs[i]) == 0)
			pairid = i;
	}
	if (pairid <= 0)
		URN_FATAL("Pair not found", ERANGE);

	signal(SIGUSR1, sig_handler);
	urn_odbk_clients_reg(odbk_clients_shmptr, pairid);
	while(true) {
		sleep(99999);
	}
}
