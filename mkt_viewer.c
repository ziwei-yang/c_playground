#include <stdio.h>
#include <unistd.h>

#include "urn.h"

char exchange[32];
char target_pair[32];
int  pairid = -1;
urn_odbk_mem *shmptr = NULL;
urn_odbk_clients *odbk_clients_shmptr = NULL;

/* verify pair info, then print data */
int init() { return 0; }
void work() {
	urn_odbk_shm_print(shmptr, pairid);
	urn_odbk_clients_print(odbk_clients_shmptr, pairid);
}
void noop() {}
void grace_exit() { exit(0); }

int main(int argc, char **argv) {
	if (argc < 3)
		URN_FATAL("Usage: exchange pair", EINVAL);
	strcpy(exchange, argv[1]);
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

	rv = init();
	if (rv != 0)
		return URN_FATAL("Error in init()", rv);
	signal(SIGINT, grace_exit);
	signal(SIGQUIT, grace_exit);

	/* create signal set contains SIGUSR1, KILL and INT */
	sigset_t sigusr1_set;
	sigemptyset(&sigusr1_set);
	sigaddset(&sigusr1_set, SIGUSR1);
	sigaddset(&sigusr1_set, SIGINT);
	sigaddset(&sigusr1_set, SIGKILL);

	/* do nothing on SIGUSR1, just sigwait then work() */
	signal(SIGUSR1, noop);
	int pid = getpid();

	/* signal incoming or timeout */
	struct timespec timeout;
	timeout.tv_sec = 2;
	timeout.tv_nsec = 0;
	int sig = 0;
#if __APPLE__
	sigaddset(&sigusr1_set, SIGALRM);
	signal(SIGALRM, noop);
	// macos has no sigtimedwait();
	// setup a alarm to send self a SIGUSR1
	unsigned secs = (unsigned)(timeout.tv_sec);
	if (secs <= 0) secs = 1;
#endif
	while (true) {
#if __APPLE__
		alarm(secs); // reset timer
		sigwait(&sigusr1_set, &sig);
		// timeout waiting signal : SIGALRM
		if (sig != SIGALRM && sig != SIGUSR1) {
#else
		sig = sigtimedwait(&sigusr1_set, NULL, &timeout);
		// timeout waiting signal : EAGAIN
		if (sig != EAGAIN && sig != SIGUSR1) {
			if (sig == -1) continue; // timeout
#endif
			URN_WARNF("SIG %d, send to self again.\n", sig);
			kill(pid, sig);
			continue;
		}
		// Assure pairid is target_pair
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
		work();
	}
}
