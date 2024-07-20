#pragma once
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#define MAX_MESSAGE_LEN 256
/* maximum length of message that we receive from stream
it consists of 7 prefix bits, 7 postfix bits, 2 markers
and 011111010 (which should be read as 01111110) repeated
(MAX_MESSAGE_LEN / 8) times */
#define MAX_RECEIVE_LEN ((7 * 2 + 8 * 2 + ((MAX_MESSAGE_LEN / 8) + 1) * 8) / 8)
#define MARKER 0x7E

/* this function gets bit from the byte with index idx.
we have one byte, enumerated from left to right
example: 12345678 - indices
         00100000 - bits
get_bit(00100000, 3) returns 1 */
static uint8_t get_bit(uint8_t byte, uint8_t idx);

/* this procedure glues new bit
on the right side of num
example: add_bit(5, 1) makes 11
which is equal to 1011, when
5 is equal to 101 */
static uint8_t add_bit(uint8_t write_byte, uint8_t bit, uint8_t byte_len);

int read_message(FILE *stream, void *buf);

int write_message(FILE *stream, const void *buf, size_t nbyte);
