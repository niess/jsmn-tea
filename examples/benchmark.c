#include "jsmn-tea.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The number of times that each bechmark is run. */
#define N_TRIALS 10000

/* Flags to enable or disable specific sections. */
#define USE_JSMN 1
#define USE_JSMN_TEA 1

int main()
{
        /* First let us build a JSON array for the benchmarking. */
        char json[65536];
        const int n = sizeof(json) / (14 * sizeof(*json)) - 1;
        sprintf(json, "[ %12.5E", (double)rand());
        int i;
        char * s;
        for (i = 0, s = json + 14; i < n - 1; i++, s += 14)
                sprintf(s, ", %12.5E", (double)rand());
        sprintf(s, " ]");

#if (USE_JSMN)
        /* Let us benchmark the direct parsing with JSMN. */
        for (i = 0; i < N_TRIALS; i++) {
                /* Parse the tokens. */
                jsmn_parser parser;
                jsmntok_t tokens[n + 1];
                jsmn_init(&parser);
                const int rc =
                    jsmn_parse(&parser, json, strlen(json), tokens, n + 1);

                /* Parse the numeric values. */
                int j;
                jsmntok_t * token;
                for (j = 1, token = tokens + 1; j < n + 1; j++, token++) {
                        char c = json[token->end];
                        json[token->end] = 0x0;
                        char * endptr;
                        const double tmp = strtod(json + token->start, &endptr);
                        if (*endptr != 0) exit(EXIT_FAILURE);
                        json[token->end] = c;
                }
        }
#endif /* USE_JSMN */

#if (USE_JSMN_TEA)
        /* Let us benchmark the parsing with JSMN-TEA. */
        for (i = 0; i < N_TRIALS; i++) {
                struct jsmn_tea_handler handler = { stderr, NULL };
                struct jsmn_tea * tea =
                    jsmn_tea_create(json, JSMN_TEA_MODE_DUP, &handler);
                int size, rc;
                rc = jsmn_tea_next_array(tea, &size);
                if (rc != JSMN_SUCCESS) exit(rc);
                int j;
                for (j = 0; j < size; j++) {
                        double value;
                        rc = jsmn_tea_next_number(
                            tea, JSMN_TEA_TYPE_DOUBLE, &value);
                        if (rc != JSMN_SUCCESS) exit(rc);
                }
                jsmn_tea_destroy(&tea);
        }
#endif /* USE_JSMN_TEA */

        exit(EXIT_SUCCESS);
}
