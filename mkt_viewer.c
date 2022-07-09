#include <stdio.h>
#include <unistd.h>

#include "urn.h"
#include "order.h"
#include "wss.h"
#include "hmap.h"
#include "redis.h"

int pairid = -1;
char target_pair[32];
urn_odbk_mem *shmptr = NULL;
urn_odbk_clients *odbk_clients_shmptr = NULL;

/* verify pair info, then print data */
void work() {
	if (strcmp(shmptr->pairs[pairid], target_pair) != 0) {
		URN_WARNF("Pair changed to %s, locate %s again",
			shmptr->pairs[pairid], target_pair);
		urn_odbk_clients_dereg(odbk_clients_shmptr, pairid);
		urn_odbk_shm_print_index(shmptr);
		pairid = urn_odbk_shm_pair_index(shmptr, target_pair);
		if (pairid <= 0)
			URN_FATAL("Pair not found", ERANGE);
		urn_odbk_clients_reg(odbk_clients_shmptr, pairid);
	}
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

	strcpy(target_pair, argv[2]);
	urn_s_upcase(target_pair, strlen(target_pair));
	urn_odbk_shm_print_index(shmptr);
	pairid = urn_odbk_shm_pair_index(shmptr, target_pair);
	if (pairid <= 0)
		URN_FATAL("Pair not found", ERANGE);
	urn_odbk_clients_reg(odbk_clients_shmptr, pairid);

	/* create signal set contains SIGUSR1, KILL and INT */
	sigset_t sigusr1_set;
	sigemptyset(&sigusr1_set);
	sigaddset(&sigusr1_set, SIGUSR1);
	sigaddset(&sigusr1_set, SIGINT);
	sigaddset(&sigusr1_set, SIGKILL);
	signal(SIGUSR1, work);
	int pid = getpid();

	/* signal incoming or timeout */
	struct timespec timeout;
	timeout.tv_sec = 2;
	timeout.tv_nsec = 0;
	int sig = 0;
#if __APPLE__
	sigaddset(&sigusr1_set, SIGALRM);
	signal(SIGALRM, work);
	// macos has no sigtimedwait();
	// setup a alarm to send self a SIGUSR1
	unsigned secs = (unsigned)(timeout.tv_sec);
	if (secs <= 0) secs = 1;

	while (true) {
		alarm(secs); // reset timer
		sigwait(&sigusr1_set, &sig);
		// timeout waiting signal : SIGALRM
		if (sig == SIGALRM || sig == SIGUSR1) {
			work();
			continue;
		}
		printf("SIG %d, send to self again.\n", sig);
		kill(pid, sig);
	}
#else
	while (true) {
		sig = sigtimedwait(&sigusr1_set, NULL, &timeout);
		// timeout waiting signal : EAGAIN
		if (sig == EAGAIN || sig == SIGUSR1) {
			work();
			continue;
		}
		printf("SIG %d, send to self again.\n", sig);
		kill(pid, sig);
	}
#endif
}
