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
#include <stdlib.h>
#include <time.h>
#include "bitmap.h"

int main (void)
{
  bitmap_t * bm = bitmap_new (33);
  if (! bm)
    abort ();

  srandom (time (NULL));

  /* All bits should be 0 after initialization. */
  bitmap_print (bm, stdout, " ");
  printf ("\n");
  
  {
    unsigned i;
    for (i = 0; i < bitmap_size (bm); ++i)
      if (bitmap_getbit (bm, i))
        abort ();
  }

  bitmap_set (bm);
  /* All bits should be 1. */
  bitmap_print (bm, stdout, " ");
  printf ("\n");

  {
    unsigned i;
    for (i = 0; i < bitmap_size (bm); ++i)
      if (! bitmap_getbit (bm, i))
        abort ();
  }

  bitmap_clear (bm);
  /* Again all bits should be 0. */
  bitmap_print (bm, stdout, " ");
  printf ("\n");

  /* Randomize bits in bitmap. */
  {
    unsigned i;
    for (i = 0; i < bitmap_size (bm); ++i)
      bitmap_putbit (bm, i, random () % 2);
  }

  bitmap_print (bm, stdout, " ");
  printf ("\n");

  /* Check if bitmap_flip() works. */
  {
    unsigned i;
    bitmap_t * clone = bitmap_clone (bm);
    if (bitmap_size (clone) != bitmap_size (bm))
      abort ();
    bitmap_flip (clone);
    bitmap_print (clone, stdout, " ");
    printf ("\n");
    for (i = 0; i < bitmap_size (clone); ++i)
      if (bitmap_getbit (clone, i) == bitmap_getbit (bm, i))
        abort ();
    bitmap_delete (clone);
  }

  /* Check enlarging. */
  {
    bitmap_t * ret;
    bitmap_set (bm);
    bitmap_print (bm, stdout, " ");
    printf ("\n");
    ret = bitmap_resize (bm, 43);
    if (! ret)
      abort ();
    bitmap_print (bm, stdout, " ");
    printf ("\n");
  }

  /* Check shrinking. */
  {
    bitmap_t * ret;
    bitmap_set (bm);
    bitmap_print (bm, stdout, " ");
    printf ("\n");
    ret = bitmap_resize (bm, 21);
    if (! ret)
      abort ();
    bitmap_print (bm, stdout, " ");
    printf ("\n");
  }

  /* Check serialization. */
  {
    bitmap_t * copy;
    void * buf;
    size_t sz = bitmap_serialize_size (bm),
      size, pos = 0;
    buf = malloc (sz);
    if (! sz)
      abort ();
    bitmap_serialize (buf, &size, &pos, bm);
    pos = 0;
    copy = bitmap_deserialize (buf, &pos);
    bitmap_print (copy, stdout, " ");
    printf ("\n");
    bitmap_delete (copy);
  }

  bitmap_delete (bm);
  return 0;
}
