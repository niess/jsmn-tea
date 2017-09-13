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

#include "jsmn-tea.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* The low level TEA object. */
struct tea_object {
        struct jsmn_tea api;
        int error_enabled;
        int n_tokens;
        char * path;
        jsmntok_t * tokens;
        char * buffer;
        char data[];
};

/* Stringification of JSMN enums, for error messages. */
static const char * string_jsmntype[5] = { "JSMN_UNDEFINED", "JSMN_OBJECT",
        "JSMN_ARRAY", "JSMN_STRING", "JSMN_PRIMITIVE" };
static const char * string_jsmnerr[3] = { "JSMN_ERROR_NOMEM",
        "JSMN_ERROR_INVAL", "JSMN_ERROR_PART" };

/* Prototypes of error messages. */
static const char * string_error_file = "%s (%d): could not open file `%s`.\n";
static const char * string_error_json =
    "%s (%d): invalid JSON file `%s` (%s).\n";
static const char * string_error_memory =
    "%s (%d): could not allocate memory.\n";
static const char * string_error_mode = "%s (%d): invalid mode (%d).\n";

static const char * string_error_io =
    "%s (%d): error while reading file `%s`.\n";
static const char * string_error_size =
    "%s (%d): [%s #%d] invalid array size (%d != %d).\n";
static const char * string_error_type =
    "%s (%d): [%s #%d] unexpected type (%s).\n";
static const char * string_error_value =
    "%s (%d): [%s #%d] invalid value. Expected %s. Got `%s`.\n";

/* Generic routine for processing an error. */
static int error_raise(
    struct jsmn_tea * tea_, enum jsmnerr rc, const char * fmt, ...)
{
        struct tea_object * tea = (struct tea_object *)tea_;
        if (!tea->error_enabled) return rc;
        if (tea_->error_stream != NULL) {
                va_list valist;
                va_start(valist, fmt);
                vfprintf(tea_->error_stream, fmt, valist);
                va_end(valist);
        }
        if (tea_->error_handler != NULL) tea_->error_handler();
        return rc;
}

/* Print a formated error message BUT without calling the error handler. */
static void error_print(struct jsmn_tea * tea_, const char * fmt, ...)
{
        struct tea_object * tea = (struct tea_object *)tea_;
        if ((tea->error_enabled) && (tea_->error_stream != NULL)) {
                va_list valist;
                va_start(valist, fmt);
                vfprintf(tea_->error_stream, fmt, valist);
                va_end(valist);
        }
}

/* Library function for creating a new `jsmn_tea` instance. */
struct jsmn_tea * jsmn_tea_create(char * arg, enum jsmn_tea_mode mode,
    jsmn_tea_cb * error_handler, FILE * error_stream)
{
        struct jsmn_tea api = { { 0, 0, 0 }, 0, error_handler, error_stream };
        struct tea_object * tea = NULL;
        size_t buffer_size = 0;

        if (mode == JSMN_TEA_MODE_LOAD) {
                /* Open the file. */
                FILE * stream = fopen(arg, "rb");
                if (stream == NULL) {
                        error_raise(&api, 0, string_error_file, __FILE__,
                            __LINE__, arg);
                        return NULL;
                }

                /* Compute the file size. */
                fseek(stream, 0, SEEK_END);
                buffer_size = ftell(stream);
                rewind(stream);

                /* Allocate the required memory. */
                const int n = strlen(arg) + 1;
                tea =
                    malloc(sizeof(*tea) + (n + buffer_size + 1) * sizeof(char));
                if (tea == NULL) {
                        error_raise(
                            &api, 0, string_error_memory, __FILE__, __LINE__);
                        return NULL;
                }

                /* Initialise the new `tea` object. */
                memset(tea, 0x0, sizeof(*tea));
                tea->path = tea->data;
                memcpy(tea->path, arg, n * sizeof(*tea->path));
                tea->buffer = tea->path + n;

                const int nread = fread(
                    tea->buffer, sizeof(*tea->buffer), buffer_size, stream);
                fclose(stream);
                if (nread != buffer_size) {
                        error_print(
                            &api, string_error_io, __FILE__, __LINE__, arg);
                        free(tea);
                        if (error_handler != NULL) error_handler();
                        return NULL;
                }
                tea->buffer[buffer_size] = 0;
        } else if ((mode == JSMN_TEA_MODE_DUP) || (mode == JSMN_TEA_MODE_RAW)) {
                /* Allocate the required memory. */
                buffer_size = strlen(arg);
                const int n = (mode == JSMN_TEA_MODE_DUP) ?
                    (buffer_size + 1) * sizeof(char) :
                    0;
                tea = malloc(sizeof(*tea) + n);
                if (tea == NULL) {
                        error_raise(
                            &api, 0, string_error_memory, __FILE__, __LINE__);
                        return NULL;
                }

                /* Initialise the new `tea` object. */
                memset(tea, 0x0, sizeof(*tea));
                tea->path = NULL;
                if (mode == JSMN_TEA_MODE_DUP) {
                        tea->buffer = tea->data;
                        memcpy(tea->buffer, arg,
                            (buffer_size + 1) * sizeof(*tea->buffer));
                } else
                        tea->buffer = arg;
        } else {
                error_raise(
                    &api, 0, string_error_mode, __FILE__, __LINE__, mode);
                return NULL;
        }

        /* Parse the JSON data. 1st let us check the number of tokens.*/
        jsmn_init(&tea->api.parser);
        tea->n_tokens =
            jsmn_parse(&tea->api.parser, tea->buffer, buffer_size, NULL, 0);
        if (tea->n_tokens <= 0) {
                const int jrc =
                    (tea->n_tokens == 0) ? JSMN_ERROR_INVAL : tea->n_tokens;
                error_print(&api, string_error_json, __FILE__, __LINE__,
                    tea->path, string_jsmnerr[-jrc - 1]);
                free(tea);
                if (error_handler != NULL) error_handler();
                return NULL;
        }

        /* Then let us allocate the tokens array and parse the file again. */
        tea->tokens = malloc(tea->n_tokens * sizeof(*tea->tokens));
        if (tea->tokens == NULL) {
                error_print(&api, string_error_memory, __FILE__, __LINE__);
                free(tea);
                if (error_handler != NULL) error_handler();
                return NULL;
        }
        jsmn_init(&tea->api.parser);
        const int jrc = jsmn_parse(&tea->api.parser, tea->buffer, buffer_size,
            tea->tokens, tea->n_tokens);
        if (jrc < 0) {
                error_print(&api, string_error_json, __FILE__, __LINE__,
                    tea->path, string_jsmnerr[-jrc - 1]);
                free(tea->tokens);
                free(tea);
                if (error_handler != NULL) error_handler();
                return NULL;
        }

        tea->api.error_handler = error_handler;
        tea->api.error_stream = error_stream;
        tea->error_enabled = 1;
        return &tea->api;
}

/* Library function for properly freeing a `jsmn_tea` instance. */
void jsmn_tea_destroy(struct jsmn_tea ** tea_)
{
        if (*tea_ == NULL) return;
        struct tea_object * tea = (struct tea_object *)(*tea_);
        free(tea->tokens);
        free(tea);
        *tea_ = NULL;
}

/* Helper macro for unpacking the TEA object and the next JSON token. */
#define UNPACK_TEA_AND_TOKEN                                                   \
        struct tea_object * tea = (struct tea_object *)tea_;                   \
        jsmntok_t * token = tea->tokens + tea->api.index

/* Helper macro for unpacking the string representation of the JSON token. */
#define UNPACK_STRING_REPR                                                     \
        tea->buffer[token->end] = 0;                                           \
        char * s = tea->buffer + token->start

/* Helper macro for unpacking all of the previous. */
#define UNPACK_ALL                                                             \
        UNPACK_TEA_AND_TOKEN;                                                  \
        UNPACK_STRING_REPR

/* Library function for getting the current JSMN token. */
jsmntok_t * jsmn_tea_token_get(struct jsmn_tea * tea_)
{
        UNPACK_TEA_AND_TOKEN;
        return token;
}

/* Library function for getting the total number of JSMN tokens. */
int jsmn_tea_token_length(struct jsmn_tea * tea_)
{
        struct tea_object * tea = (struct tea_object *)tea_;
        return tea->n_tokens;
}

/* Library function for getting the raw JSON string corresponding to the
 * current JSMN token.
 */
char * jsmn_tea_token_raw(struct jsmn_tea * tea_)
{
        UNPACK_TEA_AND_TOKEN;
        if ((token->type == JSMN_OBJECT) || (token->type == JSMN_OBJECT))
                return NULL;
        UNPACK_STRING_REPR;
        return s;
}

/* Get the next token in the JSON string as a compound type. */
static enum jsmnerr next_compound(
    struct jsmn_tea * tea_, jsmntype_t type, int * size)
{
        UNPACK_TEA_AND_TOKEN;
        if (token->type != type) {
                if (size != NULL) *size = 0;
                return error_raise(&tea->api, JSMN_ERROR_INVAL,
                    string_error_type, __FILE__, __LINE__, tea->path,
                    tea->api.index, string_jsmntype[token->type]);
        }
        if (size != NULL) *size = token->size;
        tea->api.index++;
        return JSMN_SUCCESS;
}

/* Get the header of the next JSON object. */
enum jsmnerr jsmn_tea_next_object(struct jsmn_tea * tea_, int * size)
{
        return next_compound(tea_, JSMN_OBJECT, size);
}

/* Get the header of the next JSON array. */
enum jsmnerr jsmn_tea_next_array(struct jsmn_tea * tea_, int * size)
{
        return next_compound(tea_, JSMN_ARRAY, size);
}

/* Get the next JSON string. */
enum jsmnerr jsmn_tea_next_string(struct jsmn_tea * tea_, char ** string)
{
        UNPACK_ALL;
        if ((token->type == JSMN_PRIMITIVE) && (strcmp(s, "null") == 0)) {
                if (string != NULL) *string = NULL;
                return JSMN_SUCCESS;
        }
        if (token->type != JSMN_STRING) {
                return error_raise(&tea->api, JSMN_ERROR_INVAL,
                    string_error_type, __FILE__, __LINE__, tea->path,
                    tea->api.index, token->type, s);
        }
        if (string != NULL) *string = s;
        tea->api.index++;
        return JSMN_SUCCESS;
}

/* Setters for integer like numbers. */
typedef void integer_setter_t(void * p, long l);

static void integer_to_char(void * p, long l) { *(char *)p = (char)l; }

static void integer_to_unsigned_char(void * p, long l)
{
        *(unsigned char *)p = (unsigned char)l;
}

static void integer_to_short(void * p, long l) { *(short *)p = (short)l; }

static void integer_to_unsigned_short(void * p, long l)
{
        *(unsigned short *)p = (unsigned short)l;
}

static void integer_to_int(void * p, long l) { *(int *)p = (int)l; }

static void integer_to_unsigned_int(void * p, long l)
{
        *(unsigned int *)p = (unsigned int)l;
}

static void integer_to_long(void * p, long l) { *(long *)p = l; }

static void integer_to_unsigned_long(void * p, long l)
{
        *(unsigned long *)p = (unsigned long)l;
}

/* Get the next JSON number. */
enum jsmnerr jsmn_tea_next_number(
    struct jsmn_tea * tea_, enum jsmn_tea_type type, void * value)
{
        UNPACK_ALL;
        if (token->type != JSMN_PRIMITIVE) {
                return error_raise(&tea->api, JSMN_ERROR_INVAL,
                    string_error_type, __FILE__, __LINE__, tea->path,
                    tea->api.index, string_jsmntype[token->type]);
        }
        char * endptr;
        if (type < JSMN_TEA_TYPE_FLOAT) {
                const long l = strtol(s, &endptr, 0);
                if (*endptr != 0) {
                        return error_raise(&tea->api, JSMN_ERROR_INVAL,
                            string_error_value, __FILE__, __LINE__, tea->path,
                            tea->api.index, "an integer", s);
                }
                if (value != NULL) {
                        integer_setter_t * setter[] = { &integer_to_char,
                                &integer_to_unsigned_char, &integer_to_short,
                                &integer_to_unsigned_short, &integer_to_int,
                                &integer_to_unsigned_int, &integer_to_long,
                                &integer_to_unsigned_long };
                        setter[type](value, l);
                }
        } else {
                const double d = strtod(s, &endptr);
                if (*endptr != 0) {
                        return error_raise(&tea->api, JSMN_ERROR_INVAL,
                            string_error_value, __FILE__, __LINE__, tea->path,
                            tea->api.index, "a floating number", s);
                }
                if (value != NULL) {
                        if (type == JSMN_TEA_TYPE_FLOAT)
                                *(float *)value = (float)d;
                        else
                                *(double *)value = (double)d;
                }
        }
        tea->api.index++;
        return JSMN_SUCCESS;
}

/* Get the next JSON boolean. */
enum jsmnerr jsmn_tea_next_bool(struct jsmn_tea * tea_, int * value)
{
        UNPACK_ALL;
        if (token->type != JSMN_PRIMITIVE) {
                return error_raise(&tea->api, JSMN_ERROR_INVAL,
                    string_error_type, __FILE__, __LINE__, tea->path,
                    tea->api.index, string_jsmntype[token->type]);
        }
        if (strcmp(s, "true") == 0)
                if (value != NULL)
                        *value = 1;
                else if (strcmp(s, "false") == 0)
                        if (value != NULL)
                                *value = 0;
                        else {
                                return error_raise(&tea->api, JSMN_ERROR_INVAL,
                                    string_error_value, tea->path,
                                    tea->api.index, "a boolean", s);
                        }
        tea->api.index++;
        return JSMN_SUCCESS;
}

/* Get the next JSON null. */
enum jsmnerr jsmn_tea_next_null(struct jsmn_tea * tea_)
{
        UNPACK_ALL;
        if ((token->type != JSMN_PRIMITIVE) || (strcmp(s, "null") != 0)) {
                return error_raise(&tea->api, JSMN_ERROR_INVAL,
                    string_error_type, __FILE__, __LINE__, tea->path,
                    tea->api.index, token->type, s);
        }
        return JSMN_SUCCESS;
}

/* Library function for disabling errors handling and dumping. */
void jsmn_tea_error_disable(struct jsmn_tea * tea_)
{
        struct tea_object * tea = (struct tea_object *)tea_;
        tea->error_enabled = 0;
}

/* Library function for enabling errors handling and dumping. */
void jsmn_tea_error_enable(struct jsmn_tea * tea_)
{
        struct tea_object * tea = (struct tea_object *)tea_;
        tea->error_enabled = 1;
}

/* Library function for raising a custom error. */
enum jsmnerr jsmn_tea_error_raise(
    struct jsmn_tea * tea_, enum jsmnerr rc, const char * fmt, ...)
{
        struct tea_object * tea = (struct tea_object *)tea_;
        if (!tea->error_enabled) return rc;
        if (tea_->error_stream != NULL) {
                struct tea_object * tea = (struct tea_object *)tea_;
                fprintf(tea_->error_stream, "%s (%d): [%s #%d] ", __FILE__,
                    __LINE__, tea->path, tea_->index);
                va_list valist;
                va_start(valist, fmt);
                vfprintf(tea_->error_stream, fmt, valist);
                va_end(valist);
        }
        if (tea_->error_handler != NULL) tea_->error_handler();
        return rc;
}
