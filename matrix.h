#ifndef _MATRIX_H_
#define _MATRIX_H_

#include <stddef.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

  /** 
      Upper triangular matrix with diagonal. Backed by bitmap_t.
      Rows and columns counted from 1.
      
          1 2 3 4
          x----->
      1 y 1 2 3 4
      2 |   6 7 8
      3 |     0 9
      4 v       3
     
  */
  struct _trimatrix_t;
  typedef struct _trimatrix_t trimatrix_t;
  
  extern trimatrix_t * trimatrix_new (unsigned n);
  extern trimatrix_t * trimatrix_init (trimatrix_t * mx, unsigned n);
  extern void trimatrix_delete (trimatrix_t * mx);
  extern void trimatrix_destruct (trimatrix_t * mx);
  extern trimatrix_t * trimatrix_clone (const trimatrix_t * mx);
  extern int trimatrix_get (const trimatrix_t * mx, unsigned x, unsigned y);
  extern int trimatrix_set (const trimatrix_t * mx, 
                            unsigned x, unsigned y, int val);
  extern size_t trimatrix_serialize_size (const trimatrix_t * mx);
  extern trimatrix_t * trimatrix_deserialize (const void * buf,
                                              size_t * pos);
  extern void trimatrix_serialize (void * buf, size_t * size, size_t * pos,
                                   const trimatrix_t * mx);
  

  /**
     Upper triangular matrix with diagonal with elements of type 
     unsigned char.
  */
  struct _wtrimatrix_t;
  typedef struct _wtrimatrix_t wtrimatrix_t;

  extern wtrimatrix_t * wtrimatrix_new (unsigned n);
  extern wtrimatrix_t * wtrimatrix_init (wtrimatrix_t * mx, unsigned n);
  extern void wtrimatrix_delete (wtrimatrix_t * mx);
  extern void wtrimatrix_destruct (wtrimatrix_t * mx);
  extern wtrimatrix_t * wtrimatrix_clone (const wtrimatrix_t * mx);
  extern unsigned wtrimatrix_get (const wtrimatrix_t * mx, 
                                  unsigned x, unsigned y);
  extern unsigned wtrimatrix_set (const wtrimatrix_t * mx, 
                                  unsigned x, unsigned y, unsigned char val);
  extern size_t wtrimatrix_serialize_size (const wtrimatrix_t * mx);
  extern wtrimatrix_t * wtrimatrix_deserialize (const void * buf,
                                                size_t * pos);
  extern void wtrimatrix_serialize (void * buf, size_t * size, size_t * pos,
                                    const wtrimatrix_t * mx);

#ifdef __cplusplus
}
#endif

#endif
