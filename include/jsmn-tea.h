/*
 * Copyright (C) 2017 Université Clermont Auvergne, CNRS/IN2P3, LPC
 * Author: Valentin NIESS (niess@in2p3.fr)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __JSMN_TEA_H_
#define __JSMN_TEA_H_

#ifndef FILE
#include <stdio.h>
#endif
#include "jsmn.h"

#ifdef __cplusplus
extern "C" {
#endif

/** JSMN return code for notifying a success. */
#define JSMN_SUCCESS 0

/** Generic callback, used for registering an error handler. */
typedef void jsmn_tea_cb(void);

/**
 * Handle for Text Encapsulation and Analysis using JSMN.
 *
 * This is a semi-opaque object. An instance of this structure **must** be
 * created only with the `jsmn_tea_create` function. It exposes mutable data,
 * i.e. that can be directly altered. However it also manages low level data
 * and dynamic memory, hidden to the end user. Use the `jsmn_tea_destroy`
 * function in order to properly free this object.
 */
struct jsmn_tea {
        /* JSMN base parser. Note that a `jsmn_tea` instance can be casted
         * to a jsmn parser if needed.
         */
        jsmn_parser parser;
        /* Index of the current JSMN token. */
        int index;
        /* User supplied error handler, or `NULL` if none. */
        jsmn_tea_cb * error_handler;
        /* Output stream for errors, or `NULL` if muted. */
        FILE * error_stream;
};

/** Creation modes for a `jsmn_tea` instance. */
enum jsmn_tea_mode {
        /** Create the `jsmn_tea` from a duplicated JSON string. */
        JSMN_TEA_MODE_DUP = 0,
        /** Load the JSON data from a file. */
        JSMN_TEA_MODE_LOAD,
        /** Create the `jsmn_tea` from a raw JSON string. */
        JSMN_TEA_MODE_RAW
};

/** The C numeric data types that a JSON number might be converted to. */
enum jsmn_tea_type {
        JSMN_TEA_TYPE_CHAR = 0,
        JSMN_TEA_TYPE_UNSIGNED_CHAR,
        JSMN_TEA_TYPE_SHORT,
        JSMN_TEA_TYPE_UNSIGNED_SHORT,
        JSMN_TEA_TYPE_INT,
        JSMN_TEA_TYPE_UNSIGNED_INT,
        JSMN_TEA_TYPE_LONG,
        JSMN_TEA_TYPE_UNSIGNED_LONG,
        JSMN_TEA_TYPE_FLOAT,
        JSMN_TEA_TYPE_DOUBLE
};

/**
 * Create a new `jsmn_tea` instance.
 * @param  arg           The JSON data string or a filename.
 * @param  mode          The `jsmn_tea_type` creation mode.
 * @param  error_handler An optional error handler, or `NULL`.
 * @param  error_stream  The stream to whicj errors are reported, or `NULL`.
 * @return               A pointer to the created `jsmn_tea` instance in case
 *                         of success, or `NULL` otherwise.
 */
struct jsmn_tea * jsmn_tea_create(char * arg, enum jsmn_tea_mode mode,
    jsmn_tea_cb * error_handler, FILE * error_stream);

/**
 * Properly destroy a `jsmn_tea` instance.
 * @param tea A pointer to a `jsmn_tea` instance.
 */
void jsmn_tea_destroy(struct jsmn_tea ** tea);

/**
 * Get the current JSMN token.
 * @param  tea A pointer to a `jsmn_tea` instance.
 * @return     The corresponding JSMN token.
 */
jsmntok_t * jsmn_tea_token_get(struct jsmn_tea * tea);

/**
 * Get the total number of JSMN tokens.
 * @param  tea A pointer to a `jsmn_tea` instance.
 * @return     The number of tokens.
 */
int jsmn_tea_token_length(struct jsmn_tea * tea);

/**
 * Get the raw string representation of the current token.
 * @param  tea  A pointer to a `jsmn_tea` instance.
 * @param  mode Flag to specify that a copy should be returned, or not.
 * @param  raw  The corresponding raw string.
 * @return      `JSMN_SUCCESS` on success or a `jsmnerr` enum value otherwise.
 *
 * Set *mode* to `JSMN_TEA_MODE_DUP` if a copy of the raw string should be
 * provided. Otherwise set it to `JSMN_TEA_MODE_RAW`. Note that in the former
 * case memory is dynamically allocated.
 *
 * **Warning** : for compound objects, i.e. JSON string or Array, the raw
 * representation might have been altered by the parser if the content
 * has already been browsed.
 */
enum jsmnerr jsmn_tea_token_raw(
     struct jsmn_tea * tea, enum jsmn_tea_mode mode, char ** raw);


/**
 * Skip the current JSMN token, recursively.
 * @param  tea A pointer to a `jsmn_tea` instance.
 * @return     `JSMN_SUCCESS` on success or a `jsmnerr` enum value otherwise.
 */
enum jsmnerr jsmn_tea_token_skip(struct jsmn_tea * tea);

/**
 * Get the next token as JSON object header.
 * @param  tea  A pointer to a `jsmn_tea` instance.
 * @param  size The number of sub elements of the JSON object.
 * @return      `JSMN_SUCCESS` on success or a `jsmnerr` enum value otherwise.
 *
 * **Note** that the *size* value corresponds to the number of { key : value}
 * pairs inside the JSON object. Not to the number of JSMN tokens.
 */
enum jsmnerr jsmn_tea_next_object(struct jsmn_tea * tea, int * size);

/**
 * Get the next token as a JSON array header.
 * @param  tea  A pointer to a `jsmn_tea` instance.
 * @param  size The number of items in the JSON array.
 * @return      `JSMN_SUCCESS` on success or a `jsmnerr` enum value otherwise.
 *
 * **Note** that the *size* value corresponds to the number of JSON items inside
 * the array. Not to the number of JSMN tokens.
 */
enum jsmnerr jsmn_tea_next_array(struct jsmn_tea * tea, int * size);

/**
 * Get the next token as a JSON string.
 * @param  tea    A pointer to a `jsmn_tea` instance.
 * @param  key    A flag indicating that the string is a key in a JSON object.
 * @param  string The corresponding C formated string.
 * @return        `JSMN_SUCCESS` on success or a `jsmnerr` enum value otherwise.
 *
 * **Note** that the returned C string is null terminated.
 */
enum jsmnerr jsmn_tea_next_string(
    struct jsmn_tea * tea, int key, char ** string);

/**
 * Get the next token as a JSON number.
 * @param  tea   A pointer to a `jsmn_tea` instance.
 * @param  type  The C type of the storing variable.
 * @param  value The address where the value should be stored.
 * @return       `JSMN_SUCCESS` on success or a `jsmnerr` enum value otherwise.
 */
enum jsmnerr jsmn_tea_next_number(
    struct jsmn_tea * tea, enum jsmn_tea_type type, void * value);

/**
 * Get the next token as a JSON boolean.
 * @param  tea   A pointer to a `jsmn_tea` instance.
 * @param  value The C translated value, i.e. 0 or 1.
 * @return       `JSMN_SUCCESS` on success or a `jsmnerr` enum value otherwise.
 */
enum jsmnerr jsmn_tea_next_bool(struct jsmn_tea * tea, int * value);

/**
 * Get the next token as JSON null.
 * @param  tea A pointer to a `jsmn_tea` instance.
 * @return     `JSMN_SUCCESS` on success or a `jsmnerr` enum value otherwise.
 */
enum jsmnerr jsmn_tea_next_null(struct jsmn_tea * tea);

/**
 * Disable errors handling and dumping.
 * @param tea A pointer to a `jsmn_tea` instance.
 *
 * **Warning** : the library functions will still return a `jsmnerr` enum value
 * in case of error.
 */
void jsmn_tea_error_disable(struct jsmn_tea * tea);

/**
 * Enable errors handling and dumping.
 * @param tea A pointer to a `jsmn_tea` instance.
 *
 * **Note** that a newly created `jsmn_tea` instance starts with errors enabled.
 */
void jsmn_tea_error_enable(struct jsmn_tea * tea);

/**
 * Raise a custom JSON parsing error.
 * @param  tea     A pointer to a `jsmn_tea` instance.
 * @param  rc      The `jsmnerr` enum value to report.
 * @param  fmt     A printf like format string.
 * @param  VARARGS Varibale arguments to the formating.
 * @return         The corresponding jsmnerr` enum value, for convenience.
 *
 * **Note** that the formated message is prepended with the current C and JSON
 * file(s) information.
 */
enum jsmnerr jsmn_tea_error_raise(
    struct jsmn_tea * tea, enum jsmnerr rc, const char * fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* __JSMN_TEA_H_ */
