#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "client.h"
#include "messages.h"
#include "vector.h"

#define STRESS_FATAL(X, ...) 			\
do {						\
	fprintf(stderr, X, __VA_ARGS__);	\
	exit(EXIT_FAILURE);			\
} while(0);

struct stress_thread {
	struct cix_client client;
	struct cix_message_order *orders;
	pthread_t thread;
};

static struct {
	const char *address;
	uint16_t port;

	unsigned int n_order;
	unsigned int n_thread;

	/* Delay between messages on a single thread, in us */
	unsigned int delay;

	unsigned int min_price;
	unsigned int max_price;
	unsigned int min_quantity;
	unsigned int max_quantity;
	struct cix_vector *symbols;
} stress_config;

static struct stress_thread *stress_threads;
static struct random_data stress_random;
static char stress_random_buf[256];

static void
stress_random_init(void)
{
	unsigned int seed = time(NULL);

	if (initstate_r(seed, stress_random_buf, sizeof stress_random_buf,
	    &stress_random) == -1) {
		fprintf(stderr, "failed to initialize random buffer: %s\n",
		    strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (srandom_r(seed, &stress_random) == 0) {
		return;
	}

	perror("srandom");
	exit(EXIT_FAILURE);
}

static void
stress_random_next(int32_t *result)
{

	if (random_r(&stress_random, result) == 0) {
		return;
	}

	perror("random");
	exit(EXIT_FAILURE);
}

static unsigned int
stress_random_range(unsigned int min_value, unsigned int max_value)
{
	int32_t r;
	unsigned int u;
	unsigned int range = (max_value - min_value) + 1;

	assert(max_value >= min_value);

	if (max_value == min_value) {
		return min_value;
	}

	stress_random_next(&r);
	u = (unsigned int)r;
	return min_value + (u % range);
}

static void
stress_thread_init_orders(struct stress_thread *thread)
{
	unsigned int i;

	for (i = 0; i < stress_config.n_order; ++i) {
		struct cix_message_order *order = &thread->orders[i];
		unsigned int u;

		order->price = stress_random_range(stress_config.min_price,
		    stress_config.max_price);
		order->quantity = stress_random_range(
		    stress_config.min_quantity, stress_config.max_quantity);
		order->side = stress_random_range(CIX_TRADE_SIDE_BUY,
		    CIX_TRADE_SIDE_SELL);
		u = stress_random_range(0, cix_vector_length(
		    stress_config.symbols) - 1);
		strcpy(order->symbol.symbol,
		    cix_vector_item(stress_config.symbols, u));
		/* XXX: external order id not used yet */
	}

	return;
}

static void
stress_thread_init(struct stress_thread *thread)
{
	struct cix_client_callbacks callbacks = { NULL, NULL };

	if (cix_client_init(&thread->client, stress_config.address,
	    stress_config.port, &callbacks, NULL) == false) {
		fprintf(stderr, "failed to connect\n");
		exit(EXIT_FAILURE);
	}

	thread->orders = malloc(stress_config.n_order *
	    sizeof(*thread->orders));
	if (thread->orders == NULL) {
		fprintf(stderr, "failed to create order pool\n");
		exit(EXIT_FAILURE);
	}

	stress_thread_init_orders(thread);
	return;
}

static void *
stress_thread_run(void *p)
{
	struct stress_thread *thread = p;
	unsigned int i = 0;

	for (i = 0; ; ++i) {
		struct timespec wait, remaining;

		if (i >= stress_config.n_order) {
			i -= stress_config.n_order;
		}

		if (cix_client_send_order(&thread->client,
		    &thread->orders[i]) == false) {
			fprintf(stderr, "failed to send order\n");
			/* XXX: Handle this more gracefully */
			exit(EXIT_FAILURE);
		}

		if (stress_config.delay == 0) {
			continue;
		}

		/* XXX: Confirm that delay is less than 1 second */
		wait.tv_sec = 0;
		wait.tv_nsec = 1000 * stress_config.delay;
		
		while (nanosleep(&wait, &remaining) == -1) {
			if (errno != EINTR) {
				fprintf(stderr, "error sleeping\n");
				exit(EXIT_FAILURE);
			}

			memcpy(&wait, &remaining, sizeof wait);
		}
	}

	return NULL;
}

static void
stress_config_init(void)
{

	memset(&stress_config, 0, sizeof stress_config);
	stress_config.n_thread = 2;
	stress_config.n_order = 1000;
	stress_config.min_price = 490;
	stress_config.max_price = 510;
	stress_config.min_quantity = 10;
	stress_config.max_quantity = 1000;

	if (cix_vector_init(&stress_config.symbols, sizeof(cix_symbol_t), 32) ==
	    false) {
		fprintf(stderr, "failed to create symbol list\n");
		exit(EXIT_FAILURE);
	}

	return;
}

static unsigned long
parse_positive(const char *s, const char *name)
{
	char *end;
	unsigned long result;

	errno = 0;

	result = strtoul(s, &end, 10);
	if (s[0] == '-') {
		fprintf(stderr, "value of %s must be positive\n", name);
		exit(EXIT_FAILURE);
	}

	if (*end != '\0') {
		fprintf(stderr, "invalid number %s\n", s);
		exit(EXIT_FAILURE);
	}

	if (result == ULONG_MAX && errno == ERANGE) {
		fprintf(stderr, "value %s exceeds maximum limit\n", s);
		exit(EXIT_FAILURE);
	}

	return result;
}

static void
usage(void)
{

	/* XXX */
	fprintf(stderr, "usage\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	struct option options[] = {
		{ "address", required_argument, NULL, 'a' },
		{ "delay", required_argument, NULL, 'd' },
		{ "port", required_argument, NULL, 'p' },
		{ "threads", required_argument, NULL, 't' },
		{ NULL, 0, NULL, 0 }
	};
	int a;
	unsigned int i;

	stress_random_init();
	stress_config_init();

	while ((a = getopt_long(argc, argv, "a:d:p:t:", options, NULL)) != -1) {
		switch(a) {
		case 'a':
			stress_config.address = optarg;
			break;
		case 'd':
			stress_config.delay = parse_positive(optarg, "delay");
			break;
		case 'p': {
			unsigned long l = parse_positive(optarg, "port");

			if (l > UINT16_MAX) {
				fprintf(stderr, "maximum port value is %"
				    PRIu16 "\n", UINT16_MAX);
				exit(EXIT_FAILURE);
			}

			stress_config.port = (uint16_t)l;
			break;
		}
		case 't':
			stress_config.n_thread = atoi(optarg);
			if (stress_config.n_thread <= 0) {
				usage();
			}
			break;
		default:
			usage();
			break;
		}
	}

	if (stress_config.address == NULL) {
		fprintf(stderr, "server address is required\n");
		exit(EXIT_FAILURE);
	}

	if (stress_config.port == 0) {
		fprintf(stderr, "server port is required\n");
		exit(EXIT_FAILURE);
	}

	if (cix_vector_length(stress_config.symbols) == 0) {
		cix_symbol_t *symbol = cix_vector_next(&stress_config.symbols);

		if (symbol == NULL) {
			fprintf(stderr, "failed to initialize symbol list\n");
			exit(EXIT_FAILURE);
		}

		strcpy(symbol->symbol, "GOOG");
	}

	stress_threads = malloc(stress_config.n_thread *
	    sizeof(*stress_threads));
	if (stress_threads == NULL) {
		fprintf(stderr, "failed to create threads\n");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < stress_config.n_thread; ++i) {
		stress_thread_init(&stress_threads[i]);
		if (pthread_create(&stress_threads[i].thread, NULL,
		    stress_thread_run, &stress_threads[i]) != 0) {
			fprintf(stderr, "failed to start threads\n");
			exit(EXIT_FAILURE);
		}
	}

	for (i = 0; i < stress_config.n_thread; ++i) {
		if (pthread_join(stress_threads[i].thread, NULL) != 0) {
			fprintf(stderr, "failed to join thread\n");
			exit(EXIT_FAILURE);
		}
	}

	return 0;
}
