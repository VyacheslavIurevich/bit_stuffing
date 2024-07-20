#include "protocol.h"
#include <inttypes.h>
#include <stdbool.h>
#define MARKER 0x7E

static uint8_t get_bit(uint8_t byte, uint8_t idx) {
  return (byte >> (8 - idx)) & 1u;
}

static uint8_t add_bit(uint8_t write_byte, uint8_t bit, uint8_t byte_len) {
  return (byte_len == 0) ? bit : 2 * write_byte + bit;
}

int read_message(FILE *stream, void *buf) {
  /* "what we have found" -> marker_flag
  nothing -> 0, 0 -> 1, 01 -> 2, 011 -> 3, 0111 -> 4, 01111 -> 5
  011111 -> 6, 0111111 -> 7 */
  uint8_t marker_flag = 0;
  /* counter of consecutive ones in the message
  we need it because there mustn't be 111111 in the message */
  uint8_t consecutive_ones_counter = 0;

  /* imagine that we place all bits from stream in a row and enumerate them
  first_marker_idx is index of the last bit of start marker
  end_marker_idx is index of the first bit of end marker
  cur_bit_num is a number of current bit */
  uint8_t start_marker_idx = 0, end_marker_idx = MAX_RECEIVE_LEN, cur_bit_num = 0;

  /* we will fill write_byte with new bits and ++byte_len
  until byte_len equals 8, then we write our byte
  into buffer and reset it */
  int8_t write_byte = 0;
  uint8_t byte_len = 0;

  // !!! flags to break while
  bool break_flag = false, maybe_break = false;

  uint8_t marker_counter = 0, bit, byte_cnt = 0, i = 0, payload_counter = 0;
  bool ignore_flag = false;
  int new_num = getc(stream);
  uint8_t new_byte, bytes[MAX_RECEIVE_LEN], *buffer = (uint8_t *)buf;
  if (new_num == EOF) {
    fprintf(stderr, "No message / troubles with reading \n");
    return EOF;
  }
  while ((new_num != EOF) && (break_flag == false)) {
    new_byte = (uint8_t)new_num;
    bytes[byte_cnt++] = new_byte;
    for (uint8_t bit_idx = 1; bit_idx <= 8; ++bit_idx) {
      bit = get_bit(new_byte, bit_idx);
      ++cur_bit_num;
      if (bit == 0) {
        // if there was no complete markers before this bit
        if (marker_counter == 0) {
          /* if marker_flag was 0, it becomes 1, because new marker starts with
          with 0 */
          if (marker_flag == 0)
            marker_flag = 1;
          // if 0 is after 0111111, the marker is complete
          else if (marker_flag == 7) {
            ++marker_counter;
            // current bit is the end of start marker
            start_marker_idx = cur_bit_num;
            // reset the flag to search for new marker
            marker_flag = 0;
          } else {
            // there is 0 before the start marker so it's incorrect message
            fprintf(stderr, "Incorrect message, 0 before the start marker \n");
            return EOF;
          }
          // if there was one complete marker before this bit
        } else if (marker_counter == 1) {
          // it may be a start of end marker or a part of message
          if (marker_flag == 0)
            marker_flag = 1;
          // if it ruins the end marker, it's just a part of message
          else if ((marker_flag >= 1) && (marker_flag <= 5))
            marker_flag = 1;
          // if it is after 5 1-bits we ignore it
          else if (marker_flag == 6)
            marker_flag = 0;
          // if it is after 0111111, the marker is complete
          else if (marker_flag == 7) {
            ++marker_counter;
            // first bit of marker is 7 bits before current one
            end_marker_idx = cur_bit_num - 7;
            // if we reached end of the byte we break
            if (cur_bit_num % 8 == 0)
              break_flag = true;
            /* if we found end marker but not at the end of byte
            we will look until the end of byte */
            else
              maybe_break = true;
          }
          consecutive_ones_counter = 0; // we met 0 so 111... ends with 0
        } else {
          // there is 0 after end marker so it's incorrect message
          fprintf(stderr, "Incorrect message, 0 after the end marker \n");
          return EOF;
        }
      }

      if (bit == 1) {
        if (marker_counter == 0) {
          /* if 1 is before start marker, we ignore it
          if 1 is after start0 but before end0, it can be a part of marker */
          if ((marker_flag >= 1) && (marker_flag <= 6))
            ++marker_flag;
          else if ((marker_flag == 7) || (marker_flag == 9)) {
            // there is 0 before start marker so it's incorrect message
            fprintf(stderr, "Incorrect message, 0 before the start marker \n");
            return EOF;
          }
        } else if (marker_counter == 1) {
          /* if 1 is before any 0, it can't be a part of a marker
          if 1 is after start0 but before end0, it can be a part of marker */
          if ((marker_flag >= 1) && (marker_flag <= 6))
            ++marker_flag;
          ++consecutive_ones_counter;
          /* if there are 6 consecutive ones, but no 0 before them, it's
          incorrect message */
          if ((consecutive_ones_counter == 6) && (marker_flag != 7)) {
            fprintf(stderr, "Incorrect message, 6 ones aren't divided by 0 \n");
            return EOF;
          }
        }
        // if 1 is after end marker, we ignore it
      }
    }
    /* if we wanted to break next byte and we reached the end of current
    - we break */
    if ((cur_bit_num % 8 == 0) && (maybe_break == true))
      break_flag = true;

    if (break_flag == false)
      new_num = getc(
          stream); // we read new byte only if we won't break next iteration
  }

  if (marker_counter == 0) {
    fprintf(stderr, "Incorrect message, no start marker \n");
    return EOF;
  } else if (marker_counter == 1) {
    fprintf(stderr, "Incorrect message, no end marker \n");
    return EOF;
  }

  cur_bit_num = 0;
  consecutive_ones_counter = 0;
  for (uint8_t byte_idx = 0; byte_idx < byte_cnt; ++byte_idx) {
    for (uint8_t bit_idx = 1; bit_idx <= 8; ++bit_idx) {
      bit = get_bit(bytes[byte_idx], bit_idx);
      ++cur_bit_num;
      if (bit == 1)
        ++consecutive_ones_counter;
      if (bit == 0) {
        if (consecutive_ones_counter == 5)
          ignore_flag = true;
        consecutive_ones_counter = 0;
      }
      if (ignore_flag == false) {
        if ((cur_bit_num > start_marker_idx) &&
            (cur_bit_num < end_marker_idx)) {
          ++payload_counter;
          if (byte_len < 8) {
            write_byte = add_bit(write_byte, bit, byte_len);
            ++byte_len;
          }
        }
        if (byte_len == 8) {
          buffer[i++] = write_byte;
          write_byte = 0;
          byte_len = 0;
        }
      } else
        ignore_flag = false;
    }
  }
  if (payload_counter % 8 != 0) {
    fprintf(stderr,
            "Incorrect message, non-integer count of bits in payload \n");
    return EOF;
  } else
    return payload_counter / 8;
}

int write_message(FILE *stream, const void *buf, size_t nbyte) {
  uint8_t bit, consecutive_ones_counter = 0, byte_len = 0, zero_counter = 0,
               marker_counter = 0;
  uint8_t write_byte = 0, byte = 0, *buffer = (uint8_t *)buf;

  // start marker
  if (putc(MARKER, stream) == EOF) {
    fprintf(stderr, "Can't write in file \n");
    return EOF;
  }

  for (uint8_t byte_idx = 0; byte_idx < nbyte; ++byte_idx) {
    byte = buffer[byte_idx];
    for (uint8_t bit_idx = 1; bit_idx <= 8; ++bit_idx) {
      bit = get_bit(byte, bit_idx);
      if (bit == 0) {
        consecutive_ones_counter = 0;
      } else {
        ++consecutive_ones_counter;

        // after 11111 we put 0
        if (consecutive_ones_counter == 5) {
          if (byte_len < 8) {
            // write fifth 1 then write 0
            write_byte = add_bit(write_byte, bit, byte_len);
            bit = 0;
            ++byte_len;
            ++zero_counter;
          }
          if (byte_len == 8) {
            if (putc(write_byte, stream) == EOF) {
              fprintf(stderr, "Can't write in file \n");
              return EOF;
            }
            write_byte = 0;
            byte_len = 0;
          }
          consecutive_ones_counter = 0; // reset the counter
        }
      }

      if (byte_len < 8) {
        write_byte = add_bit(write_byte, bit, byte_len);
        ++byte_len;
      }
      if (byte_len == 8) {
        if (putc(write_byte, stream) == EOF) {
          fprintf(stderr, "Can't write in file \n");
          return EOF;
        }
        write_byte = 0;
        byte_len = 0;
      }
    }
  }
  // if we put additional 8k zeros, we can put end marker itself
  if (zero_counter % 8 == 0) {
    // full end marker
    if (putc(MARKER, stream) == EOF) {
      fprintf(stderr, "Can't write in file \n");
      return EOF;
    }
  } else {
    // write as much end marker bit as we can
    for (uint8_t bit_idx = 0; bit_idx < 8 - (zero_counter % 8); ++bit_idx) {
      if (byte_len < 8) {
        // if it is first bit of marker, it is 0, else 1
        bit = marker_counter == 0 ? 0 : 1;
        write_byte = add_bit(write_byte, bit, byte_len);
        ++byte_len;
        ++marker_counter;
      }
      if (byte_len == 8) {
        if (putc(write_byte, stream) == EOF) {
          fprintf(stderr, "Can't write in file \n");
          return EOF;
        }
        write_byte = 0;
        byte_len = 0;
      }
    }
    // write remaining bits and fill with 1 to the end
    for (uint8_t bit_idx = 0; bit_idx < 8; ++bit_idx) {
      if (byte_len < 8) {
        // if it is last bit of marker, it is 0, else 1
        bit = marker_counter == 7 ? 0 : 1;
        write_byte = add_bit(write_byte, bit, byte_len);
        ++byte_len;
        ++marker_counter;
      }
      if (byte_len == 8) {
        if (putc(write_byte, stream) == EOF) {
          fprintf(stderr, "Can't write in file \n");
          return EOF;
        }
      }
    }
  }
  return nbyte;
}
