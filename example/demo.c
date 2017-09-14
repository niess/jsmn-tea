#include "jsmn-tea.h"
#include <stdio.h>
#include <stdlib.h>

/* Handle for the JSMN-TEA object. */
static struct jsmn_tea * tea = NULL;

/* Gracefully exit to the OS. */
void exit_gracefully(int rc)
{
        jsmn_tea_destroy(&tea);
        exit(rc);
}

/* Error handler for JSMN-TEA. */
void error_handler(void) { exit_gracefully(EXIT_FAILURE); }

int main()
{
        /* First let us create a new JSMN-TEA object from a JSON file. */
        tea = jsmn_tea_create(
            "example/demo.json", JSMN_TEA_MODE_LOAD, &error_handler, stderr);

        /* Then, let us require a JSON object as base token. */
        jsmn_tea_next_object(tea, NULL);

        /* The following would raise an error if not muted since JSON keys must
         * be strings.
         */
        jsmn_tea_error_disable(tea);
        int i;
        jsmn_tea_next_number(tea, JSMN_TEA_TYPE_INT, &i);
        jsmn_tea_error_enable(tea);

        /* Let us now parse the content of the first cell requiring a double
         * for the value.
         */
        char * key;
        double value;
        jsmn_tea_next_string(tea, &key);
        jsmn_tea_next_number(tea, JSMN_TEA_TYPE_DOUBLE, &value);
        printf("{ %s : %g }\n", key, value);

        /* Let us trigger some error. */
        jsmn_tea_error_raise(tea, JSMN_ERROR_INVAL, "invalid key `%s`\n", key);

        exit_gracefully(EXIT_SUCCESS);
}
