/*
 * Copyright (c) 2021 roleo.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Dump h264 content from /dev/shm/fshare_frame_buffer to stdout or fifo
 */

#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <getopt.h>
#include <signal.h>
#include <pthread.h>

#define BUF_OFFSET_Y21GA 368
#define FRAME_HEADER_SIZE_Y21GA 28
#define DATA_OFFSET_Y21GA 4
#define LOWRES_BYTE_Y21GA 8
#define HIGHRES_BYTE_Y21GA 4

#define BUF_OFFSET_Y211GA 368
#define FRAME_HEADER_SIZE_Y211GA 28
#define DATA_OFFSET_Y211GA 4
#define LOWRES_BYTE_Y211GA 8
#define HIGHRES_BYTE_Y211GA 4

#define BUF_OFFSET_H30GA 368
#define FRAME_HEADER_SIZE_H30GA 28
#define DATA_OFFSET_H30GA 4
#define LOWRES_BYTE_H30GA 8
#define HIGHRES_BYTE_H30GA 4

#define BUF_OFFSET_R30GB 300
#define FRAME_HEADER_SIZE_R30GB 22
#define DATA_OFFSET_R30GB 0
#define LOWRES_BYTE_R30GB 8
#define HIGHRES_BYTE_R30GB 4

#define BUF_OFFSET_R35GB 300
#define FRAME_HEADER_SIZE_R35GB 26
#define DATA_OFFSET_R35GB 4
#define LOWRES_BYTE_R35GB 8
#define HIGHRES_BYTE_R35GB 4

#define BUF_OFFSET_R40GA 300
#define FRAME_HEADER_SIZE_R40GA 26
#define DATA_OFFSET_R40GA 4
#define LOWRES_BYTE_R40GA 8
#define HIGHRES_BYTE_R40GA 4

#define BUF_OFFSET_H51GA 368
#define FRAME_HEADER_SIZE_H51GA 28
#define DATA_OFFSET_H51GA 4
#define LOWRES_BYTE_H51GA 8
#define HIGHRES_BYTE_H51GA 4

#define BUF_OFFSET_H52GA 368
#define FRAME_HEADER_SIZE_H52GA 28
#define DATA_OFFSET_H52GA 4
#define LOWRES_BYTE_H52GA 8
#define HIGHRES_BYTE_H52GA 4

#define BUF_OFFSET_H60GA 368
#define FRAME_HEADER_SIZE_H60GA 28
#define DATA_OFFSET_H60GA 4
#define LOWRES_BYTE_H60GA 8
#define HIGHRES_BYTE_H60GA 4

#define BUF_OFFSET_Y28GA 368
#define FRAME_HEADER_SIZE_Y28GA 28
#define DATA_OFFSET_Y28GA 4
#define LOWRES_BYTE_Y28GA 8
#define HIGHRES_BYTE_Y28GA 4

#define BUF_OFFSET_Y29GA 368
#define FRAME_HEADER_SIZE_Y29GA 28
#define DATA_OFFSET_Y29GA 4
#define LOWRES_BYTE_Y29GA 8
#define HIGHRES_BYTE_Y29GA 4

#define BUF_OFFSET_Q321BR_LSX 300
#define FRAME_HEADER_SIZE_Q321BR_LSX 26
#define DATA_OFFSET_Q321BR_LSX 4
#define LOWRES_BYTE_Q321BR_LSX 8
#define HIGHRES_BYTE_Q321BR_LSX 4

#define BUF_OFFSET_QG311R 300
#define FRAME_HEADER_SIZE_QG311R 26
#define DATA_OFFSET_QG311R 4
#define LOWRES_BYTE_QG311R 8
#define HIGHRES_BYTE_QG311R 4

#define BUF_OFFSET_B091QP 300
#define FRAME_HEADER_SIZE_B091QP 26
#define DATA_OFFSET_B091QP 4
#define LOWRES_BYTE_B091QP 8
#define HIGHRES_BYTE_B091QP 4

#define MILLIS_25 25000

#define RESOLUTION_NONE 0
#define RESOLUTION_LOW  360
#define RESOLUTION_HIGH 1080
#define RESOLUTION_BOTH 1440

#define RESOLUTION_FHD  1080
#define RESOLUTION_3K   1296

#define BUFFER_FILE "/dev/shm/fshare_frame_buf"

#define FIFO_NAME_LOW "/tmp/h264_low_fifo"
#define FIFO_NAME_HIGH "/tmp/h264_high_fifo"

int buf_offset;
int buf_size;
int frame_header_size;
int data_offset;
int lowres_byte;
int highres_byte;

unsigned char IDR4[]               = {0x65, 0xB8};
unsigned char NALx_START[]         = {0x00, 0x00, 0x00, 0x01};
unsigned char IDR4_START[]         = {0x00, 0x00, 0x00, 0x01, 0x65, 0x88};
unsigned char IDR5_START[]         = {0x00, 0x00, 0x00, 0x01, 0x26};
unsigned char PFR4_START[]         = {0x00, 0x00, 0x00, 0x01, 0x41};
unsigned char PFR5_START[]         = {0x00, 0x00, 0x00, 0x01, 0x02};
unsigned char SPS4_START[]         = {0x00, 0x00, 0x00, 0x01, 0x67};
unsigned char SPS5_START[]         = {0x00, 0x00, 0x00, 0x01, 0x42};
unsigned char PPS4_START[]         = {0x00, 0x00, 0x00, 0x01, 0x68};
unsigned char PPS5_START[]         = {0x00, 0x00, 0x00, 0x01, 0x44};
unsigned char VPS5_START[]         = {0x00, 0x00, 0x00, 0x01, 0x40};

unsigned char SPS4_640X360[]       = {0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x00, 0x14,
                                        0x96, 0x54, 0x05, 0x01, 0x7B, 0xCB, 0x37, 0x01,
                                        0x01, 0x01, 0x02};
unsigned char SPS4_640X360_TI[]    = {0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x00, 0x14,
                                        0x96, 0x54, 0x05, 0x01, 0x7B, 0xCB, 0x37, 0x01,
                                        0x01, 0x01, 0x40, 0x00, 0x00, 0x7D, 0x00, 0x00,
                                        0x13, 0x88, 0x21};
unsigned char SPS4_1920X1080[]     = {0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x00, 0x20,
                                        0x96, 0x54, 0x03, 0xC0, 0x11, 0x2F, 0x2C, 0xDC,
                                        0x04, 0x04, 0x04, 0x08};
unsigned char SPS4_1920X1080_TI[]  = {0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x00, 0x20,
                                        0x96, 0x54, 0x03, 0xC0, 0x11, 0x2F, 0x2C, 0xDC,
                                        0x04, 0x04, 0x05, 0x00, 0x00, 0x03, 0x01, 0xF4,
                                        0x00, 0x00, 0x4E, 0x20, 0x84};
unsigned char SPS4_2304X1296[]     = {0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x00, 0x20,
                                        0x96, 0x54, 0x01, 0x20, 0x05, 0x19, 0x37, 0x01,
                                        0x01, 0x01, 0x02};
unsigned char SPS4_2304X1296_TI[]  = {0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x00, 0x20,
                                        0x96, 0x54, 0x01, 0x20, 0x05, 0x19, 0x37, 0x01,
                                        0x00, 0x00, 0x40, 0x00, 0x00, 0x7D, 0x00, 0x00,
                                        0x13, 0x88, 0x21};
unsigned char SPS4_2_640X360[]     = {0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x14,
                                        0xAC, 0x2C, 0xA8, 0x0A, 0x02, 0xF7, 0x96, 0x6E,
                                        0x02, 0x02, 0x02, 0x04};
unsigned char SPS4_2_640X360_TI[]  = {0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x14,
                                        0xAC, 0x2C, 0xA8, 0x0A, 0x02, 0xF7, 0x96, 0x6E,
                                        0x02, 0x02, 0x02, 0x80, 0x00, 0x00, 0xFA, 0x00,
                                        0x00, 0x27, 0x10, 0x42};
unsigned char SPS4_2_1920X1080[]   = {0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x20,
                                        0xAC, 0x2C, 0xA8, 0x07, 0x80, 0x22, 0x5E, 0x59,
                                        0xB8, 0x08, 0x08, 0x08, 0x10};
unsigned char SPS4_2_1920X1080_TI[] = {0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x20,
                                        0xAC, 0x2C, 0xA8, 0x07, 0x80, 0x22, 0x5E, 0x59,
                                        0xB8, 0x08, 0x08, 0x0A, 0x00, 0x00, 0x03, 0x03,
                                        0xE8, 0x00, 0x00, 0x9C, 0x41, 0x08};
unsigned char SPS4_2_2304X1296[]   = {0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x20,
                                        0xAC, 0x2C, 0xA8, 0x02, 0x40, 0x0A, 0x32, 0x6E,
                                        0x02, 0x02, 0x02, 0x04};
unsigned char SPS4_2_2304X1296_TI[] = {0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x20,
                                        0xAC, 0x2C, 0xA8, 0x02, 0x40, 0x0A, 0x32, 0x6E,
                                        0x02, 0x02, 0x02, 0x80, 0x00, 0x00, 0xFA, 0x00,
                                        0x00, 0x27, 0x10, 0x42};
unsigned char SPS4_3_640X360[]      = {0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x14,
                                       0xAC, 0x2C, 0xA8, 0x0A, 0x02, 0xF7, 0x96, 0x6A,
                                       0x02, 0x02, 0x02, 0x04};
unsigned char SPS4_3_640X360_TI[]   = {0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x14,
                                       0xAC, 0x2C, 0xA8, 0x0A, 0x02, 0xF7, 0x96, 0x6A,
                                       0x02, 0x02, 0x02, 0x80, 0x00, 0x00, 0xFA, 0x00,
                                       0x00, 0x27, 0x10, 0x42};
unsigned char SPS4_3_1920X1080[]    = {0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x20,
                                       0xAC, 0x2C, 0xA8, 0x07, 0x80, 0x22, 0x5E, 0x59,
                                       0xA8, 0x08, 0x08, 0x08, 0x10};
unsigned char SPS4_3_1920X1080_TI[] = {0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x20,
                                       0xAC, 0x2C, 0xA8, 0x07, 0x80, 0x22, 0x5E, 0x59,
                                       0xA8, 0x08, 0x08, 0x0A, 0x00, 0x00, 0x03, 0x03,
                                       0xE8, 0x00, 0x00, 0x9C, 0x41, 0x08};
unsigned char VPS5_1920X1080[]     = {0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0C, 0x01,
                                        0xFF, 0xFF, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00,
                                        0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03,
                                        0x00, 0x7B, 0xAC, 0x09};
unsigned char VPS5_1920X1080_TI[]  = {0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0C, 0x01,
                                        0xFF, 0xFF, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00,
                                        0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03,
                                        0x00, 0x7B, 0xAC, 0x0C, 0x00, 0x00, 0x0F, 0xA4,
                                        0x00, 0x01, 0x38, 0x81, 0x40};

unsigned char *addr;                      /* Pointer to shared memory region (header) */
int resolution;
int sps_timing_info;
int fifo;
int debug;

long long current_timestamp() {
    struct timeval te; 
    gettimeofday(&te, NULL); // get current time
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // calculate milliseconds

    return milliseconds;
}

/* Locate a string in the circular buffer */
unsigned char *cb_memmem(unsigned char *src, int src_len, unsigned char *what, int what_len)
{
    unsigned char *p;

    if (src_len >= 0) {
        p = (unsigned char*) memmem(src, src_len, what, what_len);
    } else {
        // From src to the end of the buffer
        p = (unsigned char*) memmem(src, addr + buf_size - src, what, what_len);
        if (p == NULL) {
            // And from the start of the buffer size src_len
            p = (unsigned char*) memmem(addr + buf_offset, src + src_len - (addr + buf_offset), what, what_len);
        }
    }
    return p;
}

unsigned char *cb_move(unsigned char *buf, int offset)
{
    buf += offset;
    if ((offset > 0) && (buf > addr + buf_size))
        buf -= (buf_size - buf_offset);
    if ((offset < 0) && (buf < addr + buf_offset))
        buf += (buf_size - buf_offset);

    return buf;
}

// The second argument is the circular buffer
int cb_memcmp(unsigned char *str1, unsigned char *str2, size_t n)
{
    int ret;

    if (str2 + n > addr + buf_size) {
        ret = memcmp(str1, str2, addr + buf_size - str2);
        if (ret != 0) return ret;
        ret = memcmp(str1 + (addr + buf_size - str2), addr + buf_offset, n - (addr + buf_size - str2));
    } else {
        ret = memcmp(str1, str2, n);
    }

    return ret;
}

// The second argument is the circular buffer
void cb2s_memcpy(unsigned char *dest, unsigned char *src, size_t n)
{
    if (src + n > addr + buf_size) {
        memcpy(dest, src, addr + buf_size - src);
        memcpy(dest + (addr + buf_size - src), addr + buf_offset, n - (addr + buf_size - src));
    } else {
        memcpy(dest, src, n);
    }
}

void sigpipe_handler(int unused)
{
    // Do nothing
}

void* unlock_fifo_thread(void *data)
{
    int fd;
    char *fifo_name = data;
    unsigned char buffer_fifo[1024];

    fd = open(fifo_name, O_RDONLY);
    read(fd, buffer_fifo, 1024);
    close(fd);

    return NULL;
}

void print_usage(char *progname)
{
    fprintf(stderr, "\nUsage: %s [-m MODEL] [-r RES] [-s] [-f] [-d]\n\n", progname);
    fprintf(stderr, "\t-m MODEL, --model MODEL\n");
    fprintf(stderr, "\t\tset model: y21ga, y211ga, h30ga, r30gb, r35gb, r40ga, h51ga, h52ga, h60ga, y28ga, y29ga, q321br_lsx, qg311r or b091qp (default y21ga)\n");
    fprintf(stderr, "\t-r RES, --resolution RES\n");
    fprintf(stderr, "\t\tset resolution: LOW or HIGH (default HIGH)\n");
    fprintf(stderr, "\t-s, --sti\n");
    fprintf(stderr, "\t\tdon't overwrite SPS timing info (default overwrite)\n");
    fprintf(stderr, "\t-f, --fifo\n");
    fprintf(stderr, "\t\tenable fifo output\n");
    fprintf(stderr, "\t-d, --debug\n");
    fprintf(stderr, "\t\tenable debug\n");
}

int main(int argc, char **argv) {
    unsigned char *buf_idx_1, *buf_idx_2;
    unsigned char *buf_idx_w, *buf_idx_tmp;
    unsigned char *buf_idx_start = NULL;
    int buf_idx_diff;
    FILE *fFS, *fFid, *fOut, *fOutLow, *fOutHigh;
    mode_t mode = 0755;

    int frame_res, frame_len;
    int frame_counter_low = -1;
    int frame_counter_high = -1;
    int frame_counter_last_valid_low = -1;
    int frame_counter_last_valid_high = -1;
    int frame_counter_invalid_low = 0;
    int frame_counter_invalid_high = 0;

    unsigned char *frame_header;

    int i, c;
    int write_enable = 0;
    int sps_sync = 0;

    resolution = RESOLUTION_HIGH;
    sps_timing_info = 1;
    fifo = 0;
    debug = 0;

    buf_offset = BUF_OFFSET_Y21GA;
    frame_header_size = FRAME_HEADER_SIZE_Y21GA;
    data_offset = DATA_OFFSET_Y21GA;
    lowres_byte = LOWRES_BYTE_Y21GA;
    highres_byte = HIGHRES_BYTE_Y21GA;

    while (1) {
        static struct option long_options[] =
        {
            {"model",  required_argument, 0, 'm'},
            {"resolution",  required_argument, 0, 'r'},
            {"sti",  no_argument, 0, 's'},
            {"fifo",  no_argument, 0, 'f'},
            {"debug",  no_argument, 0, 'd'},
            {"help",  no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "m:r:fsdh",
                         long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c) {
        case 'm':
            if (strcasecmp("y21ga", optarg) == 0) {
                buf_offset = BUF_OFFSET_Y21GA;
                frame_header_size = FRAME_HEADER_SIZE_Y21GA;
                data_offset = DATA_OFFSET_Y21GA;
                lowres_byte = LOWRES_BYTE_Y21GA;
                highres_byte = HIGHRES_BYTE_Y21GA;
            } else if (strcasecmp("y211ga", optarg) == 0) {
                buf_offset = BUF_OFFSET_Y211GA;
                frame_header_size = FRAME_HEADER_SIZE_Y211GA;
                data_offset = DATA_OFFSET_Y211GA;
                lowres_byte = LOWRES_BYTE_Y211GA;
                highres_byte = HIGHRES_BYTE_Y211GA;
            } else if (strcasecmp("h30ga", optarg) == 0) {
                buf_offset = BUF_OFFSET_H30GA;
                frame_header_size = FRAME_HEADER_SIZE_H30GA;
                data_offset = DATA_OFFSET_H30GA;
                lowres_byte = LOWRES_BYTE_H30GA;
                highres_byte = HIGHRES_BYTE_H30GA;
            } else if (strcasecmp("r30gb", optarg) == 0) {
                buf_offset = BUF_OFFSET_R30GB;
                frame_header_size = FRAME_HEADER_SIZE_R30GB;
                data_offset = DATA_OFFSET_R30GB;
                lowres_byte = LOWRES_BYTE_R30GB;
                highres_byte = HIGHRES_BYTE_R30GB;
            } else if (strcasecmp("r35gb", optarg) == 0) {
                buf_offset = BUF_OFFSET_R35GB;
                frame_header_size = FRAME_HEADER_SIZE_R35GB;
                data_offset = DATA_OFFSET_R35GB;
                lowres_byte = LOWRES_BYTE_R35GB;
                highres_byte = HIGHRES_BYTE_R30GB;
            } else if (strcasecmp("r40ga", optarg) == 0) {
                buf_offset = BUF_OFFSET_R40GA;
                frame_header_size = FRAME_HEADER_SIZE_R40GA;
                data_offset = DATA_OFFSET_R40GA;
                lowres_byte = LOWRES_BYTE_R40GA;
                highres_byte = HIGHRES_BYTE_R40GA;
            } else if (strcasecmp("h51ga", optarg) == 0) {
                buf_offset = BUF_OFFSET_H51GA;
                frame_header_size = FRAME_HEADER_SIZE_H51GA;
                data_offset = DATA_OFFSET_H51GA;
                lowres_byte = LOWRES_BYTE_H51GA;
                highres_byte = HIGHRES_BYTE_H51GA;
            } else if (strcasecmp("h52ga", optarg) == 0) {
                buf_offset = BUF_OFFSET_H52GA;
                frame_header_size = FRAME_HEADER_SIZE_H52GA;
                data_offset = DATA_OFFSET_H52GA;
                lowres_byte = LOWRES_BYTE_H52GA;
                highres_byte = HIGHRES_BYTE_H52GA;
            } else if (strcasecmp("h60ga", optarg) == 0) {
                buf_offset = BUF_OFFSET_H60GA;
                frame_header_size = FRAME_HEADER_SIZE_H60GA;
                data_offset = DATA_OFFSET_H60GA;
                lowres_byte = LOWRES_BYTE_H60GA;
                highres_byte = HIGHRES_BYTE_H60GA;
            } else if (strcasecmp("y28ga", optarg) == 0) {
                buf_offset = BUF_OFFSET_Y28GA;
                frame_header_size = FRAME_HEADER_SIZE_Y28GA;
                data_offset = DATA_OFFSET_Y28GA;
                lowres_byte = LOWRES_BYTE_Y28GA;
                highres_byte = HIGHRES_BYTE_Y28GA;
            } else if (strcasecmp("y29ga", optarg) == 0) {
                buf_offset = BUF_OFFSET_Y29GA;
                frame_header_size = FRAME_HEADER_SIZE_Y29GA;
                data_offset = DATA_OFFSET_Y29GA;
                lowres_byte = LOWRES_BYTE_Y29GA;
                highres_byte = HIGHRES_BYTE_Y29GA;
            } else if (strcasecmp("q321br_lsx", optarg) == 0) {
                buf_offset = BUF_OFFSET_Q321BR_LSX;
                frame_header_size = FRAME_HEADER_SIZE_Q321BR_LSX;
                data_offset = DATA_OFFSET_Q321BR_LSX;
                lowres_byte = LOWRES_BYTE_Q321BR_LSX;
                highres_byte = HIGHRES_BYTE_Q321BR_LSX;
            } else if (strcasecmp("qg311r", optarg) == 0) {
                buf_offset = BUF_OFFSET_QG311R;
                frame_header_size = FRAME_HEADER_SIZE_QG311R;
                data_offset = DATA_OFFSET_QG311R;
                lowres_byte = LOWRES_BYTE_QG311R;
                highres_byte = HIGHRES_BYTE_QG311R;
            } else if (strcasecmp("b091qp", optarg) == 0) {
                buf_offset = BUF_OFFSET_B091QP;
                frame_header_size = FRAME_HEADER_SIZE_B091QP;
                data_offset = DATA_OFFSET_B091QP;
                lowres_byte = LOWRES_BYTE_B091QP;
                highres_byte = HIGHRES_BYTE_B091QP;
            }
            break;

        case 'r':
            if (strcasecmp("low", optarg) == 0) {
                resolution = RESOLUTION_LOW;
            } else if (strcasecmp("high", optarg) == 0) {
                resolution = RESOLUTION_HIGH;
            } else if (strcasecmp("both", optarg) == 0) {
                resolution = RESOLUTION_BOTH;
            }
            break;

        case 's':
            sps_timing_info = 0;
            break;

        case 'f':
            fprintf (stderr, "using fifo as output\n");
            fifo = 1;
            break;

        case 'd':
            fprintf (stderr, "debug on\n");
            debug = 1;
            break;

        case 'h':
            print_usage(argv[0]);
            return -1;
            break;

        case '?':
            /* getopt_long already printed an error message. */
            break;

        default:
            print_usage(argv[0]);
            return -1;
        }
    }

    if ((fifo == 0) && (resolution == RESOLUTION_BOTH)) {
        fprintf(stderr, "Both resolution are not supported with output to stdout\n");
        fprintf(stderr, "Use fifo or run two processes\n");
        return -1;
    }

    fFS = fopen(BUFFER_FILE, "r");
    if ( fFS == NULL ) {
        fprintf(stderr, "could not get size of %s\n", BUFFER_FILE);
        return -1;
    }
    fseek(fFS, 0, SEEK_END);
    buf_size = ftell(fFS);
    fclose(fFS);
    if (debug) fprintf(stderr, "the size of the buffer is %d\n", buf_size);

    frame_header = (unsigned char *) malloc(frame_header_size * sizeof(unsigned char));

    // Opening an existing file
    fFid = fopen(BUFFER_FILE, "r") ;
    if ( fFid == NULL ) {
        fprintf(stderr, "error - could not open file %s\n", BUFFER_FILE) ;
        return -2;
    }

    // Map file to memory
    addr = (unsigned char*) mmap(NULL, buf_size, PROT_READ, MAP_SHARED, fileno(fFid), 0);
    if (addr == MAP_FAILED) {
        fprintf(stderr, "error - mapping file %s\n", BUFFER_FILE);
        fclose(fFid);
        return -3;
    }
    if (debug) fprintf(stderr, "mapping file %s, size %d, to %08x\n", BUFFER_FILE, buf_size, (unsigned int) addr);

    // Closing the file
    if (debug) fprintf(stderr, "closing the file %s\n", BUFFER_FILE) ;
    fclose(fFid) ;

    // Opening/setting output file
    if (fifo == 0) {
        char stdoutbuf[262144];

        if (setvbuf(stdout, stdoutbuf, _IOFBF, sizeof(stdoutbuf)) != 0) {
            fprintf(stderr, "Error setting stdout buffer\n");
        }
        if (resolution == RESOLUTION_LOW) {
            fOutLow = stdout;
        } else if (resolution == RESOLUTION_HIGH) {
            fOutHigh = stdout;
        }
    } else {
        pthread_t unlock_low_thread, unlock_high_thread;

        sigaction(SIGPIPE, &(struct sigaction){sigpipe_handler}, NULL);

        if ((resolution == RESOLUTION_LOW) || (resolution == RESOLUTION_BOTH)) {
            unlink(FIFO_NAME_LOW);
            if (mkfifo(FIFO_NAME_LOW, mode) < 0) {
                fprintf(stderr, "mkfifo failed for file %s\n", FIFO_NAME_LOW);
                return -4;
            }
            if(pthread_create(&unlock_low_thread, NULL, unlock_fifo_thread, (void *) FIFO_NAME_LOW)) {
                fprintf(stderr, "Error creating thread\n");
                return -4;
            }
            pthread_detach(unlock_low_thread);
            fOutLow = fopen(FIFO_NAME_LOW, "w");
            if (fOutLow == NULL) {
                fprintf(stderr, "Error opening fifo %s\n", FIFO_NAME_LOW);
                return -4;
            }
        }
        if ((resolution == RESOLUTION_HIGH) || (resolution == RESOLUTION_BOTH)) {
            unlink(FIFO_NAME_HIGH);
            if (mkfifo(FIFO_NAME_HIGH, mode) < 0) {
                fprintf(stderr, "mkfifo failed for file %s\n", FIFO_NAME_HIGH);
                return -4;
            }
            if(pthread_create(&unlock_high_thread, NULL, unlock_fifo_thread, (void *) FIFO_NAME_HIGH)) {
                fprintf(stderr, "Error creating thread\n");
                return -4;
            }
            pthread_detach(unlock_high_thread);
            fOutHigh = fopen(FIFO_NAME_HIGH, "w");
            if (fOutHigh == NULL) {
                fprintf(stderr, "Error opening fifo %s\n", FIFO_NAME_HIGH);
                return -4;
            }
        }
        fprintf(stderr, "fifo started\n");
    }

    memcpy(&i, addr + 16, sizeof(i));
    buf_idx_w = addr + buf_offset + i;
    buf_idx_1 = buf_idx_w;

    if (debug) fprintf(stderr, "starting capture main loop\n");

    // Infinite loop
    while (1) {
        memcpy(&i, addr + 16, sizeof(i));
        buf_idx_w = addr + buf_offset + i;
//        if (debug) fprintf(stderr, "buf_idx_w: %08x\n", (unsigned int) buf_idx_w);
        buf_idx_tmp = cb_memmem(buf_idx_1, buf_idx_w - buf_idx_1, NALx_START, sizeof(NALx_START));
        if (buf_idx_tmp == NULL) {
            usleep(MILLIS_25);
            continue;
        } else {
            buf_idx_1 = buf_idx_tmp;
        }
//        if (debug) fprintf(stderr, "found buf_idx_1: %08x\n", (unsigned int) buf_idx_1);

        buf_idx_tmp = cb_memmem(buf_idx_1 + 1, buf_idx_w - (buf_idx_1 + 1), NALx_START, sizeof(NALx_START));
        if (buf_idx_tmp == NULL) {
            usleep(MILLIS_25);
            continue;
        } else {
            buf_idx_2 = buf_idx_tmp;
        }
//        if (debug) fprintf(stderr, "found buf_idx_2: %08x\n", (unsigned int) buf_idx_2);

        if ((write_enable) && (sps_sync)) {
            if ((frame_res == RESOLUTION_LOW) && (resolution != RESOLUTION_HIGH)) {
                fOut = fOutLow;
            } else if ((frame_res == RESOLUTION_HIGH) && (resolution != RESOLUTION_LOW)) {
                fOut = fOutHigh;
            } else {
                fOut = NULL;
            }
            if (fOut != NULL) {
                if (sps_timing_info) {
                    if (cb_memcmp(SPS4_640X360, buf_idx_start, sizeof(SPS4_640X360)) == 0) {
                        fwrite(SPS4_640X360_TI, 1, sizeof(SPS4_640X360_TI), fOut);
                    } else if (cb_memcmp(SPS4_1920X1080, buf_idx_start, sizeof(SPS4_1920X1080)) == 0) {
                        fwrite(SPS4_1920X1080_TI, 1, sizeof(SPS4_1920X1080_TI), fOut);
                    } else if (cb_memcmp(SPS4_2304X1296, buf_idx_start, sizeof(SPS4_2304X1296)) == 0) {
                        fwrite(SPS4_2304X1296_TI, 1, sizeof(SPS4_2304X1296_TI), fOut);
                    } else if (cb_memcmp(SPS4_2_640X360, buf_idx_start, sizeof(SPS4_2_640X360)) == 0) {
                        fwrite(SPS4_2_640X360_TI, 1, sizeof(SPS4_2_640X360_TI), fOut);
                    } else if (cb_memcmp(SPS4_2_1920X1080, buf_idx_start, sizeof(SPS4_2_1920X1080)) == 0) {
                        fwrite(SPS4_2_1920X1080_TI, 1, sizeof(SPS4_2_1920X1080_TI), fOut);
                    } else if (cb_memcmp(SPS4_2_2304X1296, buf_idx_start, sizeof(SPS4_2_2304X1296)) == 0) {
                        fwrite(SPS4_2_2304X1296_TI, 1, sizeof(SPS4_2_2304X1296_TI), fOut);
                    } else if (cb_memcmp(SPS4_3_640X360, buf_idx_start, sizeof(SPS4_3_640X360)) == 0) {
                        fwrite(SPS4_3_640X360_TI, 1, sizeof(SPS4_3_640X360_TI), fOut);
                    } else if (cb_memcmp(SPS4_3_1920X1080, buf_idx_start, sizeof(SPS4_3_1920X1080)) == 0) {
                        fwrite(SPS4_3_1920X1080_TI, 1, sizeof(SPS4_3_1920X1080_TI), fOut);
                    } else if (cb_memcmp(VPS5_1920X1080, buf_idx_start, sizeof(VPS5_1920X1080)) == 0) {
                        fwrite(VPS5_1920X1080_TI, 1, sizeof(VPS5_1920X1080_TI), fOut);
                    } else {
                        if (buf_idx_start + frame_len > addr + buf_size) {
                            fwrite(buf_idx_start, 1, addr + buf_size - buf_idx_start, fOut);
                            fwrite(addr + buf_offset, 1, frame_len - (addr + buf_size - buf_idx_start), fOut);
                        } else {
                            fwrite(buf_idx_start, 1, frame_len, fOut);
                        }
                    }
                } else {
                    if (buf_idx_start + frame_len > addr + buf_size) {
                        fwrite(buf_idx_start, 1, addr + buf_size - buf_idx_start, fOut);
                        fwrite(addr + buf_offset, 1, frame_len - (addr + buf_size - buf_idx_start), fOut);
                    } else {
                        fwrite(buf_idx_start, 1, frame_len, fOut);
                    }
                }
                if (debug) fprintf(stderr, "%lld: writing frame, length %d\n", current_timestamp(), frame_len);
            }
        }

        if ((cb_memcmp(SPS4_START, buf_idx_1, sizeof(SPS4_START)) == 0) ||
                    (cb_memcmp(SPS5_START, buf_idx_1, sizeof(SPS5_START)) == 0)) {
            // SPS frame
            write_enable = 1;
            sps_sync = 1;
            buf_idx_1 = cb_move(buf_idx_1, - (6 + frame_header_size));
            cb2s_memcpy(frame_header, buf_idx_1, frame_header_size);
            buf_idx_1 = cb_move(buf_idx_1, 6 + frame_header_size);
            if (frame_header[17 + data_offset] == lowres_byte) {
                frame_res = RESOLUTION_LOW;
            } else if (frame_header[17 + data_offset] == highres_byte) {
                frame_res = RESOLUTION_HIGH;
            } else {
                frame_res = RESOLUTION_NONE;
            }
            if ((frame_res == RESOLUTION_LOW) && (resolution != RESOLUTION_HIGH)) {
                memcpy((unsigned char *) &frame_len, frame_header, 4);
                frame_len -= 6;                                                              // -6 only for SPS
                // Check if buf_idx_2 is greater than buf_idx_1 + frame_len
                buf_idx_diff = buf_idx_2 - buf_idx_1;
                if (buf_idx_diff < 0) buf_idx_diff += (buf_size - buf_offset);
                if (buf_idx_diff > frame_len) {
                    frame_counter_low = (int) frame_header[18 + data_offset] + (int) frame_header[19 + data_offset] * 256;
                    if ((frame_counter_low - frame_counter_last_valid_low > 20) ||
                                ((frame_counter_low < frame_counter_last_valid_low) && (frame_counter_low - frame_counter_last_valid_low > -65515))) {

                        if (debug) fprintf(stderr, "%lld: warning - incorrect frame counter - frame_counter: %d - frame_counter_last_valid: %d\n",
                                    current_timestamp(), frame_counter_low, frame_counter_last_valid_low);
                        frame_counter_invalid_low++;
                        // Check if sync is lost
                        if (frame_counter_invalid_low > 40) {
                            if (debug) fprintf(stderr, "%lld: error - sync lost\n", current_timestamp());
                            frame_counter_last_valid_low = frame_counter_low;
                            frame_counter_invalid_low = 0;
                            sps_sync = 0;
                        } else {
                            write_enable = 0;
                        }
                    } else {
                        if (debug) fprintf(stderr, "%lld: SPS   detected - frame_len: %d - frame_counter: %d - frame_counter_last_valid: %d - resolution: %d\n",
                                    current_timestamp(), frame_len, frame_counter_low,
                                    frame_counter_last_valid_low, frame_res);
                        frame_counter_invalid_low = 0;
                        frame_counter_last_valid_low = frame_counter_low;
                    }
                } else {
                    if (debug) fprintf(stderr, "%lld: warning - detected SPS with wrong size - buf_idx_diff: %d - frame_len: %d - frame_counter: %d - frame_counter_last_valid: %d - resolution: %d\n",
                                current_timestamp(), buf_idx_diff, frame_len, frame_counter_high,
                                frame_counter_last_valid_low, frame_res);
                    write_enable = 0;
                }

                buf_idx_start = buf_idx_1;
            } else if ((frame_res == RESOLUTION_HIGH) && (resolution != RESOLUTION_LOW)) {
                memcpy((unsigned char *) &frame_len, frame_header, 4);
                frame_len -= 6;                                                              // -6 only for SPS
                // Check if buf_idx_2 is greater than buf_idx_1 + frame_len
                buf_idx_diff = buf_idx_2 - buf_idx_1;
                if (buf_idx_diff < 0) buf_idx_diff += (buf_size - buf_offset);
                if (buf_idx_diff > frame_len) {
                    frame_counter_high = (int) frame_header[18 + data_offset] + (int) frame_header[19 + data_offset] * 256;
                    if ((frame_counter_high - frame_counter_last_valid_high > 20) ||
                                ((frame_counter_high < frame_counter_last_valid_high) && (frame_counter_high - frame_counter_last_valid_high > -65515))) {

                        if (debug) fprintf(stderr, "%lld: warning - incorrect frame counter - frame_counter: %d - frame_counter_last_valid: %d\n",
                                    current_timestamp(), frame_counter_high, frame_counter_last_valid_high);
                        frame_counter_invalid_high++;
                        // Check if sync is lost
                        if (frame_counter_invalid_high > 40) {
                            if (debug) fprintf(stderr, "%lld: error - sync lost\n", current_timestamp());
                            frame_counter_last_valid_high = frame_counter_high;
                            frame_counter_invalid_high = 0;
                            sps_sync = 0;
                        } else {
                            write_enable = 0;
                        }
                    } else {
                        if (debug) fprintf(stderr, "%lld: SPS   detected - frame_len: %d - frame_counter: %d - frame_counter_last_valid: %d - resolution: %d\n",
                                    current_timestamp(), frame_len, frame_counter_high,
                                    frame_counter_last_valid_high, frame_res);
                        frame_counter_invalid_high = 0;
                        frame_counter_last_valid_high = frame_counter_high;
                    }
                } else {
                    if (debug) fprintf(stderr, "%lld: warning - detected SPS with wrong size - buf_idx_diff: %d - frame_len: %d - frame_counter: %d - frame_counter_last_valid: %d - resolution: %d\n",
                                current_timestamp(), buf_idx_diff, frame_len, frame_counter_high,
                                frame_counter_last_valid_high, frame_res);
                    write_enable = 0;
                }

                buf_idx_start = buf_idx_1;
            } else {
                write_enable = 0;
//                if (debug & 1) fprintf(stderr, "%lld: warning - unexpected NALU header\n", current_timestamp());
            }
        } else if ((cb_memcmp(PPS4_START, buf_idx_1, sizeof(PPS4_START)) == 0) ||
                        (cb_memcmp(PPS5_START, buf_idx_1, sizeof(PPS5_START)) == 0) ||
                        (cb_memcmp(VPS5_START, buf_idx_1, sizeof(VPS5_START)) == 0) ||
                        (cb_memcmp(IDR4_START, buf_idx_1, sizeof(IDR4_START)) == 0) ||
                        (cb_memcmp(IDR5_START, buf_idx_1, sizeof(IDR5_START)) == 0) ||
                        (cb_memcmp(PFR4_START, buf_idx_1, sizeof(PFR4_START)) == 0) ||
                        (cb_memcmp(PFR5_START, buf_idx_1, sizeof(PFR5_START)) == 0)) {
            // PPS, VPS, IDR and PFR frames
            write_enable = 1;
            buf_idx_1 = cb_move(buf_idx_1, -frame_header_size);
            cb2s_memcpy(frame_header, buf_idx_1, frame_header_size);
            buf_idx_1 = cb_move(buf_idx_1, frame_header_size);
            if (frame_header[17 + data_offset] == lowres_byte) {
                frame_res = RESOLUTION_LOW;
            } else if (frame_header[17 + data_offset] == highres_byte) {
                frame_res = RESOLUTION_HIGH;
            } else {
                frame_res = RESOLUTION_NONE;
            }
            if ((frame_res == RESOLUTION_LOW) && (resolution != RESOLUTION_HIGH)) {
                memcpy((unsigned char *) &frame_len, frame_header, 4);
                // Check if buf_idx_2 is greater than buf_idx_1 + frame_len
                buf_idx_diff = buf_idx_2 - buf_idx_1;
                if (buf_idx_diff < 0) buf_idx_diff += (buf_size - buf_offset);
                if (buf_idx_diff > frame_len) {
                    frame_counter_low = (int) frame_header[18 + data_offset] + (int) frame_header[19 + data_offset] * 256;
                    if ((frame_counter_low - frame_counter_last_valid_low > 20) ||
                                ((frame_counter_low < frame_counter_last_valid_low) && (frame_counter_low - frame_counter_last_valid_low > -65515))) {

                        if (debug) fprintf(stderr, "%lld: warning - incorrect frame counter - frame_counter: %d - frame_counter_last_valid: %d\n",
                                    current_timestamp(), frame_counter_low, frame_counter_last_valid_low);
                        frame_counter_invalid_low++;
                        // Check if sync is lost
                        if (frame_counter_invalid_low > 40) {
                            if (debug) fprintf(stderr, "%lld: error - sync lost\n", current_timestamp());
                            frame_counter_last_valid_low = frame_counter_low;
                            frame_counter_invalid_low = 0;
                            sps_sync = 0;
                        } else {
                            write_enable = 0;
                        }
                    } else {
                        if (debug) fprintf(stderr, "%lld: frame detected - frame_len: %d - frame_counter: %d - frame_counter_last_valid: %d - resolution: %d\n",
                                    current_timestamp(), frame_len, frame_counter_low,
                                    frame_counter_last_valid_low, frame_res);
                        frame_counter_invalid_low = 0;
                        frame_counter_last_valid_low = frame_counter_low;
                    }
                } else {
                    if (debug) fprintf(stderr, "%lld: warning - detected frame with wrong size - buf_idx_diff: %d - frame_len: %d - frame_counter: %d - frame_counter_last_valid: %d - resolution: %d\n",
                                current_timestamp(), buf_idx_diff, frame_len, frame_counter_high,
                                frame_counter_last_valid_low, frame_res);
                    write_enable = 0;
                }

                buf_idx_start = buf_idx_1;
            } else if ((frame_res == RESOLUTION_HIGH) && (resolution != RESOLUTION_LOW)) {
                memcpy((unsigned char *) &frame_len, frame_header, 4);
                // Check if buf_idx_2 is greater than buf_idx_1 + frame_len
                buf_idx_diff = buf_idx_2 - buf_idx_1;
                if (buf_idx_diff < 0) buf_idx_diff += (buf_size - buf_offset);
                if (buf_idx_diff > frame_len) {
                    frame_counter_high = (int) frame_header[18 + data_offset] + (int) frame_header[19 + data_offset] * 256;
                    if ((frame_counter_high - frame_counter_last_valid_high > 20) ||
                                ((frame_counter_high < frame_counter_last_valid_high) && (frame_counter_high - frame_counter_last_valid_high > -65515))) {

                        if (debug) fprintf(stderr, "%lld: warning - incorrect frame counter - frame_counter: %d - frame_counter_last_valid: %d\n",
                                    current_timestamp(), frame_counter_high, frame_counter_last_valid_high);
                        frame_counter_invalid_high++;
                        // Check if sync is lost
                        if (frame_counter_invalid_high > 40) {
                            if (debug) fprintf(stderr, "%lld: error - sync lost\n", current_timestamp());
                            frame_counter_last_valid_high = frame_counter_high;
                            frame_counter_invalid_high = 0;
                            sps_sync = 0;
                        } else {
                            write_enable = 0;
                        }
                    } else {
                        if (debug) fprintf(stderr, "%lld: frame detected - frame_len: %d - frame_counter: %d - frame_counter_last_valid: %d - resolution: %d\n",
                                    current_timestamp(), frame_len, frame_counter_high,
                                    frame_counter_last_valid_high, frame_res);
                        frame_counter_invalid_high = 0;
                        frame_counter_last_valid_high = frame_counter_high;
                    }
                } else {
                    if (debug) fprintf(stderr, "%lld: warning - detected frame with wrong size - buf_idx_diff: %d - frame_len: %d - frame_counter: %d - frame_counter_last_valid: %d - resolution: %d\n",
                                current_timestamp(), buf_idx_diff, frame_len, frame_counter_high,
                                frame_counter_last_valid_high, frame_res);
                    write_enable = 0;
                }

                buf_idx_start = buf_idx_1;
            } else {
                write_enable = 0;
//                if (debug & 1) fprintf(stderr, "%lld: warning - unexpected NALU header\n", current_timestamp());
            }
        } else {
            write_enable = 0;
        }

        buf_idx_1 = buf_idx_2;
    }

    // Unreacheable path

    // Unmap file from memory
    if (munmap(addr, buf_size) == -1) {
        if (debug) fprintf(stderr, "error - unmapping file");
    } else {
        if (debug) fprintf(stderr, "unmapping file %s, size %d, from %08x\n", BUFFER_FILE, buf_size, (unsigned int) addr);
    }

    if (fifo == 1) {
        if ((resolution == RESOLUTION_LOW) || (resolution == RESOLUTION_BOTH)) {
            fclose(fOutLow);
            unlink(FIFO_NAME_LOW);
        }
        if ((resolution == RESOLUTION_HIGH) || (resolution == RESOLUTION_BOTH)) {
            fclose(fOutHigh);
            unlink(FIFO_NAME_HIGH);
        }
    }

    free(frame_header);

    return 0;
}
