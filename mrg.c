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
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>
#include <mpi.h>
#include "config.h"
#include "bitmap.h"
#include "matrix.h"
#include "list.h"
#include "utility.h"


#define TYPE_MSG 'M' /* A message. See MSG_*. */
#define TYPE_STKELEM 'S' /* Stack element, as response to a request. */
#define TYPE_BEST 'B' /* Best stack element. */
#define TYPE_BWEIGHT 'C' /* Best weight. */
/*#define TYPE_GRAPH 'G'*/ /* Graph */
/*#define TYPE_GWEIGHTS 'H'*/ /* Weights of graph's edges. */
#define TYPE_TOKEN  'T' /* Token for ADUV. */
#define TYPE_DONOR 'D' /* Answer to a donor request. */

#define MSG_EOC 'E' /* End of computation. */
#define MSG_REQ 'R' /* Request work. */
#define MSG_DENY 'D' /* Deny work. */
#define MSG_DREQ 'O' /* Request donor from P1. */
#define MSG_EOE 'F' /* No more stack elements are coming. */

#define TOKEN_BLACK 'B'
#define TOKEN_WHITE 'W'
#define TOKEN_NONE 'N'

#define TAG_CAN_WAIT 1
#define TAG_NEEDS_ATTENTION 20
//#define TAG_WORK_COMM 2
//#define TAG_DONOR_COMM 3

/*
struct _msg_t
{
  char type;
  char buf[1];
};
typedef struct _msg_t msg_t;
*/

/* Message receive buffer. */
void * recv_buf;
size_t recv_buf_len;
/* */
/*
msg_t * graph_buf;
msg_t * weights_buf;
*/
/* Predefined messages. */
/*
msg_t donor_msg = {TYPE_DONOR, {'\0'}};
const msg_t eoc_msg = {TYPE_MSG, {MSG_EOC}};
const msg_t req_msg = {TYPE_MSG, {MSG_REQ}};
const msg_t deny_msg = {TYPE_MSG, {MSG_DENY}};
const msg_t dreq_msg = {TYPE_MSG, {MSG_DREQ}};
const msg_t eoe_msg = {TYPE_MSG, {MSG_EOE}};
const msg_t btoken_msg = {TYPE_TOKEN, {TOKEN_BLACK}};
const msg_t wtoken_msg = {TYPE_TOKEN, {TOKEN_WHITE}};
*/
/* GCZ-AHD counter. */
int donor;
/* ADUV */
char mycolor, token;
/* MPI status. */
MPI_Status status;
/* "Would give out work" flag. */
int wouldgive = 0;
/* */
unsigned denycount = 0;


struct _stkelem_t 
{
  int uptodate;
  /* Weight of this cut. */
  int weight;
  /* Offset of the rightmost 1 in the future new element
     generated from this element.*/
  unsigned next;
  /* Representation of X and Y sets. */
  bitmap_t * set;
};
typedef struct _stkelem_t stkelem_t;


/* Number of nodes. */
unsigned N = 0;
/* Stack for DFS algorithm. */
list_t * stack;
/* */
trimatrix_t * graph;
/* Matrix of edges' weights. */
wtrimatrix_t * weights;
/* Best solution. */
stkelem_t * best;
/* Rank of a process. */
int rank;
/* Size of the world. */
int worldsize;


/**
   Initializes new DFS stack element.
   @param se pointer to stack element
   @param width width of bitmap/set
   @param weight weight of cut in this step
   @param rightmost offset of the rightmost 1 in bitmap/set
   @param last offset of the last 1 in the last generated element
*/
inline
stkelem_t *
stkelem_init (stkelem_t * se, unsigned width, unsigned weight, 
              unsigned next, int utd)
{
  if (width == 0 || next > width - 1)
    abort ();
  
  se->set = bitmap_new (width);
  if (! se->set)
    {
      free (se);
      return NULL;
    }
  se->weight = weight;
  se->next = next;
  se->uptodate = utd;
  return se;
}


/**
   Allocates and initializes new DFS stack element.
   @param width width of bitmap/set
   @param weight weight of cut in this step
   @param rightmost offset of the rightmost 1 in bitmap/set
   @param next offset of the rightmost 1 of future generated element
*/
stkelem_t * 
stkelem_new (unsigned width, unsigned weight, unsigned next,
             int utd)
{
  stkelem_t * se;

  se = malloc (sizeof (stkelem_t));
  if (! se)
    return NULL;
  if (! stkelem_init (se, width, weight, next, utd))
    {
      free (se);
      return NULL;
    }
  return se;
}


/**
   Clones DFS stack element.
   @param se stack element
   @return copy of the stack element
*/
stkelem_t * 
stkelem_clone (const stkelem_t * se)
{
  stkelem_t * newse;
  
  newse = malloc (sizeof (stkelem_t));
  if (! newse)
    return NULL;
  newse->set = bitmap_clone (se->set);
  if (! newse->set)
    {
      free (newse);
      return NULL;
    }
  newse->weight = se->weight;
  newse->next = se->next;
  newse->uptodate = se->uptodate;
  return newse;
}


/**

*/
inline
void 
stkelem_destroy (const stkelem_t * se)
{
  bitmap_delete (se->set);
}


/**
 
*/
void
stkelem_delete (stkelem_t * se)
{
  stkelem_destroy (se);
  free (se);
}


/**

*/
size_t 
stkelem_serialize_size (const stkelem_t * se)
{
  return 2*sizeof (int) + sizeof (unsigned)
    + bitmap_serialize_size (se->set);
}


/**

*/ 
void 
stkelem_serialize (void * buf, size_t size, size_t * pos, stkelem_t * se)
{
  int ret;

  ret = MPI_Pack (&se->uptodate, 1, MPI_INT, buf, size, pos, MPI_COMM_WORLD);
  if (ret != MPI_SUCCESS)
    mpierror (ret, "MPI_Pack()");

  ret = MPI_Pack (&se->weight, 1, MPI_INT, buf, size, pos, MPI_COMM_WORLD);
  if (ret != MPI_SUCCESS)
    mpierror (ret, "MPI_Pack()");

  ret = MPI_Pack (&se->next, 1, MPI_UNSIGNED, buf, size, pos, MPI_COMM_WORLD);
  if (ret != MPI_SUCCESS)
    mpierror (ret, "MPI_Pack()");
  
  bitmap_serialize (buf, size, pos, se->set);
}


/**

*/
stkelem_t * 
stkelem_deserialize (void * buf, size_t insize, size_t * pos)
{
  stkelem_t * se;
  int ret;

  se = malloc (sizeof (stkelem_t));
  if (! se)
    return NULL;

  ret = MPI_Unpack (buf, insize, pos, &se->uptodate, 1, MPI_INT, 
                    MPI_COMM_WORLD);
  if (ret != MPI_SUCCESS)
    mpierror (ret, "MPI_Unpack()");

  ret = MPI_Unpack (buf, insize, pos, &se->weight, 1, MPI_INT, 
                    MPI_COMM_WORLD);
  if (ret != MPI_SUCCESS)
    mpierror (ret, "MPI_Unpack()");

  ret = MPI_Unpack (buf, insize, pos, &se->next, 1, MPI_UNSIGNED, 
                    MPI_COMM_WORLD);
  if (ret != MPI_SUCCESS)
    mpierror (ret, "MPI_Unpack()");

  se->set = bitmap_deserialize (buf, insize, pos);
  if (! se->set)
    {
      free (se);
      return NULL;
    }
  
  return se;
}


void pack_type (void * buf, size_t size, size_t * pos, char type)
{
  int ret;

  ret = MPI_Pack (&type, 1, MPI_CHAR, buf, size, pos, MPI_COMM_WORLD);
  if (ret != MPI_SUCCESS)
    mpierror (ret, "MPI_Pack()");
}


void pack_simple_msg (void * buf, size_t size, size_t * pos, char msg_type)
{
  int ret;

  pack_type (buf, size, pos, TYPE_MSG);
  ret = MPI_Pack (&msg_type, 1, MPI_CHAR, buf, size, pos, MPI_COMM_WORLD);
  if (ret != MPI_SUCCESS)
    mpierror (ret, "MPI_Pack()");
}


void pack_bweight_msg (void * buf, size_t size, size_t * pos, int bweight)
{
  int ret;

  pack_type (buf, size, pos, TYPE_BWEIGHT);
  ret = MPI_Pack (&bweight, 1, MPI_INT, buf, size, pos, MPI_COMM_WORLD);
  if (ret != MPI_SUCCESS)
    mpierror (ret, "MPI_Pack()");
}


void pack_stkelem_msg (void * buf, size_t size, size_t * pos, stkelem_t * se)
{
  pack_type (buf, size, pos, TYPE_STKELEM);
  stkelem_serialize (buf, size, pos, se);
}


void pack_best_msg (void * buf, size_t size, size_t * pos, stkelem_t * se)
{
  pack_type (buf, size, pos, TYPE_BEST);
  stkelem_serialize (buf, size, pos, se);
}


void pack_char (void * buf, size_t size, size_t * pos, char ch)
{
  int ret; 

  ret = MPI_Pack (&ch, 1, MPI_CHAR, buf, size, pos, MPI_COMM_WORLD);
  if (ret != MPI_SUCCESS)
    mpierror (ret, "MPI_Pack()");
}


void pack_donor_msg (void * buf, size_t size, size_t * pos, char dnr)
{
  pack_type (buf, size, pos, TYPE_DONOR);
  pack_char (buf, size, pos, dnr);
}


void pack_token_msg (void * buf, size_t size, size_t * pos, char tok)
{
  pack_type (buf, size, pos, TYPE_TOKEN);
  pack_char (buf, size, pos, tok);
}


char unpack_type (void * buf, size_t insize, size_t * pos, char * type)
{
  int ret;
  char t;

  ret = MPI_Unpack (buf, insize, pos, &t, 1, MPI_CHAR, MPI_COMM_WORLD);
  if (ret != MPI_SUCCESS)
    mpierror (ret, "MPI_Unpack()");
  if (type)
    *type = t;
  return t;
}


char unpack_char (void * buf, size_t insize, size_t * pos)
{
  int ret;
  char ch;

  ret = MPI_Unpack (buf, insize, pos, &ch, 1, MPI_CHAR, MPI_COMM_WORLD);
  if (ret != MPI_SUCCESS)
    mpierror (ret, "MPI_Unpack()");
  return ch;
}

/**
   Initializes stack for DFS algoritm.
*/
void 
initialize_stack (void)
{
  stkelem_t * el = stkelem_new (N, 0, 0, 1);
  
  fprintf (stderr, "[%d] initializing stack\n", rank);
  if (! el)
    error ("Memory allocation failure");
  if (! list_pushback (stack, el))
    error ("list_pushback()");
}


/**
   Global initialization of computation.
*/
void
initialize (void)
{
  /* Best solution. */
  best = stkelem_new (N, INT_MAX, 0, 1);  
  /* Receive buffer. */
  recv_buf_len = 1 + stkelem_serialize_size (best) + 1000 /*rezerva :)*/;
  recv_buf = malloc (recv_buf_len);
  /* Other buffers. */
  /*graph_buf = malloc (1 + trimatrix_serialize_size (graph));*/
  /*weights_buf = malloc (1 + wtrimatrix_serialize_size (weights));*/
  if (!best || ! recv_buf /*|| ! graph_buf || ! weights_buf*/)
    error ("Memory allocation failure");
  /* The rest. */
  if (rank == 0)
    token = TOKEN_WHITE;
  else
    token = TOKEN_NONE;
  mycolor = TOKEN_WHITE;
  if (rank == 0)
    initialize_stack ();
  else
    ;
}


/**
   MPI initialization.
*/
void 
initialize_mpi (int * argc, char *** argv, int * rank, int * size)
{
  int ret;

  MPI_Init (argc, argv);
  ret = MPI_Comm_rank (MPI_COMM_WORLD, rank);
  if (ret != MPI_SUCCESS)
    mpierror (ret, "MPI_Comm_rank");
  fprintf (stderr, "[%d] reporting\n", *rank);
  ret = MPI_Comm_size(MPI_COMM_WORLD, size);
  if (ret != MPI_SUCCESS)
    mpierror (ret, "MPI_Comm_size");
  fprintf (stderr, "[%d] worldsize=%d\n", *rank, worldsize);
}


/**
   Updates weight of cut when we move one node from set X to Y.
   @param el element of DFS tree to update
   @return true if the cut has weight 1, false otherwise
*/
int 
update_weight (stkelem_t * el, unsigned node)
{
  unsigned i;
  int ret;

  if (node == 0)
    abort ();
  if (el->uptodate)
    {
      fprintf (stderr, "[%d] trying to update up-to-date stack element\n", 
               rank);
      abort ();
    }

  /* Add/substract weight of edges to/from current */
  for (i = 1; i <= N; ++i)
    {
      if (i == node)
        continue;
      if (trimatrix_get (graph, node, i))
        {
          /* Is node i in set Y? */
          /*fprintf (stderr, "[%d] N=%d, i=%d, bitmap_size(el->set)=%d\n", 
            rank, N, i, bitmap_size (el->set));*/
          if (bitmap_getbit (el->set, i-1))
            /* Substract weight of edges whose end nodes are now
               both in Y from the weight of the cut. */
            el->weight -= wtrimatrix_get (weights, node, i);
          else
            /* Add weight of edges whose end nodes are now one in
               the set X and the other in the set Y. */
            el->weight += wtrimatrix_get (weights, node, i);
        }
    }
  
  el->uptodate = 1;
  if (el->weight < best->weight && el->weight > 0)
    {
      size_t pos = 0;

      fprintf (stderr, "[%d] got better solution than current best %d < %d\n",
              rank, el->weight, best->weight);
      stkelem_delete (best);
      best = stkelem_clone (el);
      if (! best)
        error ("Memory allocation failure.");
      if (rank != 0)
        {
          size_t pos = 0;
          
          /* Send best stack element to P1. */
          pack_best_msg (recv_buf, recv_buf_len, &pos, best);
          fprintf (stderr, "[%d] sending my best to 0\n", rank);
          ret = MPI_Send (recv_buf, pos, MPI_PACKED, 0, TAG_CAN_WAIT,
                          MPI_COMM_WORLD);
          if (ret != MPI_SUCCESS)
            mpierror (ret, "MPI_Send()");
          
        }
      /* Send best weight to everybody else. */
      pack_bweight_msg (recv_buf, recv_buf_len, &pos, best->weight);
      for (i = 1; i < worldsize; ++i)
        {
          if (i == rank)
            continue;
          fprintf (stderr, "[%d] sending my best weight to everybody else\n",
                   rank);
          ret = MPI_Send (recv_buf, pos, MPI_PACKED, i, TAG_CAN_WAIT, 
                          MPI_COMM_WORLD);
          if (ret != MPI_SUCCESS)
            mpierror (ret, "MPI_Send()");
        }
    }

  if (el->weight == 1)
    return 1;
  else
    return 0;
}


/**
   Generates next level of DFS tree from element el and pushes it 
   at the end of list.
   @param el element
   @return true if the next element was successfully generated,
   false otherwise.
*/
int
generate_depth (list_t * list, stkelem_t * el)
{
  stkelem_t * newel;

  /* Is it possible to go deeper in DFS tree? */
  if (el->next < N)
    {
      /* el:    [1 0 0 ... 0]
         |        
         v        
         newel: [1 1 0 ... 0] */
      newel = stkelem_clone (el);
      if (! newel)
        error ("Memory allocation failure");
      bitmap_setbit (newel->set, el->next);
      newel->next = el->next + 1;
      el->next += 1;
      newel->uptodate = 0;
      /* Push newel onto DFS stack. */
      if (! list_push (list, newel))
        error ("list_push()");
      return 1;
    }
  else
    return 0;
}


void 
end_computation (void)
{
  int i;
  size_t pos = 0;
  FILE * output = stdout;

  if (rank != 0)
    error ("end_computation() called by rank != 0");

  /* Prepare the message. */
  pack_simple_msg (recv_buf, recv_buf_len, &pos, MSG_EOC);
  fprintf (stderr, "[%d] sending MSG_EOC to processor", rank);
  /* End of computation. */
  for (i = 1; i < worldsize; ++i)
    {
      int ret;

      ret = MPI_Send (recv_buf, pos, MPI_PACKED, i, TAG_NEEDS_ATTENTION, 
                      MPI_COMM_WORLD); 
      if (ret != MPI_SUCCESS)
        mpierror (ret, "MPI_Send()");
      fprintf (stderr, " %d", i);
    }
  fprintf (stderr, "\n");

  /* Print out the solution. */
  fprintf (output, "\nWeight of the best solution: %d\n", best->weight);
  fprintf (output, "Set X:");
  for (i = 0; i < bitmap_size (best->set); ++i)
    {
      int b = bitmap_getbit (best->set, i);
      if (! b)
        fprintf (output, " %d", i+1);
    }
  fprintf (output, "\n");
  fprintf (output, "Set Y:");
  for (i = 0; i < bitmap_size (best->set); ++i)
    {
      int b = bitmap_getbit (best->set, i);
      if (b)
        fprintf (output, " %d", i+1);
    }
  fprintf (output, "\n");
  fflush (output);

  MPI_Finalize ();
  exit (EXIT_SUCCESS);
}


void 
do_tokens ()
{
  int ret;
  size_t pos = 0;

  if (rank == 0)
    {
      /* Prepare the message. */
      pack_token_msg (recv_buf, recv_buf_len, &pos, TOKEN_WHITE);
      //pack_token_msg (recv_buf, recv_buf_len, &pos, token);
      /* Send white token to P2. */
      fprintf (stderr, "[0] sending WHITE token to 1\n");
      ret = MPI_Send (recv_buf, pos, MPI_PACKED, 1, 
                      TAG_NEEDS_ATTENTION, MPI_COMM_WORLD);
      if (ret != MPI_SUCCESS)
        mpierror (ret, "MPI_Send()");
      token = TOKEN_NONE;
    }
  else
    {
      if (token != TOKEN_NONE)
        {
          /* Prepare the message. */
          pack_token_msg (recv_buf, recv_buf_len, &pos, token);
          /* Send my token to the next process. */
          fprintf (stderr, "[%d] sending '%c' token to %d\n",
                   rank, token, (rank + 1) % worldsize);
          ret = MPI_Send (recv_buf, pos, MPI_PACKED, 
                          (rank + 1) % worldsize, TAG_NEEDS_ATTENTION, 
                          MPI_COMM_WORLD);
          if (ret != MPI_SUCCESS)
            mpierror (ret, "MPI_Send()");
          mycolor = TOKEN_WHITE;
          token = TOKEN_NONE;
        }
    }
}
 
void process_message (void * buf, size_t insize);

/* Either send out work or deny the request. */
void 
process_work_request (int from)
{
  listelem_t * it;
  stkelem_t * el;
  unsigned half;
  int i, ret, j=0;
  list_t * tmplist;

  fprintf (stderr, "[%d] received work request from %d\n", rank, from);
 
  /* 
     Do we have anything to give? 
     Do we want to give at all?
  */
  if (list_size (stack) == 0 || ! wouldgive)
    {
      size_t pos = 0;

      /* Prepare the message. */
      pack_simple_msg (recv_buf, recv_buf_len, &pos, MSG_DENY);
      /* Nope, deny the request. */
      fprintf (stderr, "[%d] there is nothing to give, denying request\n",
              rank);
      ret = MPI_Send (recv_buf, pos, MPI_PACKED, from, TAG_NEEDS_ATTENTION, 
                      MPI_COMM_WORLD);
      if (ret != MPI_SUCCESS)
        mpierror (ret, "MPI_Send()");
      return;
    }
  /* We have something to give. */
  tmplist = list_new ();
  if (! tmplist)
    error ("Memory allocation failure.");
  el = list_last (stack, &it);
  if (! el->uptodate)
    update_weight (el, el->next);
  half = (N - el->next) / 2;
  if (half != 0 && rank > from)
    /* Change token. */
    mycolor = TOKEN_BLACK;
  /* Generate the half. */
  fprintf (stderr, "[%d] generating %d new stack elements\n",
          rank, half);
  for (i = 1; i <= half; ++i)
    generate_depth (tmplist, el);
  /* Send the half to requester. */
  while (list_size (tmplist) != 0)
    {
      stkelem_t * tmpel;
      size_t pos = 0;
                
      tmpel = list_popback (tmplist);
      pack_stkelem_msg (recv_buf, recv_buf_len, &pos, tmpel);
      fprintf (stderr, "[%d] sending generated stack element %d to %d\n",
              rank, ++j, from);
      ret = MPI_Send (recv_buf, pos, MPI_PACKED, from, TAG_NEEDS_ATTENTION,
                      MPI_COMM_WORLD);
      if (ret != MPI_SUCCESS)
        mpierror (ret, "MPI_Send()");
      stkelem_delete (tmpel);
    }
  list_delete (tmplist);
  /* Send MSG_EOE to requester. */
  {
    size_t pos = 0; 
    
    pack_simple_msg (recv_buf, recv_buf_len, &pos, MSG_EOE);
    fprintf (stderr, "[%d] sending MSG_EOE to %d\n", rank, from);
    ret = MPI_Send (recv_buf, pos, MPI_PACKED, from, TAG_NEEDS_ATTENTION,
                    MPI_COMM_WORLD);
    if (ret != MPI_SUCCESS)
      mpierror (ret, "MPI_Send()");
  }
}


void 
process_donor_request (int from)
{
  int ret;

  if (rank == 0)
    {
      size_t pos = 0;
      pack_donor_msg (recv_buf, recv_buf_len, &pos, (char)donor);
      fprintf (stderr, "[0] request for donor has been received"
              ", sending donor %d to process %d\n", donor, from);
      donor = (donor + 1) % worldsize;
      ret = MPI_Send (recv_buf, pos, MPI_PACKED, from, TAG_NEEDS_ATTENTION, 
                      MPI_COMM_WORLD);
      if (ret != MPI_SUCCESS)
        mpierror (ret, "MPI_Send()");
      return;
    }
  else
    error ("Donor request to process != P1!!!");
}


/* Processes messages with TAG_NEEDS_ATTENTION. */
void
process_clamour_message (void * buf, size_t insize)
{
  size_t inpos = 0;
  char type;
  char msg_type;
  
  type = unpack_char (buf, insize, &inpos);
  fprintf (stderr, "[%d] processing message type='%c'\n", rank, type);
  switch (type)
    {
    case TYPE_MSG:
      msg_type = unpack_char (buf, insize, &inpos);
      fprintf (stderr, "[%d] processing simple message type='%c'\n", rank, 
               msg_type);
      switch (msg_type)
        {
        case MSG_REQ:
          process_work_request (status.MPI_SOURCE);
          return;

        case MSG_DREQ:
          process_donor_request (status.MPI_SOURCE);
          return;

        case MSG_EOC:
          fprintf (stderr, "[%d] end of computation has been received\n", 
                   rank);
          MPI_Finalize ();
          exit (EXIT_SUCCESS);

        default:
          fprintf (stderr, "[%d] unhandled clamour message!!!\n", rank);
          error ("Unhandled MSG_ in process_clamour_message()!!!");
        }
    case TYPE_TOKEN:
      {
        char tok;
        
        tok = unpack_char (buf, insize, &inpos);
        fprintf (stderr, "[%d] received '%c' token\n", rank, tok);
        if (rank == 0)
          if (tok == TOKEN_WHITE)
            {
              fprintf (stderr, "[0] got WHITE token back"
                       ", invoking end_computation()\n");
              end_computation ();
            }
          else
            {
              fprintf (stderr, "[%d] coloring to 'W'\n", rank);
              token = TOKEN_WHITE;
            }
        else
          if (mycolor == TOKEN_WHITE)
            {
              fprintf (stderr, "[%d] color='%c', token='%c' has been"
                       " received\n",
                       rank, mycolor, tok);
              token = tok;
            }
          else
            {
              fprintf (stderr, "[%d] color='%c', coloring token to '%c'\n",
                       rank, mycolor, TOKEN_BLACK);
              token = TOKEN_BLACK;
            }
        return;
      }

    default:
      error ("Unhandled TYPE_* in process_clamour_message()!!!");
    }
}


int 
request_donor (void)
{
  int ret;
  size_t pos = 0;
  char type;

  /* Send request for donor to 0. */
  pack_simple_msg (recv_buf, recv_buf_len, &pos, MSG_DREQ);
  fprintf (stderr, "[%d] sending request for donor to 0\n", rank);
  ret = MPI_Send (recv_buf, pos, MPI_PACKED, 0, TAG_NEEDS_ATTENTION, 
                  MPI_COMM_WORLD);
  if (ret != MPI_SUCCESS)
    mpierror (ret, "MPI_Send()");
  /* Wait for answer from 0. */
  while (1)
    {
      ret = MPI_Recv (recv_buf, recv_buf_len, MPI_PACKED, MPI_ANY_SOURCE, 
                      TAG_NEEDS_ATTENTION, MPI_COMM_WORLD, &status);
      if (ret != MPI_SUCCESS)
        mpierror (ret, "MPI_Recv()");
      pos = 0;
      type = unpack_char (recv_buf, recv_buf_len, &pos);
      if (type != TYPE_DONOR || status.MPI_SOURCE != 0)
        /* Some other clamour message. */
        {
          process_clamour_message (recv_buf, recv_buf_len);
          continue;
        }
      else
        /* Received the donor. */
        break;
    }

  return (int)unpack_char (recv_buf, recv_buf_len, &pos);
}


void request_work (int from)
{
  int ret, j=0;
  size_t pos = 0;

  /* Send the request. */
  pack_simple_msg (recv_buf, recv_buf_len, &pos, MSG_REQ);
  fprintf (stderr, "[%d] sending request for work to %d\n", rank, from);
  ret = MPI_Send (recv_buf, pos, MPI_PACKED, from, TAG_NEEDS_ATTENTION, 
                  MPI_COMM_WORLD);
  if (ret != MPI_SUCCESS)
    mpierror (ret, "MPI_Send()");

  /* Process answer. */
  while (1)
    {
      char type, msg_type;
      stkelem_t * se;
      int dnr;
      

      ret = MPI_Recv (recv_buf, recv_buf_len, MPI_PACKED, MPI_ANY_SOURCE, 
                      TAG_NEEDS_ATTENTION, MPI_COMM_WORLD, &status);
      if (status.MPI_SOURCE != from)
        {
          process_clamour_message (recv_buf, recv_buf_len);
          continue;
        }
      if (ret != MPI_SUCCESS)
        mpierror (ret, "MPI_Recv()");
      pos = 0;
      type = unpack_char (recv_buf, recv_buf_len, &pos);
      switch (type)
        {
        case TYPE_MSG:
          msg_type = unpack_char (recv_buf, recv_buf_len, &pos);
          switch (msg_type)
            {
            case MSG_EOE:
              fprintf (stderr, "[%d] received MSG_EOE from %d\n", rank, from);
              return;
              
            case MSG_DENY:
              fprintf (stderr, "[%d] received denying answer from %d\n", 
                       rank, from);
              do_tokens ();
              do 
                {
                  if (rank != 0)
                    /* Request a donor from 0. */
                    dnr = request_donor ();
                  else
                    {
                      dnr = donor;
                      donor = (donor + 1) % worldsize;
                    }
                  if (dnr != rank)
                    /* Send request to obtained donor and read results. */
                    request_work (dnr);
                  //continue;
                  /* Request a new donor from 0. */
                  //dnr = request_donor ();
                  /* Send request to obtained donor and read results. */
                  //request_work (dnr);
                }
              while (dnr == rank);
              return;

            default:
              fprintf (stderr, "[%d] processing clamour message from %d"
                       " in request_work()\n", rank, status.MPI_SOURCE);
              process_clamour_message (recv_buf, recv_buf_len);
              continue;
            }

        case TYPE_STKELEM:
          se = stkelem_deserialize (recv_buf, recv_buf_len, &pos);
          if (! se)
            error ("Memory allocation problem.");
          if (! list_pushback (stack, se))
            error ("list_pushback()");
          fprintf (stderr, "[%d] received stack element %d from %d\n",
                   rank, ++j, from);
          continue;
          
        default:
          fprintf (stderr, "[%d] processing clamour message from %d"
                   " in request_work()\n", rank, status.MPI_SOURCE);
          process_clamour_message (recv_buf, recv_buf_len);
          continue;
        }
    }
  error ("BUG!!! You should not have ever seen this!!!");
}


void 
process_best (void * buf, size_t insize, size_t * pos)
{
  stkelem_t * se;
  
  if (rank != 0)
    error ("'Best' stack element received by process != P1!!!");
  
  se = stkelem_deserialize (buf, insize, pos);
  if (! se)
    error ("Memory allocation problem.");
  if (se->weight < best->weight)
    {
      best = se;
      if (best->weight == 1)
        {
          fprintf (stderr, "[%d] got best->weight==1, "
                   "invoking end_computation()\n", rank);
          end_computation ();
        }
    }
  else
    {
      stkelem_delete (se);
      fprintf (stderr, "[%d] received 'best' that"
               " is worse than its 'best'!!!\n", rank);
    }
  fprintf (stderr, "[0] received new best stack element, weight=%d\n",
          best->weight);
}


void 
process_message (void * buf, size_t insize)
{
  int ret;
  size_t inpos = 0;
  char type;
  char msg_type;
  
  type = unpack_char (buf, insize, &inpos);
  fprintf (stderr, "[%d] processing message type='%c'\n", rank, type);
  switch (type)
    {
    case TYPE_MSG:
      msg_type = unpack_char (buf, insize, &inpos);
      fprintf (stderr, "[%d] processing simple message type='%c'\n", rank, 
               msg_type);
      switch (msg_type)
        {
        default:
          fprintf (stderr, "[%d] unhandled MSG_* in process_message()!!!\n", 
                   rank);
          error ("Unhandled MSG_*!!!");
        }

    case TYPE_BEST:
      process_best (buf, insize, &inpos);
      return;

    case TYPE_BWEIGHT:
      {
        int w;
        
        if (rank == 0)
          error ("Message TYPE_BWEIGHT received by process 0.");
        ret = MPI_Unpack (buf, insize, &inpos, &w, 1, MPI_INT, MPI_COMM_WORLD);
        if (ret != MPI_SUCCESS)
          mpierror (ret, "MPI_Unpack");
        if (w < best->weight)
          best->weight = w;
        else
          fprintf (stderr, "[%d] received 'best' weight"
                  " that is worse than its 'best'!!!\n", rank);
        fprintf (stderr, "[%d] received new best weight=%d\n", rank, 
                 best->weight);
        return;
      }


    default:
      error ("Unhandled TYPE_* in process_message()!!!");
    }
}


int 
main (int argc, char * argv[])
{
  int ret;
  unsigned i, j;
  FILE * infile;


  initialize_mpi (&argc, &argv, &rank, &worldsize);
  /* Some basic checks and initialization. */
  if (argc < 2)
    {
      fprintf (stderr, "Pocet argumentu: %d\n", argc);
      for (i = 0; i < argc; ++i)
        fprintf (stderr, "`%s'\n", argv[i]);
      error ("Syntax: mrg <input graph>");
    }
  srandom (time (NULL));
  
  /* Open input file and read graph's dimension. */
  fprintf (stderr, "File to open: %s\n", argv[argc-1]); 
  infile = fopen (argv[argc-1], "r");
  if (! infile)
    error ("fopen()");
  ret = fscanf (infile, "%u", &N);
  if (ret < 1)
    error ("fscanf()");
  
  /* Allocate structures. */
  stack = list_new ();
  graph = trimatrix_new (N);
  weights = wtrimatrix_new (N);
  if (! stack || ! graph || ! weights)
    error ("Memory allocation failure");
  
  /* Read graph from file. */
  for (i = 1; i <= N; ++i)
    for (j = 1; j <= N; ++j)
      {
        unsigned val;
        ret = fscanf (infile, "%u", &val);
        if (ret < 1)
          error ("fscanf()");
        trimatrix_set (graph, i, j, val);
        if (val)
          wtrimatrix_set (weights, i, j, random () % 255 + 1);
      }

  /* Do the actual work here.  */
  initialize ();
  /* Synchronize before start of the computation. */
  MPI_Barrier (MPI_COMM_WORLD);
  while (1)
    {
      listelem_t * it;
      stkelem_t * el;
      int dnr, flag = 0;

      fflush (stdout);

      /* Probe for incoming messages and process them. */
      while (1)
        {
          flag = 0;
          //fprintf (stderr, "[%d] probing for messages\n", rank);
          ret = MPI_Iprobe (MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag,
                            &status);
          if (ret != MPI_SUCCESS)
            mpierror (ret, "MPI_Iprobe()");
          if (flag)
            {
              ret = MPI_Recv (recv_buf, recv_buf_len, MPI_PACKED, 
                              MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, 
                              &status);
              if (ret != MPI_SUCCESS)
                mpierror (ret, "MPI_Recv()");
              switch (status.MPI_TAG)
                {
                case TAG_NEEDS_ATTENTION:
                  process_clamour_message (recv_buf, recv_buf_len);
                  break;
                case TAG_CAN_WAIT:
                  process_message (recv_buf, recv_buf_len);
                  break;
                default:
                  error ("Unknown TAG_*!!!");
                }
              continue;
            }
          else
            break;
        }

      /* Idle while waiting for MSG_EOC from 0. */
      if (best->weight == 1)
        {
          sleep (1);
          continue;
        }

      /* Are we out of work? */
      if (list_size (stack) == 0)
        {
          fprintf (stderr, "[%d] out of work\n", rank);
          /* Deny any requests for work. */
          wouldgive = 0;

          /* First do the right thing with tokens. */
          do_tokens ();

          if (rank != 0)
            /* Request a donor from 0. */
            dnr = request_donor ();
          else
            {
              dnr = donor;
              donor = (donor + 1) % worldsize;
            }
          if (dnr != rank)
            /* Send request to obtained donor and read results. */
            request_work (dnr);

          /* At this point we should have some work to give. */
          wouldgive = 1;

          continue;
        }
      
      el = list_first (stack, &it);
      if (! el)
        {
          fprintf (stderr, "[%d] stack empty even after request for work, exiting\n",
                  rank);
          MPI_Finalize ();
          exit (EXIT_SUCCESS);
        }
      /* Move deeper in DFS tree if possible. */
      if (! el->uptodate)
        if (update_weight (el, el->next)) 
          /* It is, we are done. */
          {
            if (rank == 0)
              end_computation ();
            else
              continue;
          }
      if (generate_depth (stack, el))
        {
          /* Get the newly generated element. */
          el = list_first (stack, &it);
          /* Update weight of a new cut.
             Is this a cut of weight 1? 
             Note: el->next because nodes are numbered from 1. */
          if (update_weight (el, el->next)) 
            /* It is, we are done. */
            {
              if (rank == 0)
                end_computation ();
              else
                continue;
            }
          else
            continue;
        }
      else
        {
          stkelem_t * se = list_pop (stack);
          if (se != best)
            stkelem_delete (se);
        }
    }

  error ("Got out of the main loop, that should never happen!!");
}
