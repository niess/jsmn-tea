# JSMN-TEA
( **JSMN** with some more **T**ext **E**ncapsulation and **A**nalysis )

_JSMN-TEA is a C99 extension to [JSMN][JSMN] providing higher level utilities
for parsing simple and small to medium size JSON files, e.g. configuration data.
Some speed and efficiency is traded for ease of use, while still keeping a
rather small code footprint, i.e ~500 additional LOC._

## Installation

Currently there is no automatic build procedure. On linux you can try the
provided [Makefile](Makefile) as:
```bash
make && cp -r lib include /path/to/install
```
Note that you'll need to clone both this repository and its [JSMN][JSMN]
submodule, e.g. using `git clone --recursive`.

The required files for building the library are `jsmn.h`, `jsmn.c`,
[jsmn-tea.c](src/jsmn-tea.c) and [jsmn-tea.h](include/jsmn-tea.h). Note also
that both header files are needed for the compilation and for a proper
installation.

## API description

A detailed description of the API is given in the
[jsmn-tea.h](include/jsmn-tea.h) header file. Simple examples exposing some
of the key functionalities can be found in the [examples](examples) folder. In
the following we however follow a more narrative description. Note that while
knowledge of JSMN is required for fully exploiting the JSMN-TEA extension, it
is not necessary for a basic usage.

### The `jsmn_tea` structure

JSMN-TEA extends JSMN with a semi-opaque `jsmn-tea` structure that can be
casted to a `jsmn_parser`. The `jsmn-tea` object exposes some mutable
data and serves as a proxy for an opaque memory handle, e.g. for loading
the JSON text from a file into memory. The `jsmn_tea` object must be created and
destroyed using the following API functions:
```c
/* Use this, e.g. to load a JSON file to memory and to analyse its content. */
struct jsmn_tea * jsmn_tea_create(char * arg, enum jsmn_tea_mode mode,
    jsmn_tea_cb * error_handler, FILE * error_stream);

/* Always destroy with this for proper memory usage. */
void jsmn_tea_destroy(struct jsmn_tea ** tea);
```

Let us point out that the `jsmn_tea_create` function has three run modes:
`JSMN_TEA_MODE_LOAD`, `JSMN_TEA_MODE_DUP` and `JSMN_TEA_MODE_RAW`. In the
first mode `arg` specifies a JSON file to load. In the others mode `arg` is
a string containing the JSON data. Note that the library functions modify
the string content after it has been analysed with JSMN, e.g. introducing
`0x0` chars for `C` strings termination. So if this is an issue
use `JSMN_TEA_MODE_DUP` in order to work with a copy. In the first mode,
the file content is never altered though.

In addition, the `jsmn_tea_create` function takes two more arguments allowing
for some automated error handling. More detail on this is provided
hereafter.

### The `next` functions

The main purpose of the JSMN-TEA library is to parse simple JSON configuration
files whose content is rather predictable. That for, it provides a set of
iterator like functions, labeled as `jsmn_tea_next_*`, where `*` stands for one
of the height JSON data types, as follow:

```c
/* Use this if the next JSON item is expected to be an object. At return
 * *size* is filled with the number of sub items, i.e. { key : value } pairs.
 * Note that the number of tokens is two times the number of pairs.
 */
enum jsmnerr jsmn_tea_next_object(struct jsmn_tea * tea, int * size);

/* Use this if the next JSON item is expected to be an array. At return
 * *size* is filled with the number of array elements.
 */
enum jsmnerr jsmn_tea_next_array(struct jsmn_tea * tea, int * size);

/* Use this if the next JSON item is expected to be a string. */
enum jsmnerr jsmn_tea_next_string(
    struct jsmn_tea * tea, int key, char ** string);

/* Use this if the next JSON item is expected to be a numeric value. Note
 * that the type of the storage C variable has to be explicitly specified.
 */
enum jsmnerr jsmn_tea_next_number(
    struct jsmn_tea * tea, enum jsmn_tea_type type, void * value);

/* Use this if the next JSON item is expected to be a boolean. */
enum jsmnerr jsmn_tea_next_bool(struct jsmn_tea * tea, int * value);

/* Use this if the next JSON item is expected to be null. */
enum jsmnerr jsmn_tea_next_null(struct jsmn_tea * tea);
```

These functions provide a direct linkage between an expected JSON item and a C
variable. Let us point out that they all return `JSMN_SUCCESS`, i.e. `0` in case
of success or a `jsmnerr` enum value otherwise.

### Error handling

Whenever a library error occurs the `jsmn_tea::error_handler` is triggered, if
not `NULL`. In addition a short descriptive error message is dumped to
`jsmn_tea::error_stream`, if not `NULL`. The format of the error message is as
follow:
```c
"file.c (line-number): [file.json #token-index] {description}\n"
```
where the two first elements specify the C file and the line where the error
was triggered. The two following one are optional. They specify the faulty JSON
file and the JSMN token index, in case of a parsing error.

Note that despite the error handler and the output stream are provided at
the creation of the `jsmn_tea` object, they can be modified afterwards, on
the flight. For convenience, the JSMN-TEA library also provides two more
functions for masking or unmasking them both, as:
```c
/* Call this to disable errors handling and dumping. */
void jsmn_tea_error_disable(struct jsmn_tea * tea);

/* Call this to enable errors handling and dumping again. */
void jsmn_tea_error_enable(struct jsmn_tea * tea);
```
Let us point out that the library function will still return a `jsmnerr` enum
value when errors are *disabled*.

In addition, one can also trigger custom parsing errors as:
```c
/* Use this to trigger your own error, using a printf format. */
enum jsmnerr jsmn_tea_error_raise(
    struct jsmn_tea * tea, enum jsmnerr rc, const char * fmt, ...);
```
This can be useful, for example if an unsupported key name, or numeric value
is found in the JSON file. Note also that the current C file and JSON file
information is prepended to the generated error message.

### Inspecting JSMN tokens

At the creation of a `jsmn_tea` object the whole content of the JSON data is
analysed with JSMN and saved in memory. Information and manipulation of the corresponding JSMN tokens is provided by the following functions:
```c
/* Use this to get the current JSMN token, i.e. at jsmn_tea::index. */
jsmntok_t * jsmn_tea_token_get(struct jsmn_tea * tea);

/* Use this to get the total number of JSMN tokens. */
int jsmn_tea_token_length(struct jsmn_tea * tea);

/* Use this to get the raw JSON representation of the current JSMN token or
 * a copy of it.
 */
enum jsmnerr jsmn_tea_token_raw(
      struct jsmn_tea * tea, enum jsmn_tea_mode mode, char ** raw);

/* Use this to skip the current token, recursively. */
enum jsmnerr jsmn_tea_token_skip(struct jsmn_tea * tea);
```

## License
The JSMN-TEA library is distributed under the **MIT** license. See the provided
[LICENSE](LICENSE) file.

[JSMN]: https://github.com/zserge/jsmn
