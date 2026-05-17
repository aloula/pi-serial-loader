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
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include "bootpc.h"
#include "bootproto.h"

static int read_full_with_timeout(void *buf, size_t size, unsigned max_ticks)
{
    uint8_t *p = (uint8_t *)buf;
    size_t got = 0;
    unsigned tick = 0;

    while (got < size && tick < max_ticks) {
        ssize_t rd = read(ttyfd, p + got, size - got);
        if (rd < 0)
            return -1;
        if (rd == 0) {
            tick++;
            continue;
        }
        got += (size_t)rd;
    }

    return got == size ? 0 : -1;
}

static void check_response()
{
    struct bp_rsp rsp;
    if (read_full_with_timeout(&rsp, sizeof(rsp), 80) != 0) {
        vm_print_e(true, "Failed to receive\n");
        exit(1);
    }

    switch (rsp.code) {
    case BPR_ACK:
        vm_print_e(false, "OK\n");
        return;
    case BPR_ERR:
        vm_print_e(true, "ERR %x\n", rsp.data);
        break;
    default:
        vm_print_e(true, "Unknown response: %08x\n", rsp.code);
        break;
    }
    exit(1);
}


static void init_hdr(struct bp_hdr *phdr, uint32_t p_type)
{
    memset(phdr, 0, sizeof(struct bp_hdr));
    phdr->p_type = p_type;
}


void ping()
{
    struct bp_hdr phdr;
    struct bp_rsp rsp;
    uint8_t raw_rsp[sizeof(struct bp_rsp)];
    uint8_t scan_buf[sizeof(struct bp_rsp)];
    char code_ascii[5];
    char data_ascii[5];
    size_t i, seen = 0;
    const size_t max_scan = 512;
    const unsigned max_ticks = 80;  /* ~8s with serial VTIME=1 */
    const unsigned resend_every = 10;
    unsigned tick;
    const unsigned int baud_try[] = {115200, 1843200};
    size_t baud_i;

    init_hdr(&phdr, BPT_PING);

    tcflush(ttyfd, TCIFLUSH);

    if (auto_load) {
        vm_print_s("Waiting connection...");
    } else {
        vm_print_s("Contacting RasPi bootloader...");
    }

    do {
        for (baud_i = 0; baud_i < sizeof(baud_try)/sizeof(baud_try[0]); baud_i++) {
            if (baud_try[baud_i] != get_serial_baud()) {
                if (set_serial_baud(baud_try[baud_i]) != 0) {
                    continue;
                }
                if (!auto_load) {
                    vm_print_e(false, " trying %u baud...", baud_try[baud_i]);
                }
            }

            seen = 0;

            if (write(ttyfd, &phdr, sizeof(phdr)) != sizeof(phdr)) {
                vm_print_e(true, "ERROR failed to send PING\n");
                exit(1);
            }

            for (tick = 0; tick < max_ticks && seen < max_scan; tick++) {
            uint8_t b;
            ssize_t rd = read(ttyfd, &b, 1);

            if (rd < 0) {
                vm_print_e(true, "ERROR failed to receive response\n");
                exit(1);
            }

            if (rd == 0) {
                if (((tick + 1) % resend_every) == 0) {
                    if (write(ttyfd, &phdr, sizeof(phdr)) != sizeof(phdr)) {
                        vm_print_e(true, "ERROR failed to resend PING\n");
                        exit(1);
                    }
                }
                continue;
            }

            if (seen < sizeof(scan_buf)) {
                scan_buf[seen] = b;
            } else {
                memmove(scan_buf, scan_buf + 1, sizeof(scan_buf) - 1);
                scan_buf[sizeof(scan_buf) - 1] = b;
            }
            seen++;

                if (seen < sizeof(scan_buf))
                    continue;

                memcpy(&rsp, scan_buf, sizeof(rsp));

                if (rsp.code == BPR_RDY) {
                    vm_print_e(false, "OK\n");
                    tcflush(ttyfd, TCIFLUSH);
                    return;
                }

                if (rsp.code == BPR_ERR) {
                    vm_print_e(true, "ERR %x\n", rsp.data);
                    exit(1);
                }
            }
        }
    } while (auto_load);

    memcpy(raw_rsp, scan_buf, sizeof(raw_rsp));
    memcpy(&rsp, raw_rsp, sizeof(rsp));
    for (i = 0; i < 4; i++) {
        code_ascii[i] = (raw_rsp[i] >= 32 && raw_rsp[i] <= 126) ? raw_rsp[i] : '.';
        data_ascii[i] = (raw_rsp[i + 4] >= 32 && raw_rsp[i + 4] <= 126) ? raw_rsp[i + 4] : '.';
    }
    code_ascii[4] = '\0';
    data_ascii[4] = '\0';

    if (seen == 0) {
        vm_print_e(true,
                   "ERROR timeout waiting for bootloader response on serial (no data after ~%u s)\n",
                   max_ticks / 10);
    } else {
        vm_print_e(true, "ERROR no RDY after %u bytes (last code=%08x data=%08x raw='%s' '%s')\n",
                   (unsigned)seen, rsp.code, rsp.data, code_ascii, data_ascii);
    }
    vm_print_e(true,
               "Hint: serial seems to carry non-protocol data. Check SD boots PiLoader kernel7.img and UART mapping on Pi 3B+.\n");
    exit(1);
}

void load_buffer(uint32_t sh_addr, void *sdata, uint32_t sh_size)
{
    struct bp_hdr phdr;
    init_hdr(&phdr, BPT_LOAD);
    phdr.address = sh_addr;
    phdr.size = sh_size;
    phdr.crc32 = crc32(0, sdata, sh_size);

    write(ttyfd, &phdr, sizeof(phdr));
    write(ttyfd, sdata, sh_size);

    check_response();
}

void load_section(uint32_t sh_addr, uint32_t sh_offset, uint32_t sh_size)
{
    void *sdata = malloc(sh_size);
    if (sdata == NULL) {
        vm_fail("Out of memory or something.\n");
    }
    fseek(ufile, sh_offset, SEEK_SET);
    fread(sdata, sh_size, 1, ufile);

    vm_print_s("LOAD %08x %08x %08x...", sh_addr, sh_offset, sh_size);

    load_buffer(sh_addr, sdata, sh_size);

    free(sdata);
}


void zero_section(uint32_t sh_addr, uint32_t sh_size)
{
    struct bp_hdr phdr;
    init_hdr(&phdr, BPT_ZERO);
    phdr.address = sh_addr;
    phdr.size = sh_size;

    if (beef_bss)
        phdr.flags |= BPF_BEEF;

    write(ttyfd, &phdr, sizeof(phdr));

    vm_print_s("%s %08x          %08x...", (beef_bss ? "BEEF" : "ZERO" ), phdr.address, phdr.size);

    check_response();
}


void exec_program(uint32_t e_entry)
{
    struct bp_hdr phdr;
    init_hdr(&phdr, BPT_EXEC);
    phdr.address = e_entry;

    if (no_watchdog)
        phdr.flags |= BPF_NOWD;

    vm_print_e(false, "EXEC %08x%s...", phdr.address, (no_watchdog ? " nwd" : ""));

    write(ttyfd, &phdr, sizeof(phdr));

    check_response();
}


void reboot_pi(void)
{
    struct bp_hdr phdr;
    struct bp_rsp rsp;

    init_hdr(&phdr, BPT_REBOOT);

    vm_print_s("Rebooting the RasPi...");

    write(ttyfd, &phdr, sizeof(phdr));

    check_response();
}
