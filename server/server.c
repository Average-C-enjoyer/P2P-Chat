#include "server.h"

int main()
{
	if (server_run() != 0) return 1;
    return 0;
}
