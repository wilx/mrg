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
