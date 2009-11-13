#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <getopt.h>
#include "server.h"

int
main(int argc, char **argv)
{
	int c;
	int port = 9876;
	
	while ((c = getopt(argc, argv, "p:")) != -1) {
		switch (c) {
			case 'p':
				port = atoi(optarg);
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
