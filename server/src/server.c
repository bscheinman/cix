#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "market.h"
#include "session.h"
#include "trade_log.h"
#include "vector.h"

/* XXX: Configurable */
#define CIX_MARKET_THREAD_COUNT 1
#define CIX_SESSION_THREAD_COUNT 1

/* XXX: read these from config file */
static char *cix_symbols[2] = { "GOOG", "AAPL" };
static struct cix_vector *cix_symbol_vector;
static struct cix_market *cix_market;

static void
create_symbol_vector(void)
{
	unsigned int i;
	size_t vector_size = sizeof(cix_symbols) / sizeof(*cix_symbols);

	if (cix_vector_init(&cix_symbol_vector, sizeof(cix_symbol_t),
	    vector_size) == false) {
		fprintf(stderr, "failed to create symbol list");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < vector_size; ++i) {
		cix_symbol_t *symbol = cix_vector_next(&cix_symbol_vector);

		if (symbol == NULL) {
			fprintf(stderr, "failed to register symbol\n");
			exit(EXIT_FAILURE);
		}

		strncpy(symbol->symbol, cix_symbols[i], sizeof symbol->symbol);
	}

	return;
}

int
main(int argc, char **argv)
{

	(void)argc;
	(void)argv;

	cix_trade_log_init();

	create_symbol_vector();

	cix_market = cix_market_init(cix_symbol_vector,
	    CIX_MARKET_THREAD_COUNT);

	if (cix_market == NULL || cix_market_run(cix_market) == false) {
		fprintf(stderr, "failed to initialize market\n");
		exit(EXIT_FAILURE);
	}

	cix_session_listen(cix_market, CIX_SESSION_THREAD_COUNT);

	for (;;) pause();
	return 0;
}
