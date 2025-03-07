#include <iostream>
#include <string>
#include <vector>
#include <catch2/catch.hpp>

enum CarpTokenType {
    TOKEN_SHORT_OPTION = 0,
    TOKEN_LONG_OPTION,
    TOKEN_SEPARATOR,
    TOKEN_ARGUMENT
};

struct CarpPrivate;

// Bypass the carp backend so callback invocations are directed to this translation unit
typedef void (*CARP_CALLBACK)(void*, const char**, int);
struct CarpOptionSpec {
    int arguments;
    CARP_CALLBACK callback;
};
struct CarpTable {
    std::string option;
    CarpOptionSpec spec;
};
std::vector<CarpTable> g_table;
int g_callback_retval = 0;

extern "C" {
    #include "carp_argument_vector.h"
    #include "carp.h"
    extern enum CarpTokenType carp_classify_token(const char* token);
    extern int carp_option_argument_handler(struct CarpPrivate* c, int required_arguments, const char* immediate);
    extern void carp_parse_short_option(struct CarpPrivate* c);
    extern void carp_parse_long_option(struct CarpPrivate* c);
    void carp_callback_override(void* param, const char** buf, int len)
    {
        (void)param; (void)buf;
        g_callback_retval = len;
    }
    struct CarpOptionSpec* carp_backend_search(const char* name, int len)
    {
        std::string opt{ name, static_cast<std::string::size_type>(len) };

        for (CarpTable& t : g_table) {
            if (opt.compare(t.option) == 0) {
                return &t.spec;
            }
        }
        return NULL;
    }
}

struct CarpPrivateState {
    CarpPrivateState(int argc, const char** argv)
        : tail{ argc }
    {
        if (argc > 1) {
            head = 1;
            token = argv[1];
        }
        else {
            head = 0;
            token = nullptr;
        }
    }

    int head;
    const int tail;
    const char* token;
};

struct CarpPrivate {
    CarpPrivate(int argc, const char** argv, CarpArgumentVector* callback_args, CarpArgumentVector* command_args)
        : state(argc, argv)
        , argv{ argv }
        , callback_args{ callback_args }
        , command_args{ command_args }
        , callback_param{ NULL }
    {
        carp_vector_init(callback_args, 25);
        carp_vector_init(command_args, 25);
    }

    ~CarpPrivate()
    {
        carp_vector_cleanup(callback_args);
        carp_vector_cleanup(command_args);
    }

    const char** argv;

    CarpArgumentVector* callback_args;
    CarpArgumentVector* command_args;

    void* callback_param;

    CarpPrivateState state;
};

// See: https://github.com/catchorg/Catch2/issues/1813
[[noreturn]] void exit(int status) {
    throw "exit";
}

TEST_CASE("test carp_classify_token()", "[.]") {
    REQUIRE(carp_classify_token("-a") == TOKEN_SHORT_OPTION);
    REQUIRE(carp_classify_token("-abc") == TOKEN_SHORT_OPTION);
    REQUIRE(carp_classify_token("--long") == TOKEN_LONG_OPTION);
    REQUIRE(carp_classify_token("--long=argument") == TOKEN_LONG_OPTION);
    REQUIRE(carp_classify_token("--") == TOKEN_SEPARATOR);
    REQUIRE(carp_classify_token("some_argument") == TOKEN_ARGUMENT);
}

TEST_CASE("test carp_option_argument_handler() no errors", "[.]") {
    const char* argv[] = {
        "./a.out",
        "-ainput.txt",
        "-binput1",
        "input2",
        "-o",
        "file1.out",
        "file2.out",
        "file3.out",
        "--long=argument"
    };
    int argc = sizeof(argv) / sizeof(argv[0]);
    CarpArgumentVector callback_args, command_args;

    CarpPrivate c(argc, argv, &callback_args, &command_args);

    SECTION("single immediate argument") {
        // -ainput.txt
        c.state.head = 1;
        const char* opt = c.argv[c.state.head];
        int head_increment = carp_option_argument_handler(&c, 1, (opt + 2));

        REQUIRE(head_increment == 1);
        REQUIRE(c.callback_args->size == 1);
        REQUIRE(std::string(c.callback_args->buf[0]) == "input.txt");
    }
    SECTION("single immediate argument followed by another required argument") {
        // -binput1 input2
        c.state.head = 2;
        const char* opt = c.argv[c.state.head];
        int head_increment = carp_option_argument_handler(&c, 2, (opt + 2));

        REQUIRE(head_increment == 2);
        REQUIRE(c.callback_args->size == 2);
        REQUIRE(std::string(c.callback_args->buf[0]) == "input1");
        REQUIRE(std::string(c.callback_args->buf[1]) == "input2");
    }
    SECTION("unknown number of arguments") {
        // -o file1.out file2.out file3.out
        c.state.head = 4;
        const char* opt = c.argv[c.state.head];
        int head_increment = carp_option_argument_handler(&c, -1, NULL);

        REQUIRE(head_increment == 4);
        REQUIRE(c.callback_args->size == 3);
        REQUIRE(std::string(c.callback_args->buf[0]) == "file1.out");
        REQUIRE(std::string(c.callback_args->buf[1]) == "file2.out");
        REQUIRE(std::string(c.callback_args->buf[2]) == "file3.out");
    }
    SECTION("long option single immediate argument") {
        // --long=argument
        c.state.head = 8;
        const char* opt = c.argv[c.state.head];
        int head_increment = carp_option_argument_handler(&c, 1, (opt + 7));

        REQUIRE(head_increment == 1);
        REQUIRE(c.callback_args->size == 1);
        REQUIRE(std::string(c.callback_args->buf[0]) == "argument");
    }
}

TEST_CASE("test carp_option_argument_handler() with errors", "[.]") {
    const char* argv[] = {
        "./a.out",
        "--long=argument"
    };
    int argc = sizeof(argv) / sizeof(argv[0]);
    CarpArgumentVector callback_args, command_args;

    CarpPrivate c(argc, argv, &callback_args, &command_args);

    SECTION("not enough arguments error") {
        // --long=argument
        c.state.head = 1;
        const char* opt = c.argv[c.state.head];

        REQUIRE_THROWS_WITH(
            (void)carp_option_argument_handler(&c, 2, (opt + 7)),
            "exit");
    }
}


TEST_CASE("test carp_parse_short_option()", "[.]") {
    const char* argv[] = {
        "./a.out",
        "-v",
        "-afile1",
        "file2",
        "file3",
        "-b",
        "out1",
        "out2",
        "--long",
        "-xfv",
        "argument",
        "-z"
    };
    int argc = sizeof(argv) / sizeof(argv[0]);
    CarpArgumentVector callback_args, command_args;
    CarpPrivate c(argc, argv, &callback_args, &command_args);

    SECTION("option wwth no arguments") {
        // -v
        c.state.head = 1;
        c.state.token = c.argv[c.state.head];
        g_table.push_back(CarpTable{ "v", CarpOptionSpec{ 0, carp_callback_override }});
        carp_parse_short_option(&c);

        REQUIRE(g_callback_retval == 0);

        g_table.clear();
    }
    SECTION("option with a determinate number of required arguments") {
        // -afile1 file2
        c.state.head = 2;
        c.state.token = c.argv[c.state.head];
        g_table.push_back(CarpTable{ "a", CarpOptionSpec{ 3, carp_callback_override }});
        carp_parse_short_option(&c);

        REQUIRE(g_callback_retval == 3);
        REQUIRE(std::string(c.callback_args->buf[0]) == "file1");
        REQUIRE(std::string(c.callback_args->buf[1]) == "file2");
        REQUIRE(std::string(c.callback_args->buf[2]) == "file3");

        g_table.clear();
    }
    SECTION("option with an indeterminate number of required arguments") {
        // -b out1 out2
        c.state.head = 5;
        c.state.token = c.argv[c.state.head];
        g_table.push_back(CarpTable{ "b", CarpOptionSpec{ -1, carp_callback_override }});
        carp_parse_short_option(&c);

        REQUIRE(g_callback_retval == 2);
        REQUIRE(std::string(c.callback_args->buf[0]) == "out1");
        REQUIRE(std::string(c.callback_args->buf[1]) == "out2");
        REQUIRE(c.state.head == 8);

        g_table.clear();
    }
    SECTION("short option grouping") {
        // -xfv argument
        c.state.head = 9;
        c.state.token = c.argv[c.state.head];
        g_table.push_back(CarpTable{ "x", CarpOptionSpec{ 0, carp_callback_override }});
        g_table.push_back(CarpTable{ "f", CarpOptionSpec{ 2, carp_callback_override }});
        carp_parse_short_option(&c);

        REQUIRE(g_callback_retval == 2);
        REQUIRE(std::string(c.callback_args->buf[0]) == "v");
        REQUIRE(std::string(c.callback_args->buf[1]) == "argument");
        REQUIRE(c.state.head == 11);

        g_table.clear();
    }
    SECTION("unknown option") {
        // -z
        c.state.head = 11;
        c.state.token = c.argv[c.state.head];

        REQUIRE_THROWS_WITH(carp_parse_short_option(&c), "exit");
    }
}

TEST_CASE("test carp_parse_long_option()", "[.]") {
    const char* argv[] = {
        "./a.out",
        "--long=argument",
        "--long",
        "arg1",
        "arg2",
        "arg3",
        "--long="
    };
    int argc = sizeof(argv) / sizeof(argv[0]);
    CarpArgumentVector callback_args, command_args;
    CarpPrivate c(argc, argv, &callback_args, &command_args);

    SECTION("long option with immediate argument") {
        // --long=argument
        c.state.head = 1;
        c.state.token = c.argv[c.state.head];
        g_table.push_back(CarpTable{ "long", CarpOptionSpec{ 1, carp_callback_override }});
        carp_parse_long_option(&c);

        REQUIRE(g_callback_retval == 1);
        REQUIRE(std::string(c.callback_args->buf[0]) == "argument");
        REQUIRE(c.state.head == 2);

        g_table.clear();
    }
    SECTION("long option with no required arguments") {
        // --long
        c.state.head = 2;
        c.state.token = c.argv[c.state.head];
        g_table.push_back(CarpTable{ "long", CarpOptionSpec{ 0, carp_callback_override }});
        carp_parse_long_option(&c);

        REQUIRE(g_callback_retval == 0);
        REQUIRE(c.state.head == 3);

        g_table.clear();
    }
    SECTION("long option with 3 required arguments") {
        // --long arg1 arg2 arg3
        c.state.head = 2;
        c.state.token = c.argv[c.state.head];
        g_table.push_back(CarpTable{ "long", CarpOptionSpec{ 3, carp_callback_override }});
        carp_parse_long_option(&c);

        REQUIRE(g_callback_retval == 3);
        REQUIRE(std::string(c.callback_args->buf[0]) == "arg1");
        REQUIRE(std::string(c.callback_args->buf[1]) == "arg2");
        REQUIRE(std::string(c.callback_args->buf[2]) == "arg3");
        REQUIRE(c.state.head == 6);

        g_table.clear();
    }
    SECTION("long option with an indeterminate number of required arguments") {
        // --long arg1 arg2 arg3
        c.state.head = 2;
        c.state.token = c.argv[c.state.head];
        g_table.push_back(CarpTable{ "long", CarpOptionSpec{ -1, carp_callback_override }});
        carp_parse_long_option(&c);

        REQUIRE(g_callback_retval == 3);
        REQUIRE(std::string(c.callback_args->buf[0]) == "arg1");
        REQUIRE(std::string(c.callback_args->buf[1]) == "arg2");
        REQUIRE(std::string(c.callback_args->buf[2]) == "arg3");
        REQUIRE(c.state.head == 6);

        g_table.clear();
    }
    SECTION("unknown option error") {
        // --long
        c.state.head = 2;
        c.state.token = c.argv[c.state.head];

        REQUIRE_THROWS_WITH(carp_parse_long_option(&c), "exit");
    }
    SECTION("unknown option error (with immediate argument)") {
        // --long=argument
        c.state.head = 1;
        c.state.token = c.argv[c.state.head];

        REQUIRE_THROWS_WITH(carp_parse_long_option(&c), "exit");
    }

    SECTION("error if long option has immediate argument but the spec requires multiple arguments") {
        // --long=argument
        c.state.head = 1;
        c.state.token = c.argv[c.state.head];
        g_table.push_back(CarpTable{ "long", CarpOptionSpec{ -1, carp_callback_override }});

        REQUIRE_THROWS_WITH(carp_parse_long_option(&c), "exit");

        g_table.clear();
    }
    SECTION("error if long option has immediate argument but the spec requires multiple arguments") {
        // --long=argument
        c.state.head = 1;
        c.state.token = c.argv[c.state.head];
        g_table.push_back(CarpTable{ "long", CarpOptionSpec{ 3, carp_callback_override }});

        REQUIRE_THROWS_WITH(carp_parse_long_option(&c), "exit");

        g_table.clear();
    }
    SECTION("error if long option has empty immediate argument") {
        // --long=
        c.state.head = 6;
        c.state.token = c.argv[c.state.head];
        g_table.push_back(CarpTable{ "long", CarpOptionSpec{ 1, carp_callback_override }});

        REQUIRE_THROWS_WITH(carp_parse_long_option(&c), "exit");

        g_table.clear();
    }
}

TEST_CASE("test carp_parse()") {
    struct Carp carp;
    const char* argv[] = {
        "a.out",
        "-abc",
        "cmd_arg1",
        "--foo=argument1",
        "cmd_arg2",
        "-xarg1",
        "arg2",
        "arg3",
        "cmd_arg3",
        "cmd_arg4",
        "--",
        "cmd_arg5",
        "cmd_arg6"
    };
    int argc = sizeof(argv) / sizeof(argv[0]);

    g_table.push_back(CarpTable{ "a", CarpOptionSpec{ 0, carp_callback_override}});
    g_table.push_back(CarpTable{ "b", CarpOptionSpec{ 0, carp_callback_override}});
    g_table.push_back(CarpTable{ "c", CarpOptionSpec{ 0, carp_callback_override}});
    g_table.push_back(CarpTable{ "foo", CarpOptionSpec{ 1, carp_callback_override}});
    g_table.push_back(CarpTable{ "x", CarpOptionSpec{ 3, carp_callback_override}});

    carp_parse(&carp, argc, (char**)argv);

    REQUIRE(carp.argc == 6);
    REQUIRE(std::string(carp.argv[0]) == "cmd_arg1");
    REQUIRE(std::string(carp.argv[1]) == "cmd_arg2");
    REQUIRE(std::string(carp.argv[2]) == "cmd_arg3");
    REQUIRE(std::string(carp.argv[3]) == "cmd_arg4");
    REQUIRE(std::string(carp.argv[4]) == "cmd_arg5");
    REQUIRE(std::string(carp.argv[5]) == "cmd_arg6");

    carp_cleanup(&carp);
    g_table.clear();

    REQUIRE(carp.argv == NULL);
    REQUIRE(carp.argc == 0);
}
