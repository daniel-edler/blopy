/* This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
**/

#include "numpy_reader.h"
#include <math.h>
#include <complex.h>
#include <stdlib.h>
#include <stdio.h>  // printf()
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <limits.h> // provides us with CHAR_BIT

#define SIGN_SQ '\''
#define SIGN_DQ '"'

//                                 \\x93,   N,  U,  M,  P,  Y
const unsigned short MAGIC_NUMPY[] = {147, 78, 85, 77, 80, 89};
const short MAGIC_LEN = 6; // Unlike numpy, I define the MAGIC_LEN *without* the version
const short VERSION_LEN = 2;

/// Reads the first MAGIC_LEN bytes from ptr_inputBlob. If it's a numpy file it reads the next
/// VERSION_LEN bytes and returns the numpy version.
/// ASSUMPTION (not checked): The input `ptr_inputBlob` is an array with at least MAGIC_LEN+VERSION_LEN elements
///
/// @Returns
///    * -1,      if no numpy object was detected
///    * VERSION, otherwise
short read_magic(unsigned char* ptr_inputBlob)
{
    for (int i = 0; i < MAGIC_LEN; i++) {
        if ((unsigned short) ptr_inputBlob[i] != MAGIC_NUMPY[i]) {
            return -1;
        }
    }

    unsigned short out = 0;
    for (int i = 0; i < VERSION_LEN; i++) {
        out += ((unsigned short) ptr_inputBlob[MAGIC_LEN+i]) * pow(10, VERSION_LEN-i-1);
    }

    return out;
}


/// Return the header length from the next two small-endian bytes
static int read_header_length(unsigned char* ptr_inputBlob)
{
    // Multiplying with 0x100u has the same effect as shifting 8 to the left (with '<< 8' ?)
    return ptr_inputBlob[MAGIC_LEN+VERSION_LEN+1]*0x100u
          +ptr_inputBlob[MAGIC_LEN+VERSION_LEN];
}

/// Returns the length (number of characters) of the header
/// If negative it contains an error message
int read_header(unsigned char* ptr_inputBlob, Header_data* header_data)
{
    int version = read_magic(ptr_inputBlob);
    //printf("VERSION=%d", version);

    if (version == -1) {
        printf("No valid numpy BLOB found\n");
        return -1;
    }

    if ((version < 10) || (version >= 20)) {
        printf("Error in understanding file definition version. Only Ver. 1 supported");
        return -2;
    }

    int header_length = read_header_length(ptr_inputBlob);

    if (header_length <= 0) {
        printf("Header has length 0 (or smaller)\n");
        return -3;
    }

    // The header describes the array's format. It's a Python literal expression of a dictionary.
    // It is terminated by a newline and padded with spaces
    char buffer[header_length];
    char cur_char;
    int counter;
    bool is_open;
    int start_hdr = MAGIC_LEN+VERSION_LEN+2;
    for (int i = 0; i < header_length; i++) {
        cur_char = (char) ptr_inputBlob[start_hdr+i];
        if (cur_char == 'd') {
            // Check for right closing quoting sign of key 'descr'
            cur_char = (char) ptr_inputBlob[start_hdr+i+5];
            if (   (cur_char == SIGN_SQ) || (cur_char == SIGN_DQ) ) {
                i += 6;

                // Move the description content between the quotes to the buffer
                counter = 0;
                is_open = false;
                for (int j = i; j < header_length; j++) {
                    cur_char = (char) ptr_inputBlob[start_hdr+j];
                    if ( (cur_char == SIGN_SQ) || (cur_char == SIGN_DQ) ) {
                        if (is_open)
                            break;
                        else
                            is_open = true;
                    }
                    else {
                        if (is_open) {
                            buffer[counter++] = cur_char;
                        }
                    }
                }

                // TODO use functions from <string>
                char* desc = malloc((counter+1) * sizeof(char));
                for (int j = 0; j < counter; j++) {
                    desc[j] = buffer[j];
                }
                desc[counter] = '\0';

                header_data->descr = desc;
                header_data->descr_len = counter;

                // Def of descr: endian byte, data type, word size
                // endian:
                //   '<' : little endian
                //   '>' : big endian (not supported)
                //   '=' : native (what does that mean?)
                //   '|' : not applicable (e.g. object, bool?)
                // data type:
                //   'b' : bool
                //   'i' : int
                //   'f' : float
                //   'c' : complex
                //   'U' : text (I assume unicode which means 1 char is 1 byte)
                //   'O' : object
                // word size: size in byte
                header_data->littleEndian = (desc[0] == '<' || desc[0] == '|' ? true : false);
                header_data->type = desc[1];
//                 header_data->wordsize_in_bytes = (unsigned int)atoi(desc+2);
                header_data->wordsize_in_bytes = atoi(desc+2);
            }
            // else maybe raise error because I do not understand the input
        }

        else if (cur_char == 's') {
            // Check for right closing quoting sign of key 'shape'
            cur_char = (char) ptr_inputBlob[start_hdr+i+5];
            if (   (cur_char == SIGN_SQ) || (cur_char == SIGN_DQ) ) {
                i += 6;

                // Assume the shape in a not-nested tuple. Only one '(' and ')' each
                // Move the (tuple) content between the parenthesis to the buffer
                counter = 0;
                is_open = false;
                for (int j = i; j < header_length; j++) {
                    cur_char = (char) ptr_inputBlob[start_hdr+j];
                    if (cur_char == '(') {
                        is_open = true;
                    }
                    else if (cur_char == ')') {
                        break;
                    }
                    else {
                        if (is_open) {
                            buffer[counter++] = cur_char;
                        }
                    }
                }

                // The tuple text contains a number and a comma.
                // This means that the largest possible number of integers is counter-1
                int shape[counter-1];

                // Copy the tuple content to a new variable. The buffer may include content
                // from previous runs
                char tuple[counter];
                memcpy(tuple, buffer, counter+1);

                // Split at ',' and convert to integers
                counter = 0;
                char* elements;
                elements = strtok(tuple, ",");
                while (elements != NULL) {
                    shape[counter] = atoi(elements);
                    counter += 1;
                    elements = strtok(NULL, ",");
                }

                // TODO Make the shape array shorter/as short as possible
                //      Although, is it worth it? Tiny bit of memory vs (tiny) bit of reshuffling
                //      But we could count the comma in advance
                header_data->shape = shape;
                header_data->shape_len = counter;

                // TODO strictly speaking only if shape has entries/ shape_len > 0
                // TODO figure out why it can have 0 elements in one dimension/element of shape
                header_data->size = 1;
                for (int j = 0; j < header_data->shape_len; j++) {
                    header_data->size *= header_data->shape[j] > 0 ? header_data->shape[j] : 1;
                }
            }

        }

        else if (cur_char == 'f') {
            // Check for right closing quoting sign of key 'fortran_order'
            cur_char = (char) ptr_inputBlob[start_hdr+i+13];
            if ((cur_char == SIGN_SQ) || (cur_char == SIGN_DQ)) {
                i += 14;

                for (int j = i; j < header_length; j++) {
                    cur_char = (char) ptr_inputBlob[start_hdr+j];

                    if (cur_char == 'T') {
                        header_data->fortran_order = true;
                        i += 4;
                        break;
                    }
                    else if (cur_char == 'F') {
                        header_data->fortran_order = false;
                        i += 5;
                        break;
                    }
                }
            }
        }

        else if (cur_char == '}') {
            break;
        }

    } // for-loop over i

    return header_length+start_hdr;
}

/// Reads the input BLOB (no type check!) and returns, if the BLOB is of type numpy, a string
/// representation of the array
char* BLOB_to_str(unsigned char* ptr_inputBlob)
{
    bool do_debug = false;
    char* out = malloc(sizeof(char)*256); // TODO why restrict to some value? May cause problems!

    Header_data* header_data;
    header_data = malloc(sizeof(*header_data));

    int header_length = read_header(ptr_inputBlob, header_data);
    if (header_length < 0)
        return "some error";

    if (do_debug) {
        printf("\n<debug>\n");

        printf(" desc=%s\n", header_data->descr);
        printf(" ├strlen=%d\n", header_data->descr_len);
        printf(" ├type=%c\n", header_data->type);
        printf(" └wordSize=%i\n", header_data->wordsize_in_bytes);

        printf(" shape=(");
        for (short i = 0; i < header_data->shape_len; i++) {
            printf("%i,", header_data->shape[i]);
        }
        printf(")\n");
        printf(" └size=%i\n", header_data->size);

        if (header_data->fortran_order)
            printf(" fortran_order=True\n");
        else
            printf(" fortran_order=False\n");
        printf("</debug>\n");
    } // if

    data = &ptr_inputBlob[header_length];

    int N = 15; // How many elements
    N = N > header_data->size ? header_data->size : N;
    short out_col_width = 16;
    char buffer[out_col_width];
    if (header_data->type == 'O') {

    } else if (header_data->type == 'i') { // int
        // For correct conversion it must be ensured that a double is really a int32
        static_assert(sizeof(int) * CHAR_BIT == 32, "32-bit int is assumed.");

        for (int j = 0; j < N; j++) {
            int out_int;

            if (header_data->wordsize_in_bytes == 8) {
                out_int = ((long*)data)[j];
            } else if (header_data->wordsize_in_bytes == 4) {
                out_int = ((int*)data)[j];
            } else if (header_data->wordsize_in_bytes == 2) {
                out_int = ((short*)data)[j];
            } else {
                // TODO wordsize_in_bytes == 1 could be implemented
                printf("data type not supported %i", header_data->wordsize_in_bytes);
                break;
            }

            //printf(" output=%i\n", ((long*)data)[j]);
            snprintf(buffer, out_col_width, "%i\t", out_int); // TODO probably it's not checking end of out variable (aka \0)
            strcat(out, buffer);
        }
    } else if (header_data->type == 'f') { // float
        // For correct conversion it must be ensured that a double is really a float64
        // Thanks to https://stackoverflow.com/a/72248807/4040140
        static_assert(sizeof(double) * CHAR_BIT == 64, "64-bit double is assumed.");

        for (int j = 0; j < N; j++) {
            double out_float;

            if (header_data->wordsize_in_bytes == 8) {
                out_float = ((double*)data)[j];
//                 printf("%f\t", out_float);
            } else if (header_data->wordsize_in_bytes == 4) {
                out_float = ((float*)data)[j];
            } else if (header_data->wordsize_in_bytes == 16) {
                out_float = ((long double*)data)[j];
            } else {
                printf("data type not supported %i", header_data->wordsize_in_bytes);
                break;
            }

            //printf(" output=%g\n", ((double*)data)[j]);
            snprintf(buffer, out_col_width, "%6.7g\t", out_float); // TODO improve output
            strcat(out, buffer);
        }
    } else if (header_data->type == 'c') { // complex
        // For correct conversion it must be ensured that a double is really a float64
        // Thanks to https://stackoverflow.com/a/72248807/4040140
        static_assert(sizeof(double) * CHAR_BIT == 64, "64-bit double is assumed.");

        for (int j = 0; j < N*2; j=j+2) {
            double complex out_complex;

            if (header_data->wordsize_in_bytes == 16) {
                out_complex = ((double*)data)[j] + ((double*)data)[j+1]*I;
            } else if (header_data->wordsize_in_bytes == 8) {
                out_complex = ((float*)data)[j] + ((float*)data)[j+1]*I;
            } else if (header_data->wordsize_in_bytes == 32) {
                out_complex = ((long double*)data)[j] + ((long double*)data)[j+1]*I;
            } else {
                printf("data type not supported %i", header_data->wordsize_in_bytes);
                break;
            }

//             printf(" output=(%g, %g)\n", creal(out_complex), cimag(out_complex));
            snprintf(buffer, out_col_width, "(%4.g,%3.g)\t", creal(out_complex), cimag(out_complex));
            strcat(out, buffer);
        }
    } else if (header_data->type == 'U') { // text
        strcat(out,  "data type U not yet supported");
    } else {
        strcat(out,  "data type not supported");
    }


    // Clean up
    free(header_data);

    if (N < header_data->size)
        strcat(out, "...\0");
    else
        strcat(out, "\0");
    return out;
}

