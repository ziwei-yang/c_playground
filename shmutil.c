#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <time.h>

#include "urn.h"

int main(int argc, char **argv) {
	if (argc <= 1)
		return URN_FATAL("args: shmkey del/resize [subargs]", EINVAL);
	if (strcasecmp(argv[1], "-v") == 0) {
		URN_LOG("Compiled at " __TIMESTAMP__);
		return 0;
	} else if (strcasecmp(argv[1], "-h") == 0) {
		URN_LOG("args: shmkey stat/del/resize [subargs]");
		return 0;
	}

	int rv = 0;
	struct shmid_ds ds;
	int shmkey = strtol(argv[1], NULL, 16);

	int shmid = shmget(shmkey, 0, 0);
	URN_LOGF("shmkey 0x%08x shmid got: %d", shmkey, shmid);
	if (shmid < 0) {
		perror("shmget error");
		return errno;
	}

	if (argc == 2) {
		URN_LOG("args: shmkey stat/del/resize [subargs]");
		return 0;
	} else if (strcasecmp(argv[2], "stat") == 0) {
		rv = shmctl(shmid, IPC_STAT, &ds);
		if (rv != 0)
			perror("shmctl IPC_STAT error");
		URN_INFOF("\tIPC_STAT rv: %d", rv);
		URN_INFOF("\tshm_segsz:   %zu", ds.shm_segsz);
		URN_INFOF("\tlast op pid: %d", ds.shm_lpid);
		URN_INFOF("\tcreator pid: %d", ds.shm_cpid);
		URN_INFOF("\tnum attach:  %lu", (unsigned long)(ds.shm_nattch));
	} else if (strcasecmp(argv[2], "del") == 0) {
		rv = shmctl(shmid, IPC_RMID, NULL);
		if (errno != 0)
			perror("shmctl IPC_RMID error");
		URN_INFOF("\tIPC_RMID rv=%d", rv);
	}

	return 0;
}
