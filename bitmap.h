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
