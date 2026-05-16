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
#include <termios.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

int ttyfd;
FILE *ttyfs;
char *port;
static unsigned int current_baud = 115200;

static int set_speed(struct termios *tio, unsigned int baud)
{
    speed_t speed;

    switch (baud) {
    case 115200:
        speed = B115200;
        break;
#ifdef B1843200
    case 1843200:
        speed = B1843200;
        break;
#endif
    default:
        return -1;
    }

    if (cfsetispeed(tio, speed) != 0 || cfsetospeed(tio, speed) != 0)
        return -1;

    return 0;
}

int set_serial_baud(unsigned int baud)
{
    struct termios tio;

    if (tcgetattr(ttyfd, &tio) != 0)
        return -1;

    if (set_speed(&tio, baud) != 0)
        return -1;

    tcflush(ttyfd, TCIFLUSH);
    if (tcsetattr(ttyfd, TCSANOW, &tio) != 0)
        return -1;

    current_baud = baud;
    return 0;
}

unsigned int get_serial_baud(void)
{
    return current_baud;
}

void setup_serial(const char *port)
{
    struct termios newtio;

    ttyfd = open(port, O_RDWR | O_NOCTTY);
    if (ttyfd == -1) {
        vm_fail("Can not open device %s\n", port);
    }


    memset(&newtio, 0, sizeof(newtio));
    newtio.c_cflag = CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    /* Timed read mode: read() returns 0 after 0.1s when no byte arrives. */
    newtio.c_cc[VTIME]    = 1;
    newtio.c_cc[VMIN]     = 0;

    if (set_speed(&newtio, current_baud) != 0) {
        vm_fail("Unsupported serial speed: %u\n", current_baud);
    }

    tcflush(ttyfd, TCIFLUSH);
    tcsetattr(ttyfd,TCSANOW,&newtio);

    ttyfs = fdopen(ttyfd, "rb");
}
