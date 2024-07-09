#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define assert(x, msg) ((x) ? 0 : __builtin_trap())
#define shift(ptr) ((*ptr)[0] ? (*ptr)++[0] : 0)
#define fprintln(stream, format, ...) (fprintf(stream, format "\n", ## __VA_ARGS__))

static bool strz_eql(char *a, char *b) { return strcmp(a, b) == 0; }

static char *cli_program_name = 0;
static int   cli_help         = 0;

static void display_help(FILE *stream)
{
    fprintln(stream, "USAGE");
    fprintln(stream, "   %s [options] [dirs]", cli_program_name);
    fprintln(stream, "");
    fprintln(stream, "OPTIONS");
    fprintln(stream, "   -h, --help   Display help information");
}

int main(int argc, char *argv[])
{
    cli_program_name = shift(&argv);
    assert(cli_program_name, "program was invoked with an empty argument list");
    for (char *arg; (arg = shift(&argv));) {
        if (strz_eql(arg, "-h") || strz_eql(arg, "--help")) { cli_help += 1; }
        else if (arg[0] == '-') {
            display_help(stderr);
            exit(1);
        }
    }

    if (cli_help) {
        display_help(stderr);
        exit(0);
    }
}
