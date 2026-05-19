/* Copyright (c) 2013 Jurģis Brigmanis
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

 #include "bootpc.h"
 #include <getopt.h>
 #include <stdlib.h>
 #include <string.h>
 #include <ctype.h>

struct option long_options[] = {
    {"help", no_argument, 0, 'h'},
    {"monitor", no_argument, 0, 'm'},
    {"terminal", no_argument, 0, 't'},
    {"auto-load", no_argument, 0, 'a'},
    {"beef-bss", no_argument, 0, 'b'},
    {"port", required_argument, 0, 'p'},
    {"verbose", no_argument, 0, 'v'},
    {"load-addr", required_argument, 0, 'l'},
    {"exec-addr", required_argument, 0, 'x'},
    {"suspended", no_argument, 0, 's'},
    {"no-watchdog", no_argument, 0, 'w'},
    {"reboot", no_argument, 0, 'r'},
    {"config", required_argument, 0, 'c'},
    {0, 0, 0, 0}
};

static char *trim(char *s)
{
    while (isspace((unsigned char)*s)) s++;
    if (*s == 0) return s;
    char *back = s + strlen(s) - 1;
    while (back > s && isspace((unsigned char)*back)) back--;
    *(back + 1) = 0;
    return s;
}

void load_config_file(const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (!f) return;

    if (verbose_mode) vm_print_e(false, "Loading config from %s\n", filename);

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *ptr = strchr(line, '#');
        if (ptr) *ptr = 0;

        ptr = strchr(line, '=');
        if (!ptr) continue;

        *ptr = 0;
        char *key = trim(line);
        char *val = trim(ptr + 1);

        if (strcmp(key, "port") == 0) {
            port = strdup(val);
        } else if (strcmp(key, "verbose") == 0) {
            verbose_mode = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
        } else if (strcmp(key, "terminal") == 0) {
            terminal_mode = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
            if (terminal_mode) run_monitor = true;
        } else if (strcmp(key, "monitor") == 0) {
            run_monitor = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
        } else if (strcmp(key, "auto_load") == 0) {
            auto_load = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
        } else if (strcmp(key, "no_watchdog") == 0) {
            no_watchdog = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
        } else if (strcmp(key, "beef_bss") == 0) {
            beef_bss = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
        } else if (strcmp(key, "load_addr") == 0) {
            e_load = parse_addr(val);
        } else if (strcmp(key, "exec_addr") == 0) {
            e_entry = parse_addr(val);
        }
    }
    fclose(f);
}

void load_config(void)
{
    // Try local piboot.conf first, then ~/.pibootrc
    load_config_file("piboot.conf");
    
    char *home = getenv("HOME");
    if (home) {
        char path[512];
        snprintf(path, sizeof(path), "%s/.pibootrc", home);
        load_config_file(path);
    }
}

void usage(void)
{
    printf("USAGE:\n");
    printf("    piboot <options> <kernel>\n");
    printf("\n");
    printf("    -h | --help             Show this screen.\n");
    printf("    -p | --port <device>    Use specified serial port to communicate.\n");
    printf("    -b | --beef-bss         Fill BSS section with 0xDEADBEEF instead of zeros.\n");
    printf("    -m | --monitor          Start monitoring uart output after starting kernel\n");
    printf("    -t | --terminal         Start serial terminal after starting kernel\n");
    printf("    -a | --auto-load        Automatically reload kernel when Pi reboots\n");
    printf("    -l | --load-addr <addr> Specify address where to load binary files.\n"
           "       |                    Defaults to 0x8000, increases with each loaded file.\n"
           "       |                    Has no effect when loading ELF files.\n");
    printf("    -x | --exec-addr <addr> Address where to start executing. Defaults to\n"
           "       |                    0x8000. When loading ELF, it's entrypoint address\n"
           "       |                    is used instead.\n");
    printf("    -s | --suspended        Do not start to execute right away.\n");
    printf("    -w | --no-watchdog      Disable watchdog.\n");
    printf("    -v | --verbose          Display what actions are performed.\n");
    printf("    -r | --reboot           Reboot the Pi.\n");
    printf("    -c | --config <file>    Load configuration from file.\n");
}

uint32_t parse_addr(const char *s_addr)
{
    char *eptr;
    unsigned long a2 = strtoul(s_addr, &eptr, 16);

    if (*eptr != '\0')
        vm_fail("Not a valid address: '%s'\n", s_addr);

    return a2;
}

void parse_cmdline(int argc, char **argv)
{
    int c;
    int option_index = 0;

    loader_action = LACT_EXEC; // Exec by default

    // First pass to find if a custom config file is specified
    while ((c = getopt_long(argc, argv, "hmvbp:l:x:swrac:", long_options, &option_index)) != -1) {
        if (c == 'c') {
            load_config_file(optarg);
        }
    }
    
    // Reset for second pass
    optind = 1;

    for (;;) {
        c = getopt_long(argc, argv, "hmvbp:l:x:swrac:", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'h':
            loader_action = LACT_USAGE;
            return;
        case 'm':
            run_monitor = true;
            break;
        case 't':
            terminal_mode = true;
            run_monitor = true;
            break;
        case 'a':
            auto_load = true;
            break;
        case 'b':
            beef_bss = true;
            break;
        case 'p':
            if (port) free(port);
            port = strdup(optarg);
            break;
        case 'v':
            verbose_mode = true;
            break;
        case 'l':
            e_load = parse_addr(optarg);
            break;
        case 'x':
            e_entry = parse_addr(optarg);
            break;
        case 's':
            loader_action = LACT_NONE;
            break;
        case 'w':
            no_watchdog = true;
            break;
        case 'r':
            loader_action = LACT_REBOOT;
            return;
        case 'c':
            // Already handled in first pass
            break;
        default:
            abort();
        }
    }

    if (port == NULL) {
        usage();
        exit(1);
    }
}
