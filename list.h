#ifndef _LIST_H_
#define _LIST_H_

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

  struct _listelem_t;
  typedef struct _listelem_t listelem_t;
  
  extern listelem_t * listelem_new (listelem_t * prev, listelem_t * next,
                                    void * data);
  extern listelem_t * listelem_init (listelem_t * el, listelem_t * prev, 
                                     listelem_t * next, void * data);
  extern void * listelem_delete (listelem_t * el);
  extern void * listelem_destroy (listelem_t * el);
  extern void * listelem_data (const listelem_t * el);


  struct _list_t;
  typedef struct _list_t list_t;

  extern list_t * list_new (void);
  extern list_t * list_init (list_t * l);
  extern unsigned list_destroy (list_t * l);
  extern unsigned list_delete (list_t * l);
  extern void * list_push (list_t * l, void * data);
  extern void * list_pop (list_t * l);
  extern void * list_pushback (list_t * l, void * data);
  extern void * list_popback (list_t * l);
  extern void * list_remove (list_t * l, listelem_t * el);
  extern unsigned list_size (const list_t * l);
  extern void * list_next (listelem_t ** pel);
  extern void * list_prev (listelem_t ** pel);
  extern void * list_first (const list_t * l, listelem_t ** pel);
  extern void * list_last (const list_t * l, listelem_t ** pel);
  extern void list_foreach (const list_t * l, int (* cb)(void *));

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
