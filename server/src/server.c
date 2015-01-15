#include "session.h"

int
main(int argc, char **argv)
{

	(void)argc;
	(void)argv;

	cix_session_listen(1);
	return 0;
}
