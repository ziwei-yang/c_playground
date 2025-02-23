#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "urn.h"

char exchange[32];
char target_pair[32];
int  pairid = -1;
urn_odbk_mem *shmptr = NULL;
urn_odbk_clients *odbk_clients_shmptr = NULL;

/* Write data into ./data/$exchange.$symbol/$unixtimestamp.txt */
char dpath[1024];
char fpath[1024];
long filename_ts;
int  fd = -1;
char line[1024];
struct timespec ts;
int mkdir_p(const char *dir) {
    char temp[1024];
    char *p = NULL;
    size_t len;
    // Copy path to temporary buffer
    snprintf(temp, sizeof(temp), "%s", dir);
    len = strlen(temp);
    // Remove trailing slashes
    while (len > 1 && temp[len - 1] == '/') { temp[--len] = '\0'; }
    // Create directories one by one
    for (p = temp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';  // Temporarily end string
            if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
                perror("mkdir");
                return -1;
            }
            *p = '/';  // Restore slash
        }
    }
    // Create final directory
    if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
        perror("mkdir");
        return -1;
    }
    return 0;
}
int init() {
	if (fd != -1) {
		URN_LOGF("Close file %s", fpath);
		close(fd);
	}
	sprintf(dpath, "./data/%s.%s", exchange, target_pair);
	clock_gettime(CLOCK_REALTIME, &ts);
	filename_ts = (long long)ts.tv_sec/86400*86400;
	sprintf(fpath, "./%s/%ld.txt", dpath, filename_ts);
	URN_LOGF("Init file %s", fpath);
	mkdir_p(dpath);
	fd = open(fpath, O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (fd < 0) {
		perror("Failed to open file");
		return fd;
	}
	return 0;
}
void work() {
	clock_gettime(CLOCK_REALTIME, &ts);
	if ((long long)ts.tv_sec/86400*86400 != filename_ts) {
		URN_LOGF("Time to start a new file");
		init();
	}
	// Write to file
	int rv = urn_odbk_shm_print_inline(shmptr, pairid, line);
	if (rv == 0) {
		write(fd, line, strlen(line));
	} else {
		URN_WARN("Error in preparing line");
	}
}
void grace_exit() {
	if (fd != -1) {
		if (fdatasync(fd) == -1) {
			perror("fdatasync failed");
		} else {
			printf("Data successfully synced to disk.\n");
		}
		close(fd);
	}
	exit(0);  // Graceful exit
}	

void noop() {};

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
		if (sig == EAGAIN || sig == SIGUSR1) {
#endif
			printf("SIG %d, send to self again.\n", sig);
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
