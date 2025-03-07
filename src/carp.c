#include "carp.h"
#include "carp_backend.h"
#include "carp_argument_vector.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef CARP_UNIT_TEST
#define CARP_STATIC
#else
#define CARP_STATIC static
#endif

struct CarpPrivate {
    const char** argv;

    struct CarpArgumentVector* callback_args;
    struct CarpArgumentVector* command_args;

    void* callback_param;

    struct {
        int head;
        const int tail;
        const char* token;
    } state;
};

enum CarpTokenType {
    TOKEN_SHORT_OPTION = 0,
    TOKEN_LONG_OPTION,
    TOKEN_SEPARATOR,
    TOKEN_ARGUMENT
};

enum CarpError {
    ERROR_NOT_ENOUGH_ARGUMENTS = 0,
    ERROR_UNKNOWN_OPTION,
    ERROR_LONG_OPTION_ARGUMENT_COUNT,
    ERROR_COUNT
};

typedef void (*CARP_ERROR_MSG_GENERATOR)(struct CarpPrivate*, char*, int);
CARP_STATIC void carp_error_msg_not_enough_arguments(
    struct CarpPrivate* c,
    char* msg_buf,
    int buf_size)
{
    (void)snprintf(msg_buf, buf_size, "Token '%s': not enough arguments supplied to option", c->state.token);
}

CARP_STATIC void carp_error_msg_unknown_option(
    struct CarpPrivate* c,
    char* msg_buf,
    int buf_size)
{
    (void)snprintf(msg_buf, buf_size, "Token '%s': unknown option", c->state.token);
}

CARP_STATIC void carp_error_msg_long_option_argument_count(
    struct CarpPrivate* c,
    char* msg_buf,
    int buf_size)
{
    // Throw this error when a long option is provided with an immediate argument (e.g.: --long=argument)
    //  but the option spec requires multiple arguments.
    (void)snprintf(msg_buf, buf_size, "Token '%s': option requires multiple arguments but use of '=' implies single argument", c->state.token);
}

CARP_STATIC void carp_exit_with_error(
    struct CarpPrivate* c,
    enum CarpError error)
{
    static const CARP_ERROR_MSG_GENERATOR error_generator[ERROR_COUNT] = {
        [ERROR_NOT_ENOUGH_ARGUMENTS] = carp_error_msg_not_enough_arguments,
        [ERROR_UNKNOWN_OPTION] = carp_error_msg_unknown_option,
        [ERROR_LONG_OPTION_ARGUMENT_COUNT] = carp_error_msg_long_option_argument_count
    };
    static char msg_buf[100] = {0};

    error_generator[error](c, msg_buf, sizeof(msg_buf));

    carp_vector_cleanup(c->callback_args);
    carp_vector_cleanup(c->command_args);

    printf("[carp] %s\n", msg_buf);
    exit(EXIT_FAILURE);
}

CARP_STATIC enum CarpTokenType carp_classify_token(
    const char* token)
{
    if (!strncmp(token, "--", 2) && strlen(token) == 2) {
        return TOKEN_SEPARATOR;
    }
    else if (!strncmp(token, "--", 2)) {
        return TOKEN_LONG_OPTION;
    }
    else if (!strncmp(token, "-", 1)) {
        return TOKEN_SHORT_OPTION;
    }
    else {
        return TOKEN_ARGUMENT;
    }
}

CARP_STATIC void carp_callback_wrapper(
    CARP_CALLBACK cb,
    void* cb_param,
    const char** argv,
    int argc)
{
    if (cb) {
        cb(cb_param, argv, argc);
    }
    else {
        // TODO: error?
    }
}

CARP_STATIC int carp_option_argument_handler(
    struct CarpPrivate* c,
    int required_arguments,
    const char* immediate)
{
    int args_remaining = required_arguments;
    int head_increment = 1;

    int head = c->state.head + 1;
    const char** argument_list = c->argv + head;

    if (immediate != NULL && *immediate != '\0') {
        carp_vector_push(c->callback_args, immediate);
        args_remaining--;
    }

    if (required_arguments == -1) {
        while (carp_classify_token(*argument_list) == TOKEN_ARGUMENT &&
               head < c->state.tail)
        {
            carp_vector_push(c->callback_args, *argument_list);
            argument_list++;
            head_increment++;
            head++;
        }
    }
    else {
        while (args_remaining > 0) {
            if (carp_classify_token(*argument_list) == TOKEN_ARGUMENT &&
                head < c->state.tail)
            {
                carp_vector_push(c->callback_args, *argument_list);
                argument_list++;
                args_remaining--;
                head_increment++;
                head++;
            }
            else {
                carp_exit_with_error(c, ERROR_NOT_ENOUGH_ARGUMENTS);
            }
        }
    }

    return head_increment;
}

CARP_STATIC void carp_parse_short_option(
    struct CarpPrivate* c)
{
    struct CarpOptionSpec* spec = NULL;
    int head_increment = 1;

    // +1 to skip '-'
    const char* token = c->state.token + 1;
    int tokenlen = strlen(token);

    for (const char* opt = token; opt < (token + tokenlen); opt++) {
        if ((spec = carp_backend_search(opt, 1)) != NULL) {
            if (spec->arguments == 0) {
                carp_callback_wrapper(spec->callback, c->callback_param, NULL, 0);
            }
            else {
                head_increment = carp_option_argument_handler(c, spec->arguments, opt + 1);
                carp_callback_wrapper(spec->callback, c->callback_param, c->callback_args->buf, c->callback_args->size);
                c->callback_args->size = 0;
                goto next_token;
            }
        }
        else {
            carp_exit_with_error(c, ERROR_UNKNOWN_OPTION);
        }
    }

next_token:
    c->state.head += head_increment;
}

CARP_STATIC void carp_parse_long_option(
    struct CarpPrivate* c)
{
    struct CarpOptionSpec* spec = NULL;
    int head_increment = 1;

    // +2 to skip '--'
    const char* opt = c->state.token + 2;
    int optlen = strlen(opt);
    char* search = strchr(opt, '=');

    if (search) {
        long long diff = llabs(search - opt);

        // Error if empty immediate argument (e.g.: '--long=')
        if (diff == (optlen - 1)) {
            carp_exit_with_error(c, ERROR_NOT_ENOUGH_ARGUMENTS);
        }

        if ((spec = carp_backend_search(opt, diff)) != NULL) {
            if (spec->arguments == -1 || spec->arguments != 1) {
                carp_exit_with_error(c, ERROR_LONG_OPTION_ARGUMENT_COUNT);
            }
            else {
                head_increment = carp_option_argument_handler(c, spec->arguments, search + 1);
                carp_callback_wrapper(spec->callback, c->callback_param, c->callback_args->buf, c->callback_args->size);
                c->callback_args->size = 0;
            }
        }
        else {
            carp_exit_with_error(c, ERROR_UNKNOWN_OPTION);
        }
    }
    else {
        if ((spec = carp_backend_search(opt, optlen)) != NULL) {
            if (spec->arguments == -1 || spec->arguments > 0) {
                head_increment = carp_option_argument_handler(c, spec->arguments, NULL);
                carp_callback_wrapper(spec->callback, c->callback_param, c->callback_args->buf, c->callback_args->size);
                c->callback_args->size = 0;
            }
            else {
                carp_callback_wrapper(spec->callback, c->callback_param, NULL, 0);
            }
        }
        else {
            carp_exit_with_error(c, ERROR_UNKNOWN_OPTION);
        }
    }

    c->state.head += head_increment;
}

CARP_STATIC void carp_parse_arguments_after_separator(
    struct CarpPrivate* c)
{
    while (c->state.head < c->state.tail) {
        c->state.token = c->argv[c->state.head++];
        carp_vector_push(c->command_args, c->state.token);
    }
}

void carp_parse(
    struct Carp* carp,
    int argc,
    char* argv[],
    void* callback_param)
{
    struct CarpArgumentVector callback_args;
    struct CarpArgumentVector command_args;

#define CARP_VECTOR_INIT_CAP 25
    if (carp_vector_init(&callback_args, CARP_VECTOR_INIT_CAP) ||
        carp_vector_init(&command_args, CARP_VECTOR_INIT_CAP))
    {
        // TODO: error msg
        exit(1);
    }

    struct CarpPrivate c = {
        .argv = (const char**)argv,
        .callback_args = &callback_args,
        .command_args = &command_args,
        .callback_param = callback_param,
        .state = {
            .head = 1,
            .tail = argc,
            .token = NULL
        }
    };

    while (c.state.head < c.state.tail) {
        c.state.token = argv[c.state.head];
        switch (carp_classify_token(c.state.token)) {
            case TOKEN_SHORT_OPTION:
                carp_parse_short_option(&c);
                break;
            case TOKEN_LONG_OPTION:
                carp_parse_long_option(&c);
                break;
            case TOKEN_SEPARATOR:
                c.state.head++;
                carp_parse_arguments_after_separator(&c);
                break;
            case TOKEN_ARGUMENT:
                carp_vector_push(c.command_args, c.state.token);
                c.state.head++;
                break;
        }
    }

    // No longer needed; all option callbacks should have been called by now
    carp_vector_cleanup(c.callback_args);

    if (carp) {
        carp->argv = c.command_args->buf;
        carp->argc = c.command_args->size;
    }
}

void carp_cleanup(
    struct Carp *carp)
{
    free(carp->argv);
    carp->argv = NULL;
    carp->argc = 0;
}
