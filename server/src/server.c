#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "market.h"
#include "session.h"
#include "vector.h"

/* XXX: read these from config file */
static char *cix_symbols[2] = { "GOOG", "AAPL" };
static struct cix_vector *cix_symbol_vector;

static void
create_symbol_vector(void)
{
	unsigned int i;
	size_t vector_size = sizeof(cix_symbols) / sizeof(*cix_symbols);

	if (cix_vector_init(&cix_symbol_vector, vector_size) == false)
		exit(EXIT_FAILURE);

	for (i = 0; i < vector_size; ++i) {
		cix_vector_element_t element;

		memcpy(element.symbol, cix_symbols[i], sizeof element.symbol);
		if (cix_vector_append(&cix_symbol_vector, &element) ==
		    false) {
			exit(EXIT_FAILURE);
		}
	}

	return;
}

int
main(int argc, char **argv)
{

	(void)argc;
	(void)argv;

	create_symbol_vector();
	cix_market_init(cix_symbol_vector);
	cix_session_listen(1);

	for (;;) pause();
	return 0;
}
