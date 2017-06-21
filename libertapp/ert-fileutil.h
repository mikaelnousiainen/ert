/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_FILEUTIL_H
#define __ERT_FILEUTIL_H

off_t fsize(FILE *fs);
size_t fgetsr(FILE *fs, size_t n, char *buf);
size_t fgetbr(FILE *fs, size_t n, char *buf);
ssize_t fcopy(FILE *source, FILE *dest);
ssize_t fcopyn(char *source_filename, char *dest_filename);

#endif
