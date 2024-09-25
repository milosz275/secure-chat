#include <stdio.h>
#include <stdlib.h>
#include "server.h"
#include "protocol.h"

int main()
{
    setbuf(stdout, NULL);

    int result = run_server();
    if (result != 0)
    {
        fprintf(stderr, "Server failed to run with error code: %d\n", result);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
