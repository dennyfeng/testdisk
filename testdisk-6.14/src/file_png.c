/*

    File: file_png.c

    Copyright (C) 1998-2009 Christophe GRENIER <grenier@cgsecurity.org>
    Thanks to Holger Klemm for JNG (JPEG Network Graphics) and
    MNG (Multiple-Image Network Graphics) support (2006)
  
    This software is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
  
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
  
    You should have received a copy of the GNU General Public License along
    with this program; if not, write the Free Software Foundation, Inc., 51
    Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

 */
/*
   MNG (Multiple-image Network Graphics) Format
   http://www.libpng.org/pub/mng/spec/
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <stdio.h>
#include <ctype.h>
#include "types.h"
#include "filegen.h"

extern const file_hint_t file_hint_doc;

static void register_header_check_png(file_stat_t *file_stat);
static int header_check_png(const unsigned char *buffer, const unsigned int buffer_size, const unsigned int safe_header_only, const file_recovery_t *file_recovery, file_recovery_t *file_recovery_new);
static int data_check_png(const unsigned char *buffer, const unsigned int buffer_size, file_recovery_t *file_recovery);
static int data_check_mng(const unsigned char *buffer, const unsigned int buffer_size, file_recovery_t *file_recovery);

const file_hint_t file_hint_png= {
  .extension="png",
  .description="Portable/JPEG/Multiple-Image Network Graphics",
  .min_header_distance=0,
  .max_filesize=PHOTOREC_MAX_FILE_SIZE,
  .recover=1,
  .enable_by_default=1,
  .register_header_check=&register_header_check_png
};

static const unsigned char png_header[8]= { 0x89, 'P', 'N','G', 0x0d, 0x0a, 0x1a, 0x0a};
static const unsigned char mng_header[8]= { 0x8a, 'M', 'N','G', 0x0d, 0x0a, 0x1a, 0x0a};
static const unsigned char jng_header[8]= { 0x8b, 'J', 'N','G', 0x0d, 0x0a, 0x1a, 0x0a};

static void register_header_check_png(file_stat_t *file_stat)
{
  register_header_check(0, png_header,sizeof(png_header), &header_check_png, file_stat);
  register_header_check(0, mng_header,sizeof(mng_header), &header_check_png, file_stat);
  register_header_check(0, jng_header,sizeof(jng_header), &header_check_png, file_stat);
}

static int header_check_png(const unsigned char *buffer, const unsigned int buffer_size, const unsigned int safe_header_only, const file_recovery_t *file_recovery, file_recovery_t *file_recovery_new)
{
  if(memcmp(buffer,jng_header,sizeof(jng_header))==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->extension="jng";
    file_recovery_new->calculated_file_size=8;
    file_recovery_new->data_check=&data_check_png;
    file_recovery_new->file_check=&file_check_size;
    return 1;
  }
  if(memcmp(buffer,mng_header,sizeof(mng_header))==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->extension="mng";
    file_recovery_new->calculated_file_size=8;
    file_recovery_new->data_check=&data_check_mng;
    file_recovery_new->file_check=&file_check_size;
    return 1;
  }
  /* SolidWorks files contains a png */
  if(file_recovery!=NULL && file_recovery->file_stat!=NULL &&
      file_recovery->file_stat->file_hint==&file_hint_doc &&
      (strcmp(file_recovery->extension,"sld")==0 ||
       strcmp(file_recovery->extension,"sldprt")==0))
    return 0;
  if(memcmp(buffer,png_header,sizeof(png_header))==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->extension=file_hint_png.extension;
    file_recovery_new->calculated_file_size=8;
    file_recovery_new->data_check=&data_check_png;
    file_recovery_new->file_check=&file_check_size;
    return 1;
  }
  return 0;
}

static int data_check_mng(const unsigned char *buffer, const unsigned int buffer_size, file_recovery_t *file_recovery)
{
  static const unsigned char mng_footer[4]= {'M','E','N','D'};
  while(file_recovery->calculated_file_size + buffer_size/2  >= file_recovery->file_size &&
      file_recovery->calculated_file_size + 8 < file_recovery->file_size + buffer_size/2)
  {
    const unsigned int i=file_recovery->calculated_file_size - file_recovery->file_size + buffer_size/2;
    /* chunk: length, type, data, crc */
    file_recovery->calculated_file_size+=(buffer[i]<<24)+(buffer[i+1]<<16)+(buffer[i+2]<<8)+buffer[i+3];
    file_recovery->calculated_file_size+=12;
    if(memcmp(&buffer[i+4], mng_footer, sizeof(mng_footer))==0)
      return 2;
  }
  return 1;
}

static int data_check_png(const unsigned char *buffer, const unsigned int buffer_size, file_recovery_t *file_recovery)
{
  while(file_recovery->calculated_file_size + buffer_size/2  >= file_recovery->file_size &&
      file_recovery->calculated_file_size + 8 < file_recovery->file_size + buffer_size/2)
  {
    const unsigned int i=file_recovery->calculated_file_size - file_recovery->file_size + buffer_size/2;
    /* chunk: length, type, data, crc */
    file_recovery->calculated_file_size+=(buffer[i]<<24)+(buffer[i+1]<<16)+(buffer[i+2]<<8)+buffer[i+3];
    file_recovery->calculated_file_size+=12;
    if(memcmp(&buffer[i+4], "IEND", 4)==0)
      return 2;
// PNG chunk code
// IDAT IHDR PLTE bKGD cHRM fRAc gAMA gIFg gIFt gIFx hIST iCCP
// iTXt oFFs pCAL pHYs sBIT sCAL sPLT sRGB sTER tEXt tRNS zTXt
    if( !((isupper(buffer[i+4]) || islower(buffer[i+4])) &&
	  (isupper(buffer[i+5]) || islower(buffer[i+5])) &&
	  (isupper(buffer[i+6]) || islower(buffer[i+6])) &&
	  (isupper(buffer[i+7]) || islower(buffer[i+7]))))
    {
      return 2;
    }
  }
  return 1;
}

