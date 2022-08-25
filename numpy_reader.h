/* This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 **/

#ifndef NUMPYREADER_FILE
#define NUMPYREADER_FILE
#include <stdbool.h>

/// NOTE This is not a sqlite extention. Wrapper code is in extension.c


extern const unsigned short MAGIC_NUMPY[];
extern const short MAGIC_LEN;
extern const short VERSION_LEN;

// The new "class" is of type `struct Header_data`
struct Header_data {
    bool fortran_order;

    int* shape; int shape_len;
    int size;

    char* descr; int descr_len;
    char type;
    bool littleEndian;
    unsigned int wordsize_in_bytes; // size of each element/word length
};
// We created the type `Header_data`
typedef struct Header_data Header_data;
// One could combine both above into a one-liner but this looks easier to understand
//   typedef struct Header_data { bool a;} Header_data;

// Public:
extern short read_magic(unsigned char* ptr_inputBlob);
extern char* BLOB_to_str(unsigned char* ptr_inputBlob);
extern int read_header(unsigned char* ptr_inputBlob, Header_data* header_data);

// Private:
static void* data;

static int _read_header_length(unsigned char* ptr_inputBlob);
#endif
