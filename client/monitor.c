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
#include <unistd.h>

bool run_monitor;

void monitor()
{
    unsigned char c;
    const char *ready_msg = "PI Serial Loader ready!";
    int msg_idx = 0;

    for (;;) {
        ssize_t rd = read(ttyfd, &c, 1);
        if (rd < 0)
            continue;
        if (rd == 0)
            continue;
        putc((int)c, stdout);
        fflush(stdout);

        if (auto_load) {
            if (c == ready_msg[msg_idx]) {
                msg_idx++;
                if (ready_msg[msg_idx] == '\0') {
                    if (verbose_mode) {
                        vm_print_e(false, "\nAutoload: Matched '%s'\n", ready_msg);
                    }
                    vm_print_e(false, "\nAutoload: Pi reboot detected\n");
                    return;
                }
            } else if (c == ready_msg[0]) {
                msg_idx = 1;
            } else {
                msg_idx = 0;
            }
        }
    }
}
