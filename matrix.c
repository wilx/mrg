#include "matrix.h"
#include "bitmap.h"
#include <stdlib.h>
#include <string.h>


struct _trimatrix_t
{
  bitmap_t * bm;
  unsigned n;
};


/**
   Computes how many elements of storage is needed for upper
   triangular matrix of width/height n.
   @param n width of matrix
   @return number of elements of storage for matrix
*/
static inline
unsigned
elems_from_n (unsigned n)
{
  return (unsigned)((n * (n + 1ull)) / 2);
}


static inline
unsigned
index_for_nxy (unsigned _n, unsigned x, unsigned y)
{
  const unsigned long n = _n;

  if (y > x)
    {
      unsigned tmp = x;
      x = y;
      y = tmp;
    }
  return (unsigned)(((y - 1) * (n + (n - (y-1-1)))) / 2
                    + (x - (y - 1)) - 1);
}


/**
   Initializes trimatrix_t of edge size n.
   @param mx matrix
   @param n height/width of matrix
   @return initialized matrix
*/
inline
trimatrix_t *
trimatrix_init (trimatrix_t * mx, unsigned n)
{
  if (n == 0)
    abort ();
  mx->bm = bitmap_new (elems_from_n (n));
  if (! mx->bm)
    return NULL;
  mx->n = n;
  return mx;
}


/**
   Allocates and initializes matrix.
   @param n height/width of matrix
   @return matrix
*/
trimatrix_t *
trimatrix_new (unsigned n)
{
  trimatrix_t * mx;

  if (n == 0)
    abort ();
  mx = malloc (sizeof (trimatrix_t));
  if (! mx)
    return NULL;
  if (! trimatrix_init (mx, n))
    {
      free (mx);
      return NULL;
    }
  else
    return mx;
}


/**
   Destructor of matrix.
   @param mx matrix
*/
inline
void 
trimatrix_destruct (trimatrix_t * mx)
{
  bitmap_destruct (mx->bm);
}


/**
   Destructs matrix and frees allocated memory.
   @param mx matrix
 */
void
trimatrix_delete (trimatrix_t * mx)
{
  trimatrix_destruct (mx);
  free (mx);
}


/**
   Duplicates matrix
   @param mx matrix to clone
   @return clone of first parameter
*/
trimatrix_t * 
trimatrix_clone (const trimatrix_t * mx)
{
  trimatrix_t * newmx;
  
  newmx = malloc (sizeof (trimatrix_t));
  if (! newmx)
    return NULL;
  newmx->bm = bitmap_clone (mx->bm);
  if (! newmx->bm)
    {
      free (newmx);
      return NULL;
    }
  newmx->n = mx->n;
  return newmx;
}


inline
int 
trimatrix_get (const trimatrix_t * mx, unsigned x, unsigned y)
{
  if (x > mx->n || y > mx->n
      || x == 0 || y == 0)
    abort ();
  return bitmap_getbit (mx->bm, index_for_nxy (mx->n, x, y));
}


/**
   Sets a value to an element at position (x,y) in matrix.
   @param mx matrix
   @param x X coordinate
   @param y Y coordinate
   @param val value
   @return previous value
*/
inline
int
trimatrix_set (const trimatrix_t * mx, unsigned x, unsigned y, int val)
{
  if (x > mx->n || y > mx->n 
      || x == 0 || y == 0)
    abort ();
  return bitmap_putbit (mx->bm, index_for_nxy (mx->n, x, y), val);
}


/**
   Returns how many bytes are needed to serialize trimatrix.
*/
size_t 
trimatrix_serialize_size (const trimatrix_t * mx)
{
  return bitmap_serialize_size (mx->bm) 
    + sizeof (unsigned);
}


/**
   Serializes trimatrix into a buffer buf.
*/
void 
trimatrix_serialize (void * buf, size_t * size, size_t * pos,
                     const trimatrix_t * mx)
{
  *(unsigned *)(buf + *pos) = mx->n;
  *pos += sizeof (unsigned);
  
  bitmap_serialize (buf, size, pos, mx->bm);
  *size += sizeof (unsigned);
}

  
/**
   Reconstructs trimatrix out of serialized represntation.
*/
trimatrix_t * 
trimatrix_deserialize (const void * buf, size_t * pos)
{
  trimatrix_t * mx;
  size_t p = *pos;

  mx = malloc (sizeof (trimatrix_t));
  if (! mx)
    return NULL;
  mx->n = *(unsigned *)(buf + p);
  p += sizeof (unsigned);
  
  mx->bm = bitmap_deserialize (buf, &p);
  if (! mx->bm)
    {
      free (mx);
      return NULL;
    }
  *pos = p;
  
  return mx;
}


/**

*/
struct _wtrimatrix_t
{
  unsigned char * buf;
  unsigned n;
};


wtrimatrix_t * 
wtrimatrix_init (wtrimatrix_t * mx, unsigned n)
{
  if (n == 0)
    abort ();
  mx->buf = malloc (elems_from_n (n));
  if (! mx->buf)
    return NULL;
  memset (mx->buf, 0, elems_from_n (n));
  mx->n = n;
  return mx;
}


wtrimatrix_t * 
wtrimatrix_new (unsigned n)
{
  wtrimatrix_t * mx;
  
  if (n == 0)
    abort ();
  mx = malloc (sizeof (wtrimatrix_t));
  if (! mx)
    return NULL;
  if (! wtrimatrix_init (mx, n))
    {
      free (mx);
      return NULL;
    }
  else
    return mx;
}


inline
void 
wtrimatrix_destruct (wtrimatrix_t * mx)
{
  free (mx->buf);
}


void 
wtrimatrix_delete (wtrimatrix_t * mx)
{
  wtrimatrix_destruct (mx);
  free (mx);
}


wtrimatrix_t * 
wtrimatrix_clone (const wtrimatrix_t * mx)
{
  wtrimatrix_t * newmx;
  
  newmx = malloc (sizeof (wtrimatrix_t));
  if (! newmx)
    return NULL;
  newmx->buf = malloc (elems_from_n (mx->n));
  if (! newmx->buf)
    {
      free (newmx);
      return NULL;
    }
  newmx->n = mx->n;
  memcpy (newmx->buf, mx->buf, elems_from_n (newmx->n));
  
  return newmx;
}


unsigned 
wtrimatrix_get (const wtrimatrix_t * mx, unsigned x, unsigned y)
{
  if (x > mx->n || y > mx->n
      || x == 0 || y == 0)
    abort ();

  return (unsigned)mx->buf[index_for_nxy (mx->n, x, y)];
}


unsigned 
wtrimatrix_set (const wtrimatrix_t * mx, unsigned x, unsigned y, unsigned char val)
{
  const unsigned i = index_for_nxy (mx->n, x, y);
  const unsigned prev = mx->buf[i];

  if (x > mx->n || y > mx->n
      || x == 0 || y == 0)
    abort ();
  
  mx->buf[i] = val;
  return prev;
}


size_t 
wtrimatrix_serialize_size (const wtrimatrix_t * mx)
{
  return sizeof (unsigned) + elems_from_n (mx->n);
}


void 
wtrimatrix_serialize (void * buf, size_t * size, size_t * pos,
                      const wtrimatrix_t * mx)
{
  unsigned sz = elems_from_n (mx->n);

  *(unsigned *)(buf + *pos) = mx->n;
  *pos += sizeof (unsigned);
  
  memcpy (buf + *pos, mx->buf, sz);

  *pos += sz;
  *size = sz + sizeof (unsigned);
}


wtrimatrix_t * 
wtrimatrix_deserialize (const void * buf, size_t * pos)
{
  size_t p = *pos;
  wtrimatrix_t * mx;
  unsigned elems;

  mx = malloc (sizeof (wtrimatrix_t));
  if (! mx)
    return NULL;
  mx->n = *(unsigned *)buf;
  p += sizeof (unsigned);

  elems = elems_from_n (mx->n);
  mx->buf = malloc (elems);
  if (! mx->buf)
    {
      free (mx);
      return NULL;
    }
  memcpy (mx->buf, buf + p, elems);
  p += elems;
  
  *pos = p;
  return mx;
}

