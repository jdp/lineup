#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <getopt.h>
#include "message.h"
#include "server.h"

#define RUNNING_DIR "/tmp"
#define PIDFILE "lineupd.pid"

/* 
 * Thanks to the Unix Daemon Server programming tutorial for the
 * daemon-related code. Read it here:
 *   http://www.enderunix.org/docs/eng/daemon.php
 */
 
/*
 * Handles signals. Hangup should refresh the server, terminate
 * should terminate it.
 */
void
signal_handler(int sig)
{
	/*
	switch(sig) {
	case SIGHUP:
		log_message(LOG_FILE,"hangup signal catched");
		break;
	case SIGTERM:
		log_message(LOG_FILE,"terminate signal catched");
		exit(0);
		break;
	}
	*/
}

/*
 * Daemonizes the server
 */
void
daemonize(char *pidfile)
{
	int i, fp;
	char str[16];
	if(getppid() == 1) {
		return; 
	}
	i = fork();
	if (i < 0) {
		exit(1);
	}
	if (i > 0) {
		exit(0);
	}
	setsid();
	for (i = sysconf(_SC_OPEN_MAX); i >= 0; --i) {
		close(i);
	}
	i = open("/dev/null", O_RDWR); dup(i); dup(i);
	umask(027);
	chdir(RUNNING_DIR);
	fp = open(pidfile, O_RDWR|O_CREAT, 0640);
	sprintf(str, "%d\n", getpid());
	write(fp, str, strlen(str));
	close(fp);
	signal(SIGCHLD, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGHUP, signal_handler);
	signal(SIGTERM, signal_handler);
}


int
main(int argc, char **argv)
{
	int c, port = 9876, daemonized = 0;
	
	while ((c = getopt(argc, argv, "dp:")) != -1) {
		switch (c) {
			case 'p':
				/* Manually set port */
				port = atoi(optarg);
				break;
			case 'd':
				/* Daemonize server */
				daemonize(PIDFILE);
				daemonized = 1;
				break;
			case '?':
				if (optopt == 'p') {
					fprintf(stderr, "Option -%c requires an argument.\n", optopt);
				}
				else if (isprint(optopt)) {
					fprintf(stderr, "Unknown option `-%c'.\n", optopt);
				}
				else {
					fprintf(stderr, "Uknown option `\\x%x'.\n", optopt);
				}
				return 1;
			default:
				abort();
		}
	}

	if (port<=0 || port>65535) {
		fprintf(stderr, "invalid port: %d", port);
		return 1;
	}
	
	if (!Server_start(port)) {
		fprintf(stderr, "couldn't start server on port %d\n", port);
		return 1;
	}
	
	return 0;
}
