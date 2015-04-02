#ifndef _CIX_MESSAGES_H
#define _CIX_MESSAGES_H

#include <inttypes.h>
#include <stdio.h> /* XXX: remove */

#include "misc.h"

#define CIX_SYMBOL_MAX 7
#define CIX_EXTERNAL_ID_MAX 16

typedef uint64_t cix_order_id_t;
typedef uint64_t cix_user_id_t;
typedef uint64_t cix_execution_id_t;
#define CIX_PR_ID	PRIu64

typedef char cix_symbol_t[CIX_SYMBOL_MAX + 1];
typedef uint32_t cix_quantity_t;
#define CIX_PR_Q	PRIu32

/*
 * This is a fixed-precision integer representing the number of cents
 * (not dollars) in the price.
 */
typedef uint32_t cix_price_t;
#define CIX_PR_P	PRIu32
#define CIX_PRICE_MULTIPLIER 100

enum cix_message_type {
	CIX_MESSAGE_ORDER = 0,
	CIX_MESSAGE_CANCEL,
	CIX_MESSAGE_TRADE,
	CIX_MESSAGE_ACK
};

enum cix_trade_side {
	CIX_TRADE_SIDE_BUY = 0,
	CIX_TRADE_SIDE_SELL = 1
};

/*
 * XXX: Look into whether extra struct padding would improve performance.
 */

/*
 * Each message has a fixed size to avoid size and parsing overhead
 * of a header indicating message length.  Messages consist of a single
 * byte to indicate message type, followed by the data for that message.
 */
struct cix_message_order {
	cix_symbol_t symbol;
	cix_quantity_t quantity;
	cix_price_t price;
	uint8_t side;
	char external_id[CIX_EXTERNAL_ID_MAX];
} CIX_STRUCT_PACKED;

struct cix_message_cancel {
	cix_order_id_t internal_id;
} CIX_STRUCT_PACKED;

struct cix_message_ack {
	/* XXX == */
};

struct cix_message_trade {
	/* XXX */
};

union cix_message_payload {
	struct cix_message_order order;
	struct cix_message_cancel cancel;
	struct cix_message_trade trade;
	struct cix_message_ack ack;
} CIX_STRUCT_PACKED;

struct cix_message {
	uint8_t type;
	union cix_message_payload payload;
} CIX_STRUCT_PACKED;

static size_t __attribute__((unused))
cix_message_length(enum cix_message_type type)
{

	switch (type) {
	case CIX_MESSAGE_ORDER:
		return sizeof(struct cix_message_order);
	case CIX_MESSAGE_CANCEL:
		return sizeof(struct cix_message_cancel);
	case CIX_MESSAGE_TRADE:
		return sizeof(struct cix_message_trade);
	case CIX_MESSAGE_ACK:
		return sizeof(struct cix_message_ack);
	default:
		return 0;
	}

	return 0;
}

#endif /* _CIX_MESSAGES_H */
