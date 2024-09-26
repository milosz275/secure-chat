#ifndef __SERVERDB_H
#define __SERVERDB_H

#include <sqlite3.h>

/**
 * Database connector. This function is used to connect to a SQLite3 database and leave it open for further operations.
 *
 * @param db The SQLite3 database.
 * @param db_name The name of the database.
 * @return The database connection exit code.
 */
int connect_db(sqlite3** db, char* db_name);

/**
 * Database setup. This function is used to set up and close a SQLite3 database, create its tables according to the schema if they do not exist.
 *
 * @param db The SQLite3 database.
 * @param db_name The name of the database.
 * @return The database setup exit code.
 */
int setup_db(sqlite3** db, char* db_name);

#endif
