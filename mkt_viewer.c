#include <stdio.h>
#include <unistd.h>

#include "urn.h"
#include "order.h"
#include "wss.h"
#include "hmap.h"
#include "redis.h"

int main(int argc, char **argv) {
	if (argc < 3)
		URN_FATAL("Usage: exchange pair1 pair2 ...", EINVAL);
	char *exchange = argv[1];
	int rv = 0;

	key_t shmkey = -1;
	int shmid = -1;
	urn_odbk_mem *shmptr = NULL;
	rv = urn_odbk_shm_init(false, exchange, &shmkey, &shmid, &shmptr);
	if (rv != 0)
		URN_FATAL("Error in odbk_shm_init", rv);

	int pairid = -1;
	for (int i=0; i<urn_odbk_mem_cap; i++) {
		if (shmptr->pairs[i][0] == '\0')
			break;
		URN_INFOF("%d pair %s", i, shmptr->pairs[i]);
		if (strcasecmp(argv[2], shmptr->pairs[i]) == 0)
			pairid = i;
	}
	if (pairid <= 1)
		URN_FATAL("Pair not found", ERANGE);

	while(true) {
		urn_odbk_shm_print(shmptr, pairid);
		usleep(50*1000);
	}
}
