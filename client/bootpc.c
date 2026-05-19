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

#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include "bootpc.h"
#include "bootproto.h"

FILE *ufile;
uint32_t e_entry = 0x8000;
uint32_t e_load  = 0x8000;
int loader_action;
bool no_watchdog;
bool auto_load;

int main(int argc, char **argv)
{


    load_config();
    parse_cmdline(argc, argv);

    if (loader_action == LACT_USAGE) {
        usage();
        return 0;
    }

    // Save initial state for auto-load
    uint32_t initial_e_entry = e_entry;
    uint32_t initial_e_load = e_load;
    int initial_optind = optind;

    setup_serial(port);

    do {
        // Reset state for each load
        e_entry = initial_e_entry;
        e_load = initial_e_load;
        optind = initial_optind;

        ping();

        for (;optind < argc; optind++) {

            ufile = fopen(argv[optind], "rb");
            if (ufile == NULL) {
                vm_fail("Can not open file %s\n", argv[optind]);
            }

            if (check_elf())
                load_elf(&e_entry);
            else
                load_binary();

            fclose(ufile);
        }

        switch(loader_action)
        {
        case LACT_EXEC:
            exec_program(e_entry);
            break;
        case LACT_REBOOT:
            reboot_pi();
            break;
        }

        if (run_monitor) {
            /* Target kernels commonly initialize PL011 for 115200; align monitor speed. */
            set_serial_baud(115200);
            if (!monitor()) break;
        }

    } while (auto_load);

    close(ttyfd);
    return 0;
}
