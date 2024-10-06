#ifndef __SERVERAUTH_H
#define __SERVERAUTH_H

#include "server.h"
#include "hash_map.h"

/**
 * Authenticates a request. This function is used to authenticate a user request. Returns the authentication result code.
 *
 * @param req The request to authenticate.
 * @param uid The obtained unique ID of the client.
 * @param user_map The user hash map.
 */
int user_auth(request_t* req, client_connection_t* cl, hash_map* user_map);

#endif
