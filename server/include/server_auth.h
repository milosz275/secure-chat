#ifndef __SERVERAUTH_H
#define __SERVERAUTH_H

#include "server.h"

/**
 * Authenticates a request. This function is used to authenticate a user request. Returns the authentication result code.
 *
 * @param req The request to authenticate.
 * @param uid The obtained unique ID of the client.
 */
int user_auth(request_t* req, client_connection_t* cl);

#endif
