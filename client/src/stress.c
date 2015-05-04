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
	struct cix_event_manager event_manager;
	struct cix_event timer_event;

	struct cix_client client;
	struct cix_message_order *orders;
	pthread_t thread;
	unsigned long messages_sent;
};

static struct {
	const char *address;
	uint16_t port;

	unsigned int n_order;
	unsigned int n_thread;
	
	unsigned long order_limit;

	/* Delay between messages on a single thread, in us */
	unsigned int delay;
	unsigned int batch_size;

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
		order->side = (i & 1) ? CIX_TRADE_SIDE_BUY :
		    CIX_TRADE_SIDE_SELL;
		u = stress_random_range(0, cix_vector_length(
		    stress_config.symbols) - 1);
		strcpy(order->symbol.symbol,
		    cix_vector_item(stress_config.symbols, u));
		/* XXX: external order id not used yet */
	}

	return;
}

static void
stress_execution_handler(struct cix_client_execution *exec, void *closure)
{
	
	(void)closure;

	printf("received execution for order %" CIX_PR_ID " (%" CIX_PR_Q
	    " shares @ %" CIX_PR_P ")\n", exec->order_id, exec->quantity,
	    exec->price);
	return;
}

static void
stress_ack_handler(struct cix_client_ack *ack, void *closure)
{

	(void)closure;

	printf("order %s ack'd (server ID %" CIX_PR_ID ")\n",
	    ack->client_id, ack->server_id);
	return;
}

static void
stress_thread_send_orders(struct cix_event *event, cix_event_flags_t flags,
    void *closure)
{
	struct stress_thread *thread = closure;

	if (stress_config.batch_size <= 1) {
		if (cix_client_send_order(&thread->client,
		    &thread->orders[thread->messages_sent & 1]) == false) {
			fprintf(stderr, "failed to send order\n");
			/* XXX: Handle this more gracefully */
			exit(EXIT_FAILURE);
		}
		++thread->messages_sent;
	} else {
		unsigned int i;

		cix_client_batch_start(&thread->client);

		for (i = 0; i < stress_config.batch_size; ++i) {
			if (cix_client_send_order(&thread->client,
			    &thread->orders[thread->messages_sent & 1]) ==
			    false) {
				fprintf(stderr, "failed to send order\n");
				/* XXX: Handle this more gracefully */
				exit(EXIT_FAILURE);
			}
			++thread->messages_sent;
		}

		if (cix_client_batch_end(&thread->client) == false) {
			fprintf(stderr, "failed to send order\n");
			/* XXX: Handle this more gracefully */
			exit(EXIT_FAILURE);
		}
	}

	if (stress_config.order_limit > 0 && thread->messages_sent >=
	    stress_config.order_limit) {
		pthread_exit(NULL);
	}

	return;
}

static void *
stress_thread_run(void *closure)
{
	struct stress_thread *thread = closure;
	struct cix_client_callbacks callbacks = {
		.ack = stress_ack_handler,
		.exec = stress_execution_handler
	};

	thread->orders = malloc(stress_config.n_order *
	    sizeof(*thread->orders));
	if (thread->orders == NULL) {
		fprintf(stderr, "failed to create order pool\n");
		exit(EXIT_FAILURE);
	}
	stress_thread_init_orders(thread);

	if (cix_event_manager_init(&thread->event_manager) == false) {
		fprintf(stderr, "failed to initialize event loop\n");
		exit(EXIT_FAILURE);
	}

	if (cix_event_init_timer(&thread->timer_event,
	    stress_thread_send_orders, thread) == false ||
	    cix_event_timer_set(&thread->timer_event,
	    stress_config.delay * 1000) == false ||
	    cix_event_add(&thread->event_manager, &thread->timer_event) ==
	    false) {
		fprintf(stderr, "failed to create timer event\n");
		exit(EXIT_FAILURE);
	}

	if (cix_client_init(&thread->client, stress_config.address,
	    stress_config.port, &callbacks, NULL) == false) {
		fprintf(stderr, "failed to connect\n");
		exit(EXIT_FAILURE);
	}

	if (cix_event_add(&thread->event_manager,
	    cix_client_event(&thread->client)) == false) {
		fprintf(stderr, "failed to setup client event\n");
		exit(EXIT_FAILURE);
	}

	if (cix_event_manager_run(&thread->event_manager) == false) {
		fprintf(stderr, "event loop failed\n");
		exit(EXIT_FAILURE);
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
	stress_config.min_quantity = 50;
	stress_config.max_quantity = 500;

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
		{ "batch-size", required_argument, NULL, 'b' },
		{ "delay", required_argument, NULL, 'd' },
		{ "orders", required_argument, NULL, 'n' },
		{ "port", required_argument, NULL, 'p' },
		{ "threads", required_argument, NULL, 't' },
		{ NULL, 0, NULL, 0 }
	};
	int a;
	unsigned int i;
	unsigned long total_messages;

	stress_random_init();
	stress_config_init();

	while ((a = getopt_long(argc, argv, "a:b:d:n:p:t:", options, NULL)) !=
	    -1) {
		switch(a) {
		case 'a':
			stress_config.address = optarg;
			break;
		case 'b':
			stress_config.batch_size = parse_positive(optarg,
			    "batch size");
			break;
		case 'd':
			stress_config.delay = parse_positive(optarg, "delay");
			break;
		case 'n':
			stress_config.order_limit = parse_positive(optarg,
			    "order limit");
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
		if (pthread_create(&stress_threads[i].thread, NULL,
		    stress_thread_run, &stress_threads[i]) != 0) {
			fprintf(stderr, "failed to start threads\n");
			exit(EXIT_FAILURE);
		}
	}

	total_messages = 0;
	for (i = 0; i < stress_config.n_thread; ++i) {
		if (pthread_join(stress_threads[i].thread, NULL) != 0) {
			fprintf(stderr, "failed to join thread\n");
			exit(EXIT_FAILURE);
		}
		total_messages += stress_threads[i].messages_sent;
	}

	printf("sent %lu total messages\n", total_messages);
	return 0;
}
