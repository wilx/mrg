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
