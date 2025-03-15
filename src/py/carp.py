'''
Made with version: Python 3.11.2
'''

import json
import sys
from shutil import which
import os
from os.path import dirname, realpath, join

CARP_JSON_OPTION_SCHEMA = {
    "arguments": { "type": int, "default": "false", "required": True, "clean": None },
    "callback": { "type": str, "default": "null", "required": True, "clean": None }
}

def exit_with_error(msg, status_code=1):
    print("[carp] " + msg, file=sys.stderr)
    exit(status_code)

def carp_read_json(carp_file):
    with open(carp_file, "r") as f:
        j = json.load(f)
    return j

def carp_json_option_validate(option):
    '''
    Validate an 'option' object from the json file.
    An option is valid if:
    - Each key in 'option' exists in the schema
    - Each field has the correct type (as specified by the schema)
    - Each required field exists
    This function will exit if an invalid field/value is found.
    If any optional fields are missing, this function will fill them in with defaults.

    Parameters
    ----------
    option : option
        A dictionary corresponding to an 'option' object in the json

    Returns
    -------
    options_clean
        A list of dictionaries. Each dictionary contains the "clean" option data.
        There can be at most two dictionaries in the list, one for 'short' and one for 'long'.
    '''
    option_keys = set(option.keys())

    # Sort the results so the keys are always in predictable order
    # i.e.: option_flags_specified := ['long', 'short'] (if both flags are provided)
    option_flags_specified = sorted(option_keys.intersection({ "short", "long" }))
    if len(option_flags_specified) == 0:
        exit_with_error("option is missing json field 'short' or 'long'")

    # Grab the name of the option for debugging messages
    if "short" in option_flags_specified:
        option_name_debug = option.get("short")
    else:
        option_name_debug = option.get("long")

    options_clean = []
    for i, k in enumerate(option_flags_specified):
        if not isinstance(option[k], str):
            exit_with_error("option '{}': json field '{}' must be of type str ('{}: {}')".format(option_name_debug, k, k, option[k]))
        options_clean.append( { k: option.pop(k).strip("-") } )
        option_keys.remove(k)
        if not options_clean[i][k]:
            exit_with_error("option '{}': empty string value ('{}: {}')".format(option_name_debug, k, options_clean[i][k]))
        if k == "short" and len(options_clean[i][k]) != 1:
            exit_with_error("option '{}': short option must be a single character ('{}: {}')".format(option_name_debug, k, options_clean[i][k]))

    if len(options_clean) == 2 and (options_clean[0]["long"] == options_clean[1]["short"]):
        exit_with_error("'short' and 'long' option have the same value: '{}'".format(options_clean[0]["long"]))

    # The schema tells us the valid fields/types for the option spec
    schema = CARP_JSON_OPTION_SCHEMA

    # Error if a key is present in 'option' and not in the schema
    if not option_keys.issubset(schema):
        exit_with_error("option '{}': the following json fields are not recognized: {}".format(option_name_debug, str(option_keys.difference(schema))))

    # For each key in the schema:
    # - if the key is not present in 'option' and is required: error
    # - if the key is not present in 'option' and is not required: use default
    # - if the key is present in 'option':
    #   - if the value is not of the proper type: error
    #   - invoke appropriate function to cleanse value
    for k, v in schema.items():
        if k not in option:
            if schema[k]["required"]:
                exit_with_error("option '{}': missing required field '{}'".format(option_name_debug, k))
            else:
                option[k] = schema[k]["default"]
        else:
            if not isinstance(option[k], schema[k]["type"]):
                exit_with_error("option '{}': field '{}' must be of type {}".format(option_name_debug, k, schema[k]["type"]))
            if schema[k]["clean"] is not None:
                option[k] = schema[k]["clean"](option[k])

    for i in range(len(options_clean)):
        options_clean[i] |= option
    return options_clean

carp_table = []
def carp_table_add_option(option, spec):
    '''
    Adds a new option to the carp table.
    Exit if the provided option already exists.

    Parameters
    ----------
    option : str
        The name of the option
    spec : dict
        A dictionary containing metadata about the option.
        Valid fields are listed in 'CARP_JSON_OPTION_SCHEMA'
    '''
    for v in carp_table:
        if v["name"] == option:
            exit_with_error("option '{}' specified more than once".format(option))
    carp_table.append({"name": option} | spec)

def carp_gperf_generate_hash(carp_table, output_dir):
    '''
    Attempt to generate a perfect hash function implementation in C using gperf.
    Exit if gperf is unable to generate a hash function.

    Parameters
    ----------
    carp_table : list
        A list of dictionaries, where each element is an option
    output_dir : str
        The absolute path to directory where the output files will be placed
    '''
    gperf_input_abs_path = realpath(join(output_dir, "gperf_input.txt"))
    gperf_output_abs_path = realpath(join(output_dir, "carp_hash.c"))
    max_option_name_len = len(max([v["name"] for v in carp_table], key=len))
    with open(gperf_input_abs_path, "w") as f:
        f.write("%{\n")
        f.write("#include \"carp_backend.h\"\n")
        f.write("#include <stdbool.h>\n")
        f.write("#include <string.h>\n")
        f.write("struct CarpOption* in_word_set(register const char *str, register size_t len);\n")

        callbacks = set([v["callback"] for v in carp_table])
        for v in callbacks:
            f.write("extern void {}(void*, const char**, int);\n".format(v))

        f.write("struct CarpOptionSpec* carp_hash(const char* name, int len) {\n")
        f.write("#define CARP_KEY_NAME_SIZE ({} + 1)\n".format(max_option_name_len))
        f.write("\tchar key_name[CARP_KEY_NAME_SIZE];\n")
        f.write("\t(void)strncpy(key_name, name, len);\n")
        f.write("\tkey_name[len] = '\\0';\n")
        f.write("\tstruct CarpOption* opt = in_word_set(key_name, len);\n\n")

        f.write("\tif (opt) return &opt->spec;\n")
        f.write("\telse return NULL;\n")
        f.write("}\n")
        f.write("%}\n")

        f.write("struct CarpOption;\n")
        f.write("%%\n")
        for v in carp_table:
            f.write("{}, {{ {}, {} }}\n".format(v["name"], v["arguments"], v["callback"]))

    gperf_command = "{} -t --output-file={} {}".format(which("gperf"), gperf_output_abs_path, gperf_input_abs_path)
    if os.system(gperf_command):
        exit_with_error("error executing gperf command: {}".format(gperf_command))

def carp_generate_search(carp_table, output_dir):
    def sort_carp_table(carp_table_unsorted):
        names_unsorted = [ v["name"] for v in carp_table_unsorted ]
        names_sorted = sorted(names_unsorted)
        carp_table_sorted = []
        for n in names_sorted:
            idx = names_unsorted.index(n)
            carp_table_sorted.append(carp_table_unsorted[idx])
        return carp_table_sorted

    ct_sorted = sort_carp_table(carp_table)
    search_output_abs_path = realpath(join(output_dir, "carp_search.c"))
    max_option_name_len = len(max([v["name"] for v in ct_sorted], key=len))
    with open(search_output_abs_path, "w") as f:
        f.write("#include \"carp_backend.h\"\n")
        f.write("#include <string.h>\n")
        f.write("#include <stdlib.h>\n\n")

        callbacks = set([v["callback"] for v in ct_sorted])
        for v in callbacks:
            f.write("extern void {}(void*, const char**, int);\n".format(v))
        f.write("\n")

        f.write("int compare_options(const void* lhs, const void* rhs) {\n")
        f.write("\treturn strcmp(((struct CarpOption*)lhs)->name, ((struct CarpOption*)rhs)->name);\n")
        f.write("}\n\n")

        f.write("struct CarpOptionSpec* carp_search(const char* name, int len) {\n")
        f.write("\tstatic struct CarpOption opts[{}] = {{\n".format(len(ct_sorted)))
        for i, v in enumerate(ct_sorted):
            f.write("\t\t[{}] = {{\n".format(i))
            f.write("\t\t\t.name = \"{}\",\n".format(ct_sorted[i]["name"]))
            f.write("\t\t\t.spec = {\n")
            f.write("\t\t\t\t.arguments = {},\n".format(ct_sorted[i]["arguments"]))
            f.write("\t\t\t\t.callback = {}\n".format(ct_sorted[i]["callback"]))
            f.write("\t\t\t}\n")
            f.write("\t\t},\n")
        f.write("\t};\n\n")

        f.write("#define CARP_KEY_NAME_SIZE ({} + 1)\n".format(max_option_name_len))
        f.write("\tchar key_name[CARP_KEY_NAME_SIZE];\n")
        f.write("\t(void)strncpy(key_name, name, len);\n")
        f.write("\tkey_name[len] = '\\0';\n")
        f.write("\tstruct CarpOption key = { .name = key_name };\n\n")

        f.write("\tvoid* result = bsearch(&key, opts, sizeof(opts) / sizeof(opts[0]), sizeof(opts[0]), compare_options);\n")
        f.write("\tif (result) return &((struct CarpOption*)result)->spec;\n")
        f.write("\telse return NULL;\n")
        f.write("}\n")

###
#  Start of script
###

def main():
    # Validate command line arguments
    CARP_IMPLEMENTATION = sys.argv[1].lower() if sys.argv[1:] else ""
    if len(sys.argv) != 4 or not {CARP_IMPLEMENTATION}.issubset({"hash", "search"}):
        exit_with_error("usage: ./carp.py <hash | search> <output_dir> <carp.json>")

    if (CARP_IMPLEMENTATION == "hash") and (not which("gperf")):
        exit_with_error("cannot find 'gperf' executable")

    if not os.path.exists(sys.argv[2]):
        exit_with_error("cannot find output directory {}".format(sys.argv[2]))

    if not os.path.exists(sys.argv[3]):
        exit_with_error("cannot find input json file {}".format(sys.argv[3]))

    CARP_OUTPUT_DIR = sys.argv[2]
    CARP_JSON_FILE = sys.argv[3]

    carp_json = carp_read_json(CARP_JSON_FILE)

    for option in carp_json["options"]:
        opt = option.copy()
        clean = carp_json_option_validate(opt)
        for v in clean:
            if "short" in v.keys():
                name = v.pop("short")
            else:
                name = v.pop("long")
            carp_table_add_option(name, v)

    if CARP_IMPLEMENTATION == "hash":
        carp_gperf_generate_hash(carp_table, CARP_OUTPUT_DIR)
    else:
        carp_generate_search(carp_table, CARP_OUTPUT_DIR)

if __name__ == '__main__':
    main()

