#include <stdlib.h>
#include "list.h"


struct _listelem_t 
{
  listelem_t * prev;
  listelem_t * next;
  void * data;
};

struct _list_t
{
  unsigned count;
  listelem_t * first;
  listelem_t * last;
};


/**
   Initializes list element.
   @param el element
   @param prev pointer to prevous element
   @param next pointer to next element
   @param data pointer to element's data
   @return element
*/
inline
listelem_t * 
listelem_init (listelem_t * el, listelem_t * prev, 
               listelem_t * next, void * data)
{
  el->prev = prev;
  el->next = next;
  el->data = data;
  return el;
}


/**
   Allocates and initializes a new list element.
   @param prev pointer to previous element
   @param next pointer to next element
   @param data pointer to elemet's data
   @return element
*/
inline
listelem_t * 
listelem_new (listelem_t * prev, listelem_t * next, void * data)
{
  listelem_t * el;
  
  el = malloc (sizeof (listelem_t));
  if (! el)
    return NULL;
  if (! listelem_init (el, prev, next, data))
    {
      free (el);
      return NULL;
    }
  else
    return el;
}
 

/**
   Destroys an element.
   @param el element
   @return pointer to elements data
*/
inline
void * 
listelem_destroy (listelem_t * el)
{
  if (el->prev)
    if (el->next)
      {
        el->prev->next = el->next;
        el->next->prev = el->prev;
      }
    else
      el->prev->next = NULL;
  else if (el->next)
    el->next->prev = NULL;
  return el->data;
}


/**
   Deallocates and destroys an element.
   @param el element
   @return pointer to elements data
*/
inline
void * 
listelem_delete (listelem_t * el)
{
  void * data = listelem_destroy (el);
  
  free (el);
  return data;
}


/**
 */
void * 
listelem_data (const listelem_t * el)
{
  return el->data;
}


/**
   Initializes list_t instance.
   @param l list
   @return list
*/
inline
list_t * 
list_init (list_t * l)
{
  l->count = 0;
  l->first = NULL;
  l->last = NULL;
  return l;
}


/**
   Allocates and initializes a new list.
   @return new list
*/
list_t * 
list_new (void)
{
  list_t * l;
  
  l = malloc (sizeof (list_t));
  if (! l)
    return NULL;
  if (! list_init (l))
    {
      free (l);
      return NULL;
    }
  else 
    return l;
}


/**
   Destroys list_t instace.
   @param l list
   @return number of free'd elements
*/
inline
unsigned 
list_destroy (list_t * l)
{
  listelem_t * el, * next;
  unsigned count = 0;

  el = l->first;
  while (el)
    {
      next = el->next;
      free (el);
      ++count;
      el = next;
    }
  if (count != l->count)
    abort ();
  return count;
}


/**
   Deallocates and destroys list.
   @param l list
   @return number of free'd elements
*/
unsigned 
list_delete (list_t * l)
{
  unsigned count = list_destroy (l);
  
  free (l);
  return count;
}


/**
   Pushes elemnt onto the top of list.
   @param l list
   @param data pointer to stored data
   @return pointer to stored data
*/
void * 
list_push (list_t * l, void * data)
{
  listelem_t * el;

  if (l->first && l->last)
    {
      el = listelem_new (NULL, l->first, data);
      if (! el)
        return NULL;
      l->first = l->first->prev = el;
    }
  else if (! l->first && ! l->last)
    {
      el = listelem_new (NULL, NULL, data);
      if (! el)
        return NULL;
      l->first = el;
      l->last = el;
    }
  else 
    abort (); /* This should never happen. */
  l->count += 1;
  return data;
}


/**
   Pops one element from the beginning of the list.
   @param l list
   @return pointer to data of the poped element
*/
void * 
list_pop (list_t * l)
{
  listelem_t * el;
  
  if (! l->first)
    abort ();
  
  el = l->first;
  if (el->next)
    l->first = el->next;
  else
    {
      l->first = NULL;
      l->last = NULL;
    }
  l->count -= 1;
  return listelem_delete (el);
}


/**
   Same as list_push() but to the end of list.
   @param l list
   @param data pointer to data
   @return pointer to data
*/
void * 
list_pushback (list_t * l, void * data)
{
  listelem_t * el;

  if (l->first && l->last)
    {
      el = listelem_new (l->last, NULL, data);
      if (! el)
        return NULL;
      l->last->next = el;
      l->last = el;
    }
  else if (! l->first && ! l->last)
    {
      el = listelem_new (NULL, NULL, data);
      l->first = el;
      l->last = el;
    }
  else
    abort (); /* This should never happen. */
  l->count += 1;
  return data;
}


/**
   Same as pop() but pops element from back of list.
   @param l list
   @return pointer to data of poped element
*/
void * 
list_popback (list_t * l)
{
  listelem_t * el;

  if (! l->last)
    abort ();
  el = l->last;
  if (el->prev)
    l->last = el->prev;
  else 
    {
      l->last = NULL;
      l->first = NULL;
    }
  l->count -= 1;
  return listelem_delete (el);
}


/**
   Removes single lemenent from list l pointed to by el.
   @param l list
   @param el element of list el
   @return pointer to elements data
*/
void * 
list_remove (list_t * l, listelem_t * el)
{
  if (el == l->first)
    if (el == l->last)
      {
        l->first = NULL;
        l->last = NULL;
      }
    else
      l->first = el->next;
  else if (el == l->last)
    l->last = el->prev;
  l->count -= 1;
  return listelem_delete (el);
}


/**
   Returns number of elements in list.
   @param l list
   @return number of elements
*/
unsigned
list_size (const list_t * l)
{
  return l->count;
}


/**
   Advances to next element and returns pointer to its data or NULL.
   @param pel pointer to list element.
   @return pointer to next element's data
*/
inline
void * 
list_next (listelem_t ** pel)
{
  *pel = (*pel)->next;
  if (*pel)
    return (*pel)->data;
  else
    return NULL;
}


/**
   Goes back one element.
   @param pel pointer to list element
   @return pointer to previous element
*/
inline
void * 
list_prev (listelem_t ** pel)
{
  *pel = (*pel)->prev;
  if (*pel)
    return (*pel)->data;
  else
    return NULL;
}


/**
   Returns pointer to data of the first element and initializes iterator.
   @param l list
   @param pel pointer to iterator
*/
inline
void * 
list_first (const list_t * l, listelem_t ** pel)
{
  *pel = l->first;
  if (*pel)
    return (*pel)->data;
  else
    return NULL;
}


/**
   Returns pointer to data of the last element and initializes iterator.
   @param l list
   @param pel pointer to iterator
*/
inline
void * 
list_last (const list_t * l, listelem_t ** pel)
{
  *pel = l->last;
  if (*pel)
    return (*pel)->data;
  else
    return NULL;
}


/**
   Executes callback for each of elements.
   @param l list
   @param cb callback
*/
void 
list_foreach (const list_t * l, int (* cb)(void *))
{
  listelem_t * el;
  void * data;

  data = list_first (l, &el);
  while (el)
    {
      if (! cb (data))
        break;
      data = list_next (&el);
    }
}
