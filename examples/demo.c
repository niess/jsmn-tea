#include "jsmn-tea.h"
#include <stdio.h>
#include <stdlib.h>

/* Handle for the JSMN-TEA object. */
static struct jsmn_tea * tea = NULL;

/* Gracefully exit to the OS. */
static void exit_gracefully(int rc)
{
        jsmn_tea_destroy(&tea);
        exit(rc);
}

/* Error handler for JSMN-TEA. */
static int error_handler(
    struct roar_handler * handler, roar_function_t * referent, int code)
{
        exit_gracefully(EXIT_FAILURE);
}

int main()
{
        /* First let us create a new JSMN-TEA object from a JSON file. */
        struct roar_handler handler = { stderr, NULL, NULL, &error_handler };
        tea =
            jsmn_tea_create("examples/demo.json", JSMN_TEA_MODE_LOAD, &handler);

        /* Then, let us require a JSON object as base token. */
        jsmn_tea_next_object(tea, NULL);

        /* Let us now parse the content of the first cell requiring a double
         * for the value.
         */
        char * key;
        double value;
        jsmn_tea_next_string(tea, 1, &key);
        jsmn_tea_next_number(tea, JSMN_TEA_TYPE_DOUBLE, &value);
        printf("{ %s : %g }\n", key, value);

        /* Let us trigger some custom error with ROAR. */
        ROAR_ERRWP_FORMAT(&handler, &main, JSMN_ERROR_INVAL, "Invalid key",
            "\"%s\", %s", key, jsmn_tea_strtoken(tea));

        exit_gracefully(EXIT_SUCCESS);
}
