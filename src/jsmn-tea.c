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

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "jsmn-tea.h"

/* Maximum size of a JSMN tag string. */
#define TAG_SIZE 13

/* The low level TEA object. */
struct tea_object {
        struct jsmn_tea api;
        int n_tokens;
        int buffer_size;
        char * strtoken;
        char * tag;
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
static const char * string_error_json[2] = { "Invalid JSON file", "%s" };
static const char * string_error_mode[2] = { "Invalid mode", "%d" };
static const char * string_error_type[2] = { "Unexpected type", "%s, %s" };
static const char * string_error_empty[2] = { "Missing value", "\"%s\", %s" };
static const char * string_error_value[2] = { "Invalid value", "\"%s\", %s" };

/* Library function for creating a new `jsmn_tea` instance. */
struct jsmn_tea * jsmn_tea_create(
    char * arg, enum jsmn_tea_mode mode, struct roar_handler * handler)
{
        struct tea_object header = { { { 0, 0, 0 }, 1, handler } };
        struct tea_object * tea = NULL;
        size_t buffer_size = 0;

        if (mode == JSMN_TEA_MODE_LOAD) {
                /* Open the file. */
                FILE * stream = fopen(arg, "rb");
                if (stream == NULL) {
                        ROAR_ERRNO_MESSAGE(handler, &jsmn_tea_create, 0, arg);
                        return NULL;
                }

                /* Compute the file size. */
                fseek(stream, 0, SEEK_END);
                buffer_size = ftell(stream);
                rewind(stream);

                /* Allocate the required memory. */
                const int path_size = strlen(arg);
                tea = malloc(sizeof(*tea) +
                    (path_size + TAG_SIZE + buffer_size + 1) * sizeof(char));
                if (tea == NULL) {
                        ROAR_ERRNO(handler, &jsmn_tea_create, 0);
                        return NULL;
                }

                /* Initialise the new `tea` object. */
                memset(tea, 0x0, sizeof(*tea));
                tea->strtoken = tea->data;
                memcpy(tea->strtoken, arg, path_size * sizeof(*tea->strtoken));
                tea->tag = tea->data + path_size;
                tea->buffer = tea->tag + TAG_SIZE;
                const int nread = fread(
                    tea->buffer, sizeof(*tea->buffer), buffer_size, stream);
                fclose(stream);
                if (nread != buffer_size) {
                        free(tea);
                        ROAR_ERRNO(handler, &jsmn_tea_create, EIO);
                        return NULL;
                }
                tea->buffer[buffer_size] = 0;
        } else if ((mode == JSMN_TEA_MODE_DUP) || (mode == JSMN_TEA_MODE_RAW)) {
                /* Allocate the required memory. */
                buffer_size = strlen(arg);
                const int n = (mode == JSMN_TEA_MODE_DUP) ?
                    (buffer_size + 1) * sizeof(char) :
                    0;
                tea = malloc(
                    sizeof(*tea) + (n + TAG_SIZE) * sizeof(*tea->strtoken));
                if (tea == NULL) {
                        ROAR_ERRNO(handler, &jsmn_tea_create, 0);
                        return NULL;
                }

                /* Initialise the new `tea` object. */
                memset(tea, 0x0, sizeof(*tea));
                tea->strtoken = tea->tag = tea->data;

                if (mode == JSMN_TEA_MODE_DUP) {
                        tea->buffer = tea->tag + TAG_SIZE;
                        memcpy(tea->buffer, arg, n);
                } else
                        tea->buffer = arg;
        } else {
                ROAR_ERRNO_FORMAT(
                    handler, &jsmn_tea_create, EINVAL, "mode=%d", mode);
                return NULL;
        }

        /* Let us analyse the content with JSMN, i.e. parse the tokens. */
        jsmn_init(&tea->api.parser);
        tea->tokens = NULL;
        int size = 0;
        tea->n_tokens = JSMN_ERROR_NOMEM;
        while (tea->n_tokens == JSMN_ERROR_NOMEM) {
                /* Increase the memory until it contains all tokens. */
                size += 2048;
                void * tmp = realloc(tea->tokens, size * sizeof(*tea->tokens));
                if (tmp == NULL) {
                        free(tea->tokens);
                        free(tea);
                        ROAR_ERRNO(handler, &jsmn_tea_create, 0);
                        return NULL;
                }
                tea->tokens = tmp;

                /* Parse with JSMN. */
                tea->n_tokens = jsmn_parse(&tea->api.parser, tea->buffer,
                    buffer_size, tea->tokens, size);
        }

        /* Let us check the termination condition of the parsing. */
        if (tea->n_tokens < 0) {
                const char * err = string_jsmnerr[-tea->n_tokens - 1];
                free(tea->tokens);
                free(tea);
                ROAR_ERRWP_FORMAT(handler, &jsmn_tea_create, EINVAL,
                    string_error_json[0], string_error_json[1], err);
                return NULL;
        }

        /* Finally let us free any extra memory. */
        if (tea->n_tokens < size)
                tea->tokens =
                    realloc(tea->tokens, tea->n_tokens * sizeof(*tea->tokens));

        /* Configure the new object and return its handle. */
        tea->api.handler = handler;
        tea->buffer_size = buffer_size + 1;
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

/* Library function for getting the raw representation of the current
 * JSMN token.
 */
enum jsmnerr jsmn_tea_token_raw(
    struct jsmn_tea * tea_, enum jsmn_tea_mode mode, char ** raw)
{
        UNPACK_TEA_AND_TOKEN;
        char c0 = 0x0, c1 = 0x0;
        if (token->type == JSMN_OBJECT)
                c0 = '{', c1 = '}';
        else if (token->type == JSMN_ARRAY)
                c0 = '[', c1 = ']';
        else if (token->type == JSMN_STRING)
                c0 = c1 = '"';

        int i0 = token->start;
        if (c0 != 0x0) {
                for (i0 = token->start - 1; i0 >= 0; i0--) {
                        if (tea->buffer[i0] == c0) break;
                }
        }
        char * s = tea->buffer + i0;

        int i1 = token->end;
        if (c1 != 0x0) {
                for (i1 = token->end; i1 < tea->buffer_size - 1; i1++) {
                        if (tea->buffer[i1] == c1) break;
                }
                i1++;
        }
        tea->buffer[i1] = 0x0;

        if (mode == JSMN_TEA_MODE_DUP) {
                int n = strlen(s) + 1;
                char * tmp = malloc(n * sizeof(char));
                if (tmp == NULL) {
                        return ROAR_ERRNO(
                            tea_->handler, &jsmn_tea_token_raw, 0);
                }
                memcpy(tmp, s, n * sizeof(char));
                s = tmp;
        }

        *raw = s;
        return JSMN_SUCCESS;
}

/* Library function for skipping the current token, recursively. */
static enum jsmnerr token_skip(struct tea_object * tea, int * index)
{
        if (*index >= tea->n_tokens) {
                return ROAR_ERRWP_FORMAT(&tea->api.handler,
                    &jsmn_tea_token_skip, JSMN_ERROR_INVAL,
                    string_error_json[0], string_error_json[1],
                    string_jsmnerr[-JSMN_ERROR_PART - 1]);
        }

        jsmntok_t * token = tea->tokens + *index;
        (*index)++;
        if ((token->type == JSMN_ARRAY) || (token->type == JSMN_OBJECT)) {
                jsmntok_t * t;
                int i;
                for (i = 0, t = token + 1; i < token->size; i++, t++) {
                        if (token->type == JSMN_OBJECT) {
                                (*index)++;
                                t++;
                        }
                        int rc = token_skip(tea, index);
                        if (rc != JSMN_SUCCESS) return rc;
                }
        }
        return JSMN_SUCCESS;
}

/* Library function for skipping the current token, recursively. */
enum jsmnerr jsmn_tea_token_skip(struct jsmn_tea * tea_)
{
        struct tea_object * tea = (struct tea_object *)tea_;
        int index = tea_->index;
        const int rc = token_skip(tea, &index);
        if (rc == JSMN_SUCCESS) tea_->index = index;
        return rc;
}

/* Get the next token in the JSON string as a compound type. */
static enum jsmnerr next_compound(struct jsmn_tea * tea_, jsmntype_t type,
    int * size, roar_function_t * referent)
{
        UNPACK_TEA_AND_TOKEN;
        if (token->type != type) {
                if (size != NULL) *size = 0;
                return ROAR_ERRWP_FORMAT(tea_->handler, referent,
                    JSMN_ERROR_INVAL, string_error_type[0],
                    string_error_type[1], string_jsmntype[token->type],
                    jsmn_tea_strtoken(tea_));
        }
        if (size != NULL) *size = token->size;
        tea->api.index++;
        return JSMN_SUCCESS;
}

/* Get the header of the next JSON object. */
enum jsmnerr jsmn_tea_next_object(struct jsmn_tea * tea_, int * size)
{
        return next_compound(
            tea_, JSMN_OBJECT, size, (roar_function_t *)&jsmn_tea_next_object);
}

/* Get the header of the next JSON array. */
enum jsmnerr jsmn_tea_next_array(struct jsmn_tea * tea_, int * size)
{
        return next_compound(
            tea_, JSMN_ARRAY, size, (roar_function_t *)&jsmn_tea_next_array);
}

/* Get the next JSON string. */
enum jsmnerr jsmn_tea_next_string(
    struct jsmn_tea * tea_, int key, char ** string)
{
        UNPACK_ALL;
        if (!key && (token->type == JSMN_PRIMITIVE) &&
            (strcmp(s, "null") == 0)) {
                if (string != NULL) *string = NULL;
                tea->api.index++;
                return JSMN_SUCCESS;
        }
        if (token->type != JSMN_STRING) {
                return ROAR_ERRWP_FORMAT(tea_->handler, &jsmn_tea_next_string,
                    JSMN_ERROR_INVAL, string_error_type[0],
                    string_error_type[1], string_jsmntype[token->type],
                    jsmn_tea_strtoken(tea_));
        }
        if (key && (token->size != 1)) {
                return ROAR_ERRWP_FORMAT(tea_->handler, &jsmn_tea_next_string,
                    JSMN_ERROR_INVAL, string_error_empty[0],
                    string_error_empty[1], s, jsmn_tea_strtoken(tea_));
        }
        if (string != NULL) *string = s;
        tea->api.index++;
        return JSMN_SUCCESS;
}

/* Setter type for integer like numbers. */
typedef void integer_setter_t(void * p, long l);

/* The destination address is a char. */
static void integer_to_char(void * p, long l) { *(char *)p = (char)l; }

/* The destination address is an unsigned char. */
static void integer_to_unsigned_char(void * p, long l)
{
        *(unsigned char *)p = (unsigned char)l;
}

/* The destination address is a short. */
static void integer_to_short(void * p, long l) { *(short *)p = (short)l; }

/* The destination address is an unsigned short. */
static void integer_to_unsigned_short(void * p, long l)
{
        *(unsigned short *)p = (unsigned short)l;
}

/* The destination address is an int. */
static void integer_to_int(void * p, long l) { *(int *)p = (int)l; }

/* The destination address is an unsigned int. */
static void integer_to_unsigned_int(void * p, long l)
{
        *(unsigned int *)p = (unsigned int)l;
}

/* The destination address is a long. */
static void integer_to_long(void * p, long l) { *(long *)p = l; }

/* The destination address is an unsigned long. */
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
                return ROAR_ERRWP_FORMAT(tea_->handler, &jsmn_tea_next_number,
                    JSMN_ERROR_INVAL, string_error_type[0],
                    string_error_type[1], string_jsmntype[token->type],
                    jsmn_tea_strtoken(tea_));
        }
        char * endptr;
        if (type < JSMN_TEA_TYPE_FLOAT) {
                const long l = strtol(s, &endptr, 10);
                if (*endptr != 0) {
                        return ROAR_ERRWP_FORMAT(tea_->handler,
                            &jsmn_tea_next_number, JSMN_ERROR_INVAL,
                            string_error_value[0], string_error_value[1], s,
                            jsmn_tea_strtoken(tea_));
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
                        return ROAR_ERRWP_FORMAT(tea_->handler,
                            &jsmn_tea_next_number, JSMN_ERROR_INVAL,
                            string_error_value[0], string_error_value[1], s,
                            jsmn_tea_strtoken(tea_));
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
                return ROAR_ERRWP_FORMAT(tea_->handler, &jsmn_tea_next_bool,
                    JSMN_ERROR_INVAL, string_error_type[0],
                    string_error_type[1], string_jsmntype[token->type],
                    jsmn_tea_strtoken(tea_));
        }
        if (strcmp(s, "true") == 0) {
                if (value != NULL) *value = 1;
        } else if (strcmp(s, "false") == 0) {
                if (value != NULL) *value = 0;
        } else {
                return ROAR_ERRWP_FORMAT(tea_->handler, &jsmn_tea_next_bool,
                    JSMN_ERROR_INVAL, string_error_value[0],
                    string_error_value[1], s, jsmn_tea_strtoken(tea_));
        }
        tea->api.index++;
        return JSMN_SUCCESS;
}

/* Get the next JSON null. */
enum jsmnerr jsmn_tea_next_null(struct jsmn_tea * tea_)
{
        UNPACK_ALL;
        if ((token->type != JSMN_PRIMITIVE) || (strcmp(s, "null") != 0)) {
                return ROAR_ERRWP_FORMAT(tea_->handler, &jsmn_tea_next_null,
                    JSMN_ERROR_INVAL, string_error_type[0],
                    string_error_type[1], string_jsmntype[token->type],
                    jsmn_tea_strtoken(tea_));
        }
        return JSMN_SUCCESS;
}

/* Library function for formating a JSMN token. */
const char * jsmn_tea_strtoken(struct jsmn_tea * tea_)
{
        struct tea_object * tea = (struct tea_object *)tea_;
        const int n = snprintf(tea->tag, TAG_SIZE, "#%d", tea_->index);
        return tea->strtoken;
}
