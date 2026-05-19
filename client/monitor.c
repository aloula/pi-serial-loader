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
#include <sys/select.h>
#include <termios.h>
#include <stdlib.h>
#include <errno.h>

bool run_monitor;
bool terminal_mode;

static struct termios orig_termios;
static bool raw_mode_enabled = false;

void disable_raw_mode()
{
    if (!raw_mode_enabled) return;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    raw_mode_enabled = false;
}

void enable_raw_mode()
{
    if (raw_mode_enabled) return;

    if (!isatty(STDIN_FILENO)) {
        return;
    }

    if (tcgetattr(STDIN_FILENO, &orig_termios) != 0) {
        perror("tcgetattr(stdin)");
        return;
    }
    
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0) {
        raw_mode_enabled = true;
    } else {
        perror("tcsetattr(stdin)");
    }
}

static void cleanup_terminal(void)
{
    disable_raw_mode();
}

bool monitor()
{
    unsigned char c;
    const char *ready_msg = "PI Serial Loader ready!";
    int msg_idx = 0;
    struct termios tio_orig, tio_mon;

    static bool cleanup_registered = false;
    if (!cleanup_registered) {
        atexit(cleanup_terminal);
        cleanup_registered = true;
    }

    /* Set ttyfd to blocking mode for select() to be reliable */
    tcgetattr(ttyfd, &tio_orig);
    tio_mon = tio_orig;
    tio_mon.c_cc[VMIN] = 1;
    tio_mon.c_cc[VTIME] = 0;
    tcsetattr(ttyfd, TCSANOW, &tio_mon);

    if (terminal_mode) {
        enable_raw_mode();
        if (raw_mode_enabled) {
            fprintf(stderr, "[Terminal mode: Use Ctrl-X to exit]\n");
        } else {
            fprintf(stderr, "[Monitor mode: stdin is not a TTY, terminal input disabled]\n");
        }
    }

    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(ttyfd, &rfds);
        if (raw_mode_enabled) {
            FD_SET(STDIN_FILENO, &rfds);
        }

        int max_fd = ttyfd;
        if (raw_mode_enabled && STDIN_FILENO > max_fd) {
            max_fd = STDIN_FILENO;
        }

        int retval = select(max_fd + 1, &rfds, NULL, NULL, NULL);

        if (retval == -1) {
            if (errno == EINTR) continue;
            perror("select()");
            break;
        }

        if (FD_ISSET(ttyfd, &rfds)) {
            ssize_t rd = read(ttyfd, &c, 1);
            if (rd > 0) {
                if (c == '\n' && !raw_mode_enabled) {
                    putc('\r', stdout);
                }
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
                            if (raw_mode_enabled) disable_raw_mode();
                            tcsetattr(ttyfd, TCSANOW, &tio_orig);
                            return true; // Continue auto-load
                        }
                    } else if (c == ready_msg[0]) {
                        msg_idx = 1;
                    } else {
                        msg_idx = 0;
                    }
                }
            } else if (rd < 0 && errno != EAGAIN) {
                perror("read(ttyfd)");
                break;
            }
        }

        if (raw_mode_enabled && FD_ISSET(STDIN_FILENO, &rfds)) {
            ssize_t rd = read(STDIN_FILENO, &c, 1);
            if (rd > 0) {
                if (c == 0x18) { // Ctrl-X
                    disable_raw_mode();
                    fprintf(stderr, "\n[Terminal mode: Exiting]\n");
                    tcsetattr(ttyfd, TCSANOW, &tio_orig);
                    return false; // Stop everything
                }
                if (write(ttyfd, &c, 1) < 0) {
                    perror("write(ttyfd)");
                    break;
                }
            } else if (rd == 0) { // EOF
                break;
            } else if (errno != EAGAIN && errno != EINTR) {
                perror("read(stdin)");
                break;
            }
        }
    }
    
    if (raw_mode_enabled) {
        disable_raw_mode();
    }
    tcsetattr(ttyfd, TCSANOW, &tio_orig);
    return false;
}
