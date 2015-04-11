#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include "trade_data.h"
#include "trade_log.h"

#define LOG_HEADER "Buyer\tSeller\tSymbol\tQuantity\tPrice"

static void
print_logs(const char *path)
{
	struct cix_trade_log_iterator iter;
	struct cix_execution exec;

	puts(LOG_HEADER);

	if (cix_trade_log_iterator_init(&iter, path) == false) {
		fprintf(stderr, "failed to initialize iterator\n");
		exit(EXIT_FAILURE);
	}

	while (cix_trade_log_iterator_next(&iter, &exec) == true) {
		printf("%" CIX_PR_ID "\t%" CIX_PR_ID "\t%s\t%" CIX_PR_Q "\t%"
		    CIX_PR_P "\n", exec.buyer, exec.seller, exec.symbol.symbol,
		    exec.quantity, exec.price);
	}

	cix_trade_log_iterator_destroy(&iter);
	return;
}

static void
usage(void)
{

	/* XXX */
	exit(EXIT_SUCCESS);
}

int
main(int argc, char **argv)
{

	if (argc < 2) {
		usage();
	}

	print_logs(argv[1]);
	return 0;
}
