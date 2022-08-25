/* This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 **/

#include <sqlite3ext.h> /* Do not use <sqlite3.h>! */
SQLITE_EXTENSION_INIT1
#include "numpy_reader.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/// === Loading this extention in sqlite
///   .load ./blopy

/// === Compiling
///   gcc -g -fPIC -shared blopy.c  numpy_reader.c numpy_reader.h -o blopy.so

// In the final version (1.0) this extention should provide the following sqlite functions
// * np_header(col) -> header: dtype, fortran_order, shape
// * np_shape(col) -> shape of stored array
// * np(col) -> convert BLOB to TEXT. Columns are seperated by '\t' and rows by '\n'
// * np(col, fmt) -> convert BLOB to TEXT where each value is formated according to `fmt`
// * np_head(col, num) -> first `num` rows
// * np_head(col, num, fmt) -> first `num` rows where each value is formated according to `fmt`
// * np_tail(col, num) -> last `num` rows
//                        TODO (check for positivity)
// * np_tail(col, num, fmt) -> last `num` rows where each value is formated according to `fmt`
//                             TODO (check for positivity)
// Potential further functions could do operations like min and max or mean. Or BLOB-size (without
// header based on wordsize*size).
// For currently supported sqlite-functions look into sqlite3_blopy_init

/// Returns the numpy version number
/// The identity if input is not a BLOB at all
static void numpy_version(sqlite3_context *context, int argc, sqlite3_value **argv)
{
    if (sqlite3_value_type(argv[0]) == SQLITE_BLOB) {
        // Get the BLOB ptr and cast it to an unsigned char
        unsigned char* input = (unsigned char*) sqlite3_value_blob(argv[0]);

        unsigned short version = read_magic(input);
        if (version == -1)
            sqlite3_result_value(context, argv[0]);
        else
            sqlite3_result_int(context, version);
    } else {
        sqlite3_result_value(context, argv[0]);
    }
}

/// Returns "true" if the field in question is a numpy BLOB, "false" if not.
/// The identity if input is not a BLOB at all
static void is_numpy_blob(sqlite3_context *context, int argc, sqlite3_value **argv)
{
    if (sqlite3_value_type(argv[0]) == SQLITE_BLOB) {
        // Get the BLOB ptr and cast it to an unsigned char
        unsigned char* input = (unsigned char*) sqlite3_value_blob(argv[0]);

        if (read_magic(input) == -1)
            sqlite3_result_text(context, "false", -1, SQLITE_TRANSIENT);
        else
            sqlite3_result_text(context, "true", -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_result_value(context, argv[0]);
    }
}

/// Returns the content of a numpy BLOB
/// Or the identity if input is not a BLOB at all
static void numpy_reader(sqlite3_context *context, int argc, sqlite3_value **argv)
{
    if (sqlite3_value_type(argv[0]) == SQLITE_BLOB) {
        // Get the BLOB ptr and cast it to an unsigned char
        unsigned char* input = (unsigned char*) sqlite3_value_blob(argv[0]);

        char* content = BLOB_to_str(input);
        sqlite3_result_text(context, content, -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_result_value(context, argv[0]);
    }
}

static void numpy_size(sqlite3_context *context, int argc, sqlite3_value **argv)
{
    if (sqlite3_value_type(argv[0]) == SQLITE_BLOB) {
        // Get the BLOB ptr and cast it to an unsigned char
        unsigned char* input = (unsigned char*) sqlite3_value_blob(argv[0]);

        Header_data* header_data;
        header_data = malloc(sizeof(*header_data));

        int header_length = read_header(input, header_data); // check that header_length > 0?
        int out = header_data->size;

        free(header_data);

        sqlite3_result_int(context, out);
    } else {
        sqlite3_result_value(context, argv[0]);
    }
}

static void numpy_desc(sqlite3_context *context, int argc, sqlite3_value **argv)
{
    if (sqlite3_value_type(argv[0]) == SQLITE_BLOB) {
        // Get the BLOB ptr and cast it to an unsigned char
        unsigned char* input = (unsigned char*) sqlite3_value_blob(argv[0]);

        Header_data* header_data;
        header_data = malloc(sizeof(*header_data));

        int header_length = read_header(input, header_data); // check that header_length > 0?
        char buffer[header_data->descr_len];
        strcpy(buffer, header_data->descr);

        free(header_data);

//         sqlite3_result_text(context, header_data->descr, -1, SQLITE_TRANSIENT); // Is this safe?
        sqlite3_result_text(context, buffer, -1, SQLITE_TRANSIENT); // Is this safe?
    } else {
        sqlite3_result_value(context, argv[0]);
    }
}

#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_blopy_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi)
{
  int rc = SQLITE_OK;
  SQLITE_EXTENSION_INIT2(pApi);
  //   (void)pzErrMsg;  /* Unused parameter */

  rc = sqlite3_create_function(db, "isnp", 1,
                               SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, is_numpy_blob, 0, 0);

  rc = sqlite3_create_function(db, "np_ver", 1,
                               SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, numpy_version, 0, 0);

  rc = sqlite3_create_function(db, "np_size", 1,
                               SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, numpy_size, 0, 0);
//   rc = sqlite3_create_function(db, "np_shape", 1,
//                                SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, numpy_shape, 0, 0);
  rc = sqlite3_create_function(db, "np_desc", 1,
                               SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, numpy_desc, 0, 0);

  rc = sqlite3_create_function(db, "np", 1,
                               SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, numpy_reader, 0, 0);

//   if (rc) return rc;

  return rc;
}
