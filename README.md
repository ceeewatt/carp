# Overview

Carp is a command line parser for C which matches command line options against a build-time generated table of acceptable option flags. Provide a high-level description of your program's command line options, and for each option, carp will parse that option's arguments (if applicable) and pass them to a callback function that you define.

Carp supports the traditional GNU-style "short" and "long" options (e.g.: `-a`, `-abc`, `--long`, `--long=<arg>`).

At runtime, carp will index into the build-time generated table of options that you specify through a JSON file. You have two option as to how this table is implemented. By default, this table is created as sorted array that will be searched via a binary search. Alternatively, this table can be created as a hash table, in which case, the dependency GNU gperf must be installed on your system. You can specify either of these implementations by setting the CMake variable `CARP_IMPLEMENTATION` to either "search" (default) or "hash".

# Dependencies

- Python3: for generating the table of options
- GNU gperf: if you optionally want the generated table to be implemented as a hash table

# Usage

Start by creating a JSON file defining each option flag that your program accepts. See the example JSON file for the valid format/fields.

Each option JSON object is placed in the `options` array formatted like so:

```json
"options": [
    {
        "short": "x",
        "arguments": 0,
        "callback": "x_callback"
    },
    {
        "long": "this-is-a-long-option",
        "arguments": 1,
        "callback": "long_callback"
    },
    {
        "short": "f",
        "long": "file",
        "arguments": -1,
        "callback": "f_callback"
    }
]
```

Each option object must contain at least three field: `short`/`long`, `arguments`, and `callback`. If both the short and long option are defined for a single option, then that option can be specified on the command line with either the short option flag or the long option flag (e.g.: `-f` == `--file`).

The arguments field determines how many command line arguments this option expects. If an option can accept any number of arguments, give this field a value of -1.

The callback field must contain the name of a function which carp will invoke when it encounters your option. The signature for this callback function should take the following form: `void my_callback(void* param, const char** buf, int len)`. Notice that there are three arguments:
- A parameter of your choosing. You'll pass this parameter to the top-level `carp_parse()` function, and carp will in turn pass it back to your callback function.
- A buffer containing the arguments to your option. If your option doesn't accept arguments, this will be NULL.
- The length of the buffer / number of arguments. If your option doesn't accept argument, this will be zero.

Once your JSON file is created, the easiest way to get carp built with your project is to add this repository as a subdirectory.

To link the carp static library with your project, add the following to your top-level CMakeLists.txt:

```cmake
set(CARP_JSON_FILE <path-to-json>)
set(CARP_IMPLEMENTATION <hash | search>)

add_subdirectory(carp)
target_link_libraries(<yourproject> carp)
```

The CMake variables `CARP_JSON_FILE` and `CARP_IMPLEMENTATION` are used to select the input JSON file and the implementation carp choose (either "hash" or "search").

To invoke carp from your project, call the `carp_parse()` function:

```c
#include "carp.h"

int main(int argc, char* argv[]) {
    struct CallbackParam cb_param;
    struct Carp carp;
    carp_parse(&carp, argc, argv, &cb_param);

    /* do stuff ... */

    carp_clean(&carp);
}
```

A few things to note:
- The non-option arguments (that is, the command line arguments that don't belong to any particular option) are placed in a buffer, accessible through the `struct Carp` (carp.argv and carp.argc).
- The non-option arguments are placed in dynamic memory, so it's necessary to call `carp_cleanup()` when you're done using this buffer. If you don't care about the non-option arguments, you can pass in NULL for the first argument to `carp_parse()`.

# TODO

- [ ] Automatic generation of `--help` messages.
- [ ] Add support for separate sets of options per subcommand, similar to how each git command (`git status`, `git checkout`, etc) has its own set of options.

# Unit tests

To enable building the unit test executable, define the CMake variable `CARP_ENABLE_TESTING` somewhere in your configuration (e.g.: `cmake -S . -B build -DCARP_ENABLE_TESTING=1`).
