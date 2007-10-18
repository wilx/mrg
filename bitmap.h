/*
Copyright (c) 1997-2007, Václav Haisman

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef _BITMAP_H_
#define _BITMAP_H_

#include <stdio.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

  struct _bitmap_t;
  /**
     Bitmap, indexed from 0.
  */
  typedef struct _bitmap_t bitmap_t;

  /* Interface for bitmap_t. */
  extern bitmap_t * bitmap_new (unsigned size);
  extern bitmap_t * bitmap_init (bitmap_t * bm, unsigned size);
  extern void bitmap_delete (bitmap_t * bm);
  extern void bitmap_destruct (bitmap_t * bm);
  extern bitmap_t * bitmap_clone (const bitmap_t * bm);
  extern bitmap_t * bitmap_resize (bitmap_t * bm, unsigned size);
  extern int bitmap_setbit (const bitmap_t * bm, unsigned pos);
  extern int bitmap_clrbit (const bitmap_t * bm, unsigned pos);
  extern int bitmap_putbit (const bitmap_t * bm, unsigned pos, int val);
  extern int bitmap_getbit (const bitmap_t * bm, unsigned pos);
  extern int bitmap_flipbit (const bitmap_t * bm, unsigned pos);
  extern bitmap_t * bitmap_clear (bitmap_t * bm);
  extern bitmap_t * bitmap_set (bitmap_t * bm);
  extern bitmap_t * bitmap_flip (bitmap_t * bm);
  extern unsigned bitmap_size (const bitmap_t * bm);
  extern int bitmap_print (const bitmap_t * bm, FILE * stream, 
                           const char * sep);
  extern size_t bitmap_serialize_size (const bitmap_t * bm);
  extern void bitmap_serialize (void * buf, size_t size, size_t * pos, 
                                bitmap_t * bm);
  extern bitmap_t * bitmap_deserialize (void * buf, size_t insize, size_t * pos);
  
  
#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
