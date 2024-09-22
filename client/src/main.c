#include <signal.h>
#include "client.h"
#include "protocol.h"

int main()
{
    signal(SIGINT, int_handler);
    run_client();
}
