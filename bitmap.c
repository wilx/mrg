#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include "gstdint.h"
#include "bitmap.h"
#include "utility.h"

#define BITS_PER_CHAR (8*SIZEOF_CHAR)

struct _bitmap_t
{
  unsigned size;
  uint32_t * buf;
};


static inline 
unsigned 
elems_from_map (const bitmap_t * bm)
{
  return bm->size / (sizeof (uint32_t) * BITS_PER_CHAR)
    + (bm->size % (sizeof (uint32_t) * BITS_PER_CHAR) ? 1 : 0);
}


static inline 
unsigned 
elems_from_size (unsigned size)
{
  return size / (sizeof (uint32_t) * BITS_PER_CHAR)
    + (size % (sizeof (uint32_t) * BITS_PER_CHAR) ? 1 : 0);
}


static inline
unsigned
bytes_from_map (const bitmap_t * bm)
{
  return elems_from_map (bm) * sizeof (uint32_t);
}


static inline
unsigned
bytes_from_size (unsigned size)
{
  return elems_from_size (size) * sizeof (uint32_t);
}


static inline
unsigned
index_for_pos (unsigned pos)
{
  return pos / (sizeof (uint32_t) * BITS_PER_CHAR);
}


static inline
uint32_t
mask_for_pos (unsigned pos)
{
  return 1u << (pos % (sizeof (uint32_t) * BITS_PER_CHAR));
}


static inline
uint32_t
mask_for_rem (unsigned rem)
{
  return 0xffffffffu >> (sizeof (uint32_t) * BITS_PER_CHAR - rem);
}


static inline
unsigned
rem_from_map (const bitmap_t * bm)
{
  return bm->size % (sizeof (uint32_t) * BITS_PER_CHAR);
}

static inline
unsigned
rem_from_size (unsigned size)
{
  return size % (sizeof (uint32_t) * BITS_PER_CHAR);
}

/**
   Creates a new bitmap.
   @param size size of bitmap
   @return pointer to bitmap
*/
bitmap_t *
bitmap_new (unsigned size)
{
  bitmap_t * bm;
  
  if (size == 0)
    abort ();
  
  bm = malloc (sizeof (bitmap_t));
  if (! bm)
    return NULL;
  if (! bitmap_init (bm, size))
    {
      free (bm);
      return NULL;
    }
  else
    return bm;
}


/**
   Initializes/constructs bitmap.
   @param bm bitmap
   @param size number of bit
   @return first parameter
 */
bitmap_t *
bitmap_init (bitmap_t * bm, unsigned size)
{
  const unsigned bytes = bytes_from_size (size);

  if (size == 0)
    abort ();

  bm->buf = malloc (bytes);
  if (! bm->buf)
    return NULL;
  memset (bm->buf, 0, bytes);
  bm->size = size;
  return bm;
}


/**
   Destructor for bitmap.
   @param bm bitmap
*/
inline
void 
bitmap_destruct (bitmap_t * bm)
{
  free (bm->buf);
}


/**
   Frees memory allocated by bitmap.
   @param bm bitmap
*/
void 
bitmap_delete (bitmap_t * bm)
{
  bitmap_destruct (bm);
  free (bm);
}


/**
   Creates a copy of bitmap.
   @param bm bitmap
   @return cloned bitmap
*/
bitmap_t * 
bitmap_clone (const bitmap_t * bm)
{
  bitmap_t * newbm;
  const unsigned bytes = bytes_from_map (bm);
  
  newbm = malloc (sizeof (bitmap_t));
  if (! newbm)
    return NULL;
  newbm->buf = malloc (bytes);
  if (! newbm->buf)
    {
      free (newbm);
      return NULL;
    }
  if (bm->size == 0)
    abort ();
  memcpy (newbm->buf, bm->buf, bytes);
  newbm->size = bm->size;
  return newbm;
}


/**
   Shrinks or enlarges bitmap.
   @param bm bitmap
   @param size new size of the bitmap
   @return bitmap
 */
bitmap_t * bitmap_resize (bitmap_t * bm, unsigned size)
{
  const unsigned oldsize = bm->size;
  const unsigned oldbytes = bytes_from_size (oldsize);
  const unsigned newbytes = bytes_from_size (size);
  uint32_t * newbuf;

  if (size == 0)
    abort ();

  /* Noop. */
  if (oldsize == size)
      return bm;
  /* Shrink or enlagrge. */
  if (newbytes != oldbytes)
    newbuf = realloc (bm->buf, newbytes);
  else 
    newbuf = bm->buf;
  /* Test for realloc()'s failure. */
  if (! newbuf)
    return NULL;
  /* Clear potentially uninitialized bits. */
  /* Enlarged bitmap with more bytes allocated. */
  if (newbytes > oldbytes)
    {
      const unsigned oldelems = elems_from_size (oldsize);
      const unsigned dif = elems_from_size (size) - oldelems;
      memset (newbuf + oldelems, 0, dif * sizeof (uint32_t));
    }
  /* Shrinked bitmap with less bytes allocated. */
  else if (newbytes < oldbytes)
    {
      const unsigned dif = newbytes * BITS_PER_CHAR - size;
      if (dif != 0)
        {
          const uint32_t mask = 0xffffffffu >> dif;
          newbuf[elems_from_size (size) - 1] &= mask;
        }
    }
  /* No change of size in bytes. */
  else
    {
      unsigned dif;
      /* Few bits added. */
      if (size > oldsize)
        dif = oldbytes * BITS_PER_CHAR - oldsize;
      /* Few bits removed. */
      else if (size < oldsize)
        dif = oldbytes * BITS_PER_CHAR - size;
      /* Noop. */
      else
        abort (); /* Already handled */
      if (dif != 0)
        {
          const unsigned mask = 0xffffffffu >> dif;
          newbuf[elems_from_size (size) - 1] &= mask;
        }
    }
  bm->buf = newbuf;
  bm->size = size;
  return bm;
}

/**
   Returns bit value at position.
   @param bm bitmap
   @param pos position
   @return bit value
*/
inline
int 
bitmap_getbit (const bitmap_t * bm, unsigned pos)
{
  if (pos < bm->size)
    {
      const unsigned i = index_for_pos (pos);
      const uint32_t mask = mask_for_pos (pos);
      uint32_t elem;
      
      elem = bm->buf[i];
      return elem & mask ? 1 : 0;
    }
  else
    abort ();
}


/**
   Sets bit's value.
   @param bm bitmap
   @param pos position
   @param val value
   @return previous value
 */
inline
int 
bitmap_putbit (const bitmap_t * bm, unsigned pos, int val)
{
  if (pos < bm->size)
    {
      const unsigned i = index_for_pos (pos);
      const uint32_t mask = mask_for_pos (pos);
      uint32_t elem;
      int prev;
      
      elem = bm->buf[i];
      prev = elem & mask ? 1 : 0;
      if (val)
        elem |= mask;
      else
        elem &= ~mask;
      bm->buf[i] = elem;
      return prev;
    }
  else
    abort ();
}


/**
   Flips (negates) specified bit.
   @param bm bitmap
   @param pos position
   @return previous value
 */
int bitmap_flipbit (const bitmap_t * bm, unsigned pos)
{
  if (pos < bm->size)
    {
      const int prev = bitmap_getbit (bm, pos);
      const int newbit = ! prev;
      
      bitmap_putbit (bm, pos, newbit);
      return prev;
    }
  else
    abort ();
}


/**
   Sets bit to 1.
   @param bm bitmap
   @param pos position
   @return previous value
*/
int 
bitmap_setbit (const bitmap_t * bm, unsigned pos)
{
  if (pos < bm->size)
    {
      const unsigned i = index_for_pos (pos);
      const uint32_t mask = mask_for_pos (pos);
      uint32_t elem;
      int prev;
      
      elem = bm->buf[i];
      prev = elem & mask ? 1 : 0;
      elem |= mask;
      bm->buf[i] = elem;
      return prev;
    }
  else
    abort ();
}

/**
   Sets bit to 0.
   @param bm bitmap
   @param pos position
   @return previous value
*/
int 
bitmap_clrbit (const bitmap_t * bm, unsigned pos)
{
  if (pos < bm->size)
    {
      const unsigned i = index_for_pos (pos);
      const uint32_t mask = mask_for_pos (pos);
      uint32_t elem;
      int prev;
      
      elem = bm->buf[i];
      prev = elem & mask ? 1 : 0;
      elem &= ~mask;
      bm->buf[i] = elem;
      return prev;
    }
  else
    abort ();
}


/**
   Sets the whole bitmap to zeros.
   @param bm bitmap
   @return bitmap
 */
bitmap_t * 
bitmap_clear (bitmap_t * bm)
{
  memset (bm->buf, 0, bytes_from_map (bm));
  return bm;
}


/**
   Sets the whole bitmap to ones.
   @param bm bitmap
   @return bitmap
 */
bitmap_t * 
bitmap_set (bitmap_t * bm)
{
  const unsigned rem = rem_from_map (bm);
  const unsigned setsize = 
    bytes_from_map (bm) - (rem ? sizeof (uint32_t) : 0);
  
  memset (bm->buf, 0xff, setsize);
  if (rem != 0)
    {
      const unsigned mask = mask_for_rem (rem);
      bm->buf[elems_from_map (bm) - 1] |= mask;
    }
  return bm;
}


/**
   Flips all bits in bitmap.
   @param bm bitmap
   @return bitmap
 */
bitmap_t * 
bitmap_flip (bitmap_t * bm)
{
  const unsigned rem = rem_from_map (bm);
  const unsigned flipelems = elems_from_map (bm) - (rem ? 1 : 0);
  unsigned i;
  
  for (i = 0; i < flipelems; ++i)
    bm->buf[i] = ~bm->buf[i];
  if (rem != 0)
    {
      const uint32_t mask = mask_for_rem (rem);
      bm->buf[i] = ~bm->buf[i] & mask;
    }
  return bm;
}

/**
   Return number of bits in bitmap.
   @param bm bitmap
   @return number of bits
 */
inline
unsigned 
bitmap_size (const bitmap_t * bm)
{
  return bm->size;
}


/**
   Prints bitmap to a stream.
   @param bm bitmap
   @param stream output stream
   @param sep separator
   @return number of bytes written or -1 on error
*/
int 
bitmap_print (const bitmap_t * bm, FILE * stream, const char * sep)
{
  int ret, count = 0;
  unsigned i;
  
  ret = fprintf (stream, "%d", bitmap_getbit (bm, 0));
  if (ret < 0)
    return ret;
  count += ret;
  for (i = 1; i < bitmap_size (bm); ++i)
    {
      ret = fprintf (stream, "%s%d", sep, bitmap_getbit (bm, i));
      if (ret < 0)
        return ret;
      count += ret;
    }
  return count;
}


/**
   Returns how many bytes are needed to serialize bitmap.
*/
size_t 
bitmap_serialize_size (const bitmap_t * bm)
{
  return sizeof (unsigned) + elems_from_map (bm) * sizeof (uint32_t);
}


/**
   Serializes bitmap into a buffer.
*/
void 
bitmap_serialize (void * buf, size_t size, size_t * pos, bitmap_t * bm)
{
  int ret;
  
  ret = MPI_Pack (&bm->size, 1, MPI_UNSIGNED, buf, size, pos, MPI_COMM_WORLD);
  if (ret != MPI_SUCCESS)
    mpierror (ret, "MPI_Pack");
  ret = MPI_Pack (bm->buf, elems_from_map(bm), MPI_UNSIGNED, buf, size, pos,
                  MPI_COMM_WORLD);
  if (ret != MPI_SUCCESS)
    mpierror (ret, "MPI_Pack");
}


/**
   Reconstructs bitmap out of serialized representation.
*/
bitmap_t * 
bitmap_deserialize (void * buf, size_t insize, size_t * pos)
{
  bitmap_t * bm;
  unsigned elems;
  int ret;

  ret = MPI_Unpack (buf, insize, pos, &elems, 1, MPI_UNSIGNED, MPI_COMM_WORLD);
  
  if (ret != MPI_SUCCESS)
    mpierror (ret, "MPI_Unpack()");
  bm = malloc (sizeof (bitmap_t));
  if (! bm)
    return NULL;
  bm->size = elems;
  elems = elems_from_size (elems);
  bm->buf = malloc (elems * sizeof (uint32_t));
  if (! bm->buf)
    {
      free (bm);
      return NULL;
    }
  ret = MPI_Unpack (buf, insize, pos, bm->buf, elems, MPI_UNSIGNED,
                    MPI_COMM_WORLD);
  if (ret != MPI_SUCCESS)
    mpierror (ret, "MPI_Unpack()");
  
  return bm;
}
