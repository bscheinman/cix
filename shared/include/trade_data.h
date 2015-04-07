#ifndef _CIX_TRADE_DATA_H
#define _CIX_TRADE_DATA_H

#include <inttypes.h>

#include "messages.h"

struct cix_execution {
	cix_execution_id_t id;
	cix_user_id_t buyer;
	cix_user_id_t seller;
	cix_symbol_t symbol;
	cix_quantity_t quantity;
	cix_price_t price;
};

#endif /* _CIX_TRADE_DATA_H */
