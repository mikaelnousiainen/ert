/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include "ert-fileutil.h"
#include "ert-log.h"

/* File must be open with 'b' in the mode parameter to fopen() */
off_t fsize(FILE *fs)
{
  off_t offset, offset2;
  int result;

  if (fseeko(fs, 0, SEEK_SET) != 0 ||
      fgetc(fs) == EOF)
    return 0;

  offset = 1;

  while ((result = fseeko(fs, offset, SEEK_SET)) == 0 &&
         (result = (fgetc(fs) == EOF)) == 0 &&
         offset <= INT32_MAX / 4 + 1)
    offset *= 2;

  /* If the last seek failed, back up to the last successfully seekable offset */
  if (result != 0)
    offset /= 2;

  for (offset2 = offset / 2; offset2 != 0; offset2 /= 2)
    if (fseeko(fs, offset + offset2, SEEK_SET) == 0 &&
        fgetc(fs) != EOF)
      offset += offset2;

  /* Return -1 for files longer than INT32_MAX */
  if (offset == INT32_MAX)
    return -1;

  return offset + 1;
}

/* File must be open with 'b' in the mode parameter to fopen() */
/* Set file position to size of file before reading last line of file */
size_t fgetsr(FILE *fs, size_t n, char *buf)
{
  off_t fpos;
  size_t bufpos;
  bool first = true;

  if (n <= 1 || (fpos = ftello(fs)) == -1 || fpos == 0) {
    return 0;
  }

  bufpos = n - 1;
  buf[bufpos] = '\0';

  while (true) {
    int c;

    if (fseeko(fs, --fpos, SEEK_SET) != 0 || (c = fgetc(fs)) == EOF) {
      return 0;
    }

    if (c == '\n' && !first) { /* accept at most one '\n' */
      break;
    }
    first = false;

    if (c != '\r' && c != '\n') {
      uint8_t ch = (uint8_t) c;
      if (bufpos == 0)
      {
        memmove(buf + 1, buf, n - 2);
        ++bufpos;
      }
      memcpy(buf + --bufpos, &ch, 1);
    }

    if (fpos == 0) {
      fseeko(fs, 0, SEEK_SET);
      break;
    }
  }

  memmove(buf, buf + bufpos, n - bufpos);

  return n - bufpos - 1;
}

/* File must be open with 'b' in the mode parameter to fopen() */
/* Set file position to size of file before reading last line of file */
size_t fgetbr(FILE *fs, size_t n, char *buf)
{
  off_t fpos;
  size_t bufpos;
  size_t index;

  if (n <= 1 || (fpos = ftello(fs)) == -1 || fpos == 0) {
    return 0;
  }

  bufpos = n;
  index = 0;

  while (true) {
    int c;

    if (fseeko(fs, --fpos, SEEK_SET) != 0 || (c = fgetc(fs)) == EOF) {
      return 0;
    }

    if (index >= (n - 1)) {
      break;
    }

    uint8_t ch = (uint8_t) c;
    memcpy(buf + --bufpos, &ch, 1);
    index++;

    if (fpos == 0) {
      fseeko(fs, 0, SEEK_SET);
      break;
    }
  }

  memmove(buf, buf + bufpos, n - bufpos);

  return n - bufpos;
}

ssize_t fcopy(FILE *source, FILE *dest)
{
  size_t bytes_read;
  ssize_t total_bytes_read = 0;
  char buffer[BUFSIZ];

  while ((bytes_read = fread(buffer, sizeof(char), sizeof(buffer), source)) > 0) {
    if (fwrite(buffer, sizeof(char), bytes_read, dest) != bytes_read) {
      return -EIO;
    }
    total_bytes_read += bytes_read;
  }

  return total_bytes_read;
}

ssize_t fcopyn(char *source_filename, char *dest_filename)
{
  FILE *source = fopen(source_filename, "rb");
  if (source == NULL) {
    ert_log_error("Error opening source file '%s': %s", source_filename, strerror(errno));
    return -ENOENT;
  }

  FILE *dest = fopen(dest_filename, "wb");
  if (dest == NULL) {
    fclose(source);
    ert_log_error("Error opening dest file '%s': %s", source_filename, strerror(errno));
    return -ENOENT;
  }

  ssize_t result = fcopy(source, dest);

  fclose(dest);
  fclose(source);

  return result;
}
