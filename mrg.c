#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <mpi.h>
#include "config.h"
#include "bitmap.h"
#include "matrix.h"
#include "list.h"


#define TYPE_MSG 'M' /* A message. See MSG_*. */
#define TYPE_STKELEM 'S' /* Stack element, as response to a request. */
#define TYPE_BEST 'B' /* Best stack element. */
#define TYPE_BWEIGHT 'C' /* Best weight. */
#define TYPE_GRAPH 'G' /* Graph */
#define TYPE_GWEIGHTS 'H' /* Weights of graph's edges. */
#define TYPE_TOKEN  'T' /* Token for ADUV. */
#define TYPE_DONOR 'D' /* Answer to a donor request. */

#define MSG_EOC 'E' /* End of computation. */
#define MSG_REQ 'R' /* Request work. */
#define MSG_DENY 'D' /* Deny work. */
#define MSG_DREQ 'O' /* Request donor from P1. */
#define MSG_EOE 'F' /* No more stack elements are coming. */

#define TOKEN_BLACK 'B'
#define TOKEN_WHITE 'W'

struct _msg_t
{
  char type;
  char buf[1];
};
typedef struct _msg_t msg_t;

/* Message receive buffer. */
msg_t * recv_buf;
size_t recv_buf_len;
/* */
msg_t * graph_buf;
msg_t * weights_buf;
/* Predefined messages. */
msg_t donor_msg = {TYPE_DONOR, {'\0'}};
const msg_t eoc_msg = {TYPE_MSG, {MSG_EOC}};
const msg_t req_msg = {TYPE_MSG, {MSG_REQ}};
const msg_t deny_msg = {TYPE_MSG, {MSG_DENY}};
const msg_t dreq_msg = {TYPE_MSG, {MSG_DREQ}};
const msg_t eoe_msg = {TYPE_MSG, {MSG_EOE}};
const msg_t btoken_msg = {TYPE_TOKEN, {TOKEN_BLACK}};
const msg_t wtoken_msg = {TYPE_TOKEN, {TOKEN_WHITE}};
/* GCZ-AHD counter. */
int donor;
/* ADUV */
char mycolor, token;
/* MPI status. */
MPI_Status status;


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
stkelem_serialize (void * buf, size_t * size, size_t * pos,
                   const stkelem_t * se)
{
  size_t sz = 0, bmsz;

  *(int *)(buf + *pos) = se->uptodate;
  sz += sizeof (int);
  *pos += sizeof (int);

  *(int *)(buf + *pos) = se->weight;
  sz += sizeof (int);
  *pos += sizeof (int);

  *(unsigned *)(buf + *pos) = se->next;
  sz += sizeof (unsigned);
  *pos += sizeof (unsigned);

  bitmap_serialize (buf, &bmsz, pos, se->set);
  sz += bmsz;
}


/**

*/
stkelem_t * 
stkelem_deserialize (const void * buf, size_t * pos)
{
  stkelem_t * se;
  size_t p = *pos;
  
  se = malloc (sizeof (stkelem_t));
  if (! se)
    return NULL;

  se->uptodate = *(int *)(buf + p);
  p += sizeof (int);

  se->weight = *(int *)(buf + p);
  p += sizeof (int);

  se->next = *(unsigned *)(buf + p);
  p += sizeof (unsigned);

  se->set = bitmap_deserialize (buf, &p);
  if (! se->set)
    {
      free (se);
      return NULL;
    }
  
  *pos = p;
  return se;
}


/**
   Prints msg and possible message for errno to stderr and 
   exits with EXIT_FAILURE.
   @param msg user supplied message
*/
void error (const char * msg) __attribute__((noreturn));
void 
error (const char * msg)
{
  if (! errno)
    fprintf (stderr, "%s\n", msg);
  else
    fprintf (stderr, "%s: %s\n", msg, strerror (errno));
  exit (EXIT_FAILURE);
}


/**
   Prints msg and possible message for MPI error to stderr and 
   exits with EXIT_FAILURE.
   @param msg user supplied message
*/
void mpierror (int ret, const char * msg) __attribute__((noreturn));
void 
mpierror (int ret, const char * msg)
{
  int len;
  char str[1000];

  MPI_Error_string (ret, str, &len);
  fprintf (stderr, "%s: %s\n", msg, str);
  exit (EXIT_FAILURE);
}



/**
   Initializes stack for DFS algoritm.
*/
void 
initialize_stack (void)
{
  stkelem_t * el = stkelem_new (N, 0, 0, 1);
  
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
  recv_buf_len = 1 + stkelem_serialize_size (best);
  recv_buf = malloc (recv_buf_len);
  /* Other buffers. */
  /*graph_buf = malloc (1 + trimatrix_serialize_size (graph));*/
  /*weights_buf = malloc (1 + wtrimatrix_serialize_size (weights));*/
  if (!best || ! recv_buf /*|| ! graph_buf || ! weights_buf*/)
    error ("Memory allocation failure");
  /* The rest. */
  token = TOKEN_WHITE;
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
  printf ("[%d] reporting\n", *rank);
  ret = MPI_Comm_size(MPI_COMM_WORLD, size);
  if (ret != MPI_SUCCESS)
    mpierror (ret, "MPI_Comm_size");
  printf ("[%d] worldsize=%d\n", *rank, worldsize);
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

  if (node == 0)
    abort ();

  for (i = 1; i <= N; ++i)
    {
      if (i == node)
        continue;
      if (trimatrix_get (graph, node, i))
        {
          /* Is node i in set Y? */
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
      best = el;
      if (rank != 0)
        {
          size_t pos = 0, size;
          int ret, i;
          
          recv_buf->type = TYPE_BEST;
          stkelem_serialize (recv_buf->buf, &size, &pos, best);
          ret = MPI_Send (recv_buf, recv_buf_len, MPI_CHAR, 0, 1,
                          MPI_COMM_WORLD);
          if (ret != MPI_SUCCESS)
            mpierror (ret, "MPI_Send()");
          recv_buf->type = TYPE_BWEIGHT;
          *(int *)recv_buf->buf = best->weight;
          for (i = 1; i < worldsize; ++i)
            {
              ret = MPI_Send (recv_buf, 1 + sizeof (int), MPI_CHAR, i, 1,
                              MPI_COMM_WORLD);
              if (ret != MPI_SUCCESS)
                mpierror (ret, "MPI_Send()");
            }
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


void end_computation (void)
{
  /* End of computation. */
  int i;
  for (i = 1; i < worldsize; ++i)
    {
      MPI_Send ((void *)&eoc_msg, sizeof (msg_t), MPI_CHAR, i,
                1, MPI_COMM_WORLD);
    }
  /* Print out the solution. */
  printf ("Weight of the best solution: %d\n", best->weight);
  printf ("Set X:");
  for (i = 0; i < bitmap_size (best->set); ++i)
    {
      int b = bitmap_getbit (best->set, i);
      if (! b)
        printf (" %d", i+1);
    }
  printf ("\n");
  printf ("Set Y:");
  for (i = 0; i < bitmap_size (best->set); ++i)
    {
      int b = bitmap_getbit (best->set, i);
      if (b)
        printf (" %d", i+1);
    }
  printf ("\n");

  MPI_Finalize ();
  exit (EXIT_SUCCESS);
}


void process_message (msg_t * msg)
{
  int ret;

  switch (msg->type)
    {
    case TYPE_MSG:
      switch (msg->buf[0])
        {
        case MSG_REQ:
          {
            listelem_t * it;
            stkelem_t * el;
            unsigned half;
            int i;
            list_t * tmplist;
            
            /* Do we have anything to give? */
            if (list_size (stack) == 0)
              {
                /* Nope, deny the request. */
                ret = MPI_Send ((void *)&deny_msg, sizeof (msg_t), MPI_CHAR,
                                status.MPI_SOURCE, 1, MPI_COMM_WORLD);
                if (ret != MPI_SUCCESS)
                  mpierror (ret, "MPI_Send()");
                return;
              }

            tmplist = list_new ();
            if (! tmplist)
              error ("Memory allocation failure.");
            el = list_last (stack, &it);
            if (! el->uptodate)
              update_weight (el, el->next);
            half = (N - el->next) / 2;
            if (half != 0 && rank > status.MPI_SOURCE)
              /* Chage token. */
              mycolor = TOKEN_BLACK;
            /* Generate the half. */
            for (i = 1; i <= half; ++i)
              generate_depth (tmplist, el);
            /* Send the half to requester. */
            while (list_size (tmplist) != 0)
              {
                stkelem_t * tmpel;
                size_t pos = 0, size;
                
                tmpel = list_popback (tmplist);
                stkelem_serialize (recv_buf->buf, &size, &pos, tmpel);
                ret = MPI_Send (recv_buf, recv_buf_len, MPI_CHAR, 
                                status.MPI_SOURCE, 1, MPI_COMM_WORLD);
                if (ret != MPI_SUCCESS)
                  mpierror (ret, "MPI_Send()");
                stkelem_delete (tmpel);
              }
            ret = MPI_Send ((void *)&eoe_msg, sizeof (msg_t), MPI_CHAR,
                            status.MPI_SOURCE, 1, MPI_COMM_WORLD);
            if (ret != MPI_SUCCESS)
              mpierror (ret, "MPI_Send()");
            return;
          }

        case MSG_DREQ:
          if (rank == 0)
            {
              donor_msg.buf[0] = (char)donor;
              donor = (donor + 1) % worldsize;
              ret = MPI_Send (&donor_msg, sizeof (msg_t), MPI_CHAR,
                              status.MPI_SOURCE, 1, MPI_COMM_WORLD);
              if (ret != MPI_SUCCESS)
                mpierror (ret, "MPI_Send()");
              return;
            }
          else
            error ("Donor request to process != P1!!!");

        case MSG_DENY:
          {
            int src;
            char type;

            /* Send request for donor to P1. */
            ret = MPI_Send ((void *)&dreq_msg, sizeof (msg_t), MPI_CHAR, 0, 1,
                            MPI_COMM_WORLD);
            if (ret != MPI_SUCCESS)
              mpierror (ret, "MPI_Send()");
            /* Wait for an answer. */
            do
              {
                ret = MPI_Recv (recv_buf, recv_buf_len, MPI_CHAR, 
                                MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &status);
                if (ret != MPI_SUCCESS)
                  mpierror (ret, "MPI_Recv()");
                src = status.MPI_SOURCE;
                type = recv_buf->type;
                process_message (recv_buf);
              }
            while (src != 0 || type != TYPE_DONOR);
            return;
          }

        case MSG_EOE:
          return;

        case MSG_EOC:
          exit (EXIT_SUCCESS);

        default:
          error ("Unknown MSG_*!!!");
        }

    case TYPE_DONOR:
      {
        int dnr = msg->buf[0];
        int src;
        char type, m;

        ret = MPI_Send ((void *)&req_msg, sizeof (msg_t), MPI_CHAR, 
                        msg->buf[0], 1, MPI_COMM_WORLD);
        if (ret != MPI_SUCCESS)
          mpierror (ret, "MPI_Send()");
        do
          {
            ret = MPI_Recv (recv_buf, recv_buf_len, MPI_CHAR, MPI_ANY_SOURCE,
                            1, MPI_COMM_WORLD, &status);
            if (ret != MPI_SUCCESS)
              mpierror (ret, "MPI_Recv()");
            src = status.MPI_SOURCE;
            type = recv_buf->type;
            m = recv_buf->buf[0];
            process_message (recv_buf);
          }
        while (src != dnr 
               || ! ((type == TYPE_MSG && (m == MSG_EOE 
                                           || m == MSG_DENY))
                     || type == TYPE_STKELEM));
        return;
      }

    case TYPE_STKELEM:
      {
        stkelem_t * se;
        unsigned pos = 0;

        se = stkelem_deserialize (msg->buf, &pos);
        if (! se)
          error ("Memory allocation problem.");
        if (! list_pushback (stack, se))
          error ("Memory allocation problem.");
        return;
      }
      
    case TYPE_BEST:
      {
        stkelem_t * se;
        unsigned pos = 0;

        if (rank != 0)
          error ("'Best' stack element received by process != P1!!!");
        
        se = stkelem_deserialize (msg->buf, &pos);
        if (! se)
          error ("Memory allocation problem.");
        if (se->weight < best->weight)
          {
            best = se;
            if (best->weight == 1)
              end_computation ();
          }
        else
          {
            stkelem_delete (se);
            printf ("Process %d received 'best' that"
                    " is worse than its 'best'!!!\n", rank);
          }
        return;
      }

    case TYPE_BWEIGHT:
      {
        if (*(int *)msg->buf < best->weight)
          best->weight = *(int *)msg->buf;
        else
          printf ("Process %d received 'best' weight"
                  " that is worse than its 'best'!!!\n", rank);
        return;
      }

    case TYPE_TOKEN:
      {
        if (rank == 0)
          {
            if (msg->buf[0] == TOKEN_WHITE)
              end_computation ();
            else
              token = TOKEN_WHITE;
          }
        else
          {
            if (mycolor == TOKEN_WHITE)
              token = msg->buf[0];
            else
              token = TOKEN_BLACK;
          }
        return;
      }

    default:
      error ("Unknown recv_buf->type!!!");
    }
}




int 
main (int argc, char * argv[])
{
  int ret;
  unsigned i, j;
  FILE * infile;

  /* Some basic checks and initialization. */
  if (argc < 2)
    {
      printf ("Pocet argumentu: %d\n", argc);
      for (i = 0; i < argc; ++i)
        printf ("`%s'\n", argv[i]);
      error ("Syntax: mrg <input graph>");
    }
  srandom (time (NULL));
  
  /* Open input file and read graph's dimension. */
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
  initialize_mpi (&argc, &argv, &rank, &worldsize);
  initialize ();
  while (1)
    {
      listelem_t * it;
      stkelem_t * el;
      int src, dnr, flag = 0;
      char type, msg;

      /* Probe for incoming messages and process them. */
      while (1)
        {
          ret = MPI_Iprobe (MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &flag, &status);
          if (ret != MPI_SUCCESS)
            mpierror (ret, "MPI_Iprobe()");
          if (flag)
            {
              ret = MPI_Recv (recv_buf, recv_buf_len, MPI_CHAR, MPI_ANY_SOURCE,
                              1, MPI_COMM_WORLD, &status);
              if (ret != MPI_SUCCESS)
                mpierror (ret, "MPI_Recv()");
              process_message (recv_buf);
              continue;
            }
          else
            break;
        }

      /* Are we out of work? */
      if (list_size (stack) == 0)
        {
          if (rank == 0)
            {
              /* Send white token to P2. */
              ret = MPI_Send ((void *)&wtoken_msg, sizeof (msg_t), MPI_CHAR, 1, 
                              1, MPI_COMM_WORLD);
              if (ret != MPI_SUCCESS)
                mpierror (ret, "MPI_Send()");
            }
          else
            {
              recv_buf->type = TYPE_TOKEN;
              recv_buf->buf[0] = token;
              ret = MPI_Send (recv_buf, sizeof (msg_t), MPI_CHAR, 
                              (rank + 1)  % worldsize, 1, MPI_COMM_WORLD);
              if (ret != MPI_SUCCESS)
                mpierror (ret, "MPI_Send()");
              mycolor = TOKEN_WHITE;
            }
          /* Send request for donor. */
          ret = MPI_Send ((void *)&dreq_msg, sizeof (msg_t), MPI_CHAR, 0, 
                          1, MPI_COMM_WORLD);
          if (ret != MPI_SUCCESS)
            mpierror (ret, "MPI_Send()");
          /* Wait for an answer. */
          do
            {
              ret = MPI_Recv (recv_buf, recv_buf_len, MPI_CHAR, MPI_ANY_SOURCE,
                              1, MPI_COMM_WORLD, &status);
              if (ret != MPI_SUCCESS)
                mpierror (ret, "MPI_Recv()");
              src = status.MPI_SOURCE;
              type = recv_buf->type;
              process_message (recv_buf);
            }
          while (src != 0 || type != TYPE_DONOR);
          /* Send request to obtained donor and read results. */
          dnr = recv_buf->buf[0];
          ret = MPI_Send ((void *)&req_msg, sizeof (msg_t), MPI_CHAR, dnr,
                          1, MPI_COMM_WORLD);
          if (ret != MPI_SUCCESS)
            mpierror (ret, "MPI_Send()");
          do 
            {
              ret = MPI_Recv (recv_buf, recv_buf_len, MPI_CHAR, MPI_ANY_SOURCE,
                              1, MPI_COMM_WORLD, &status);
              src = status.MPI_SOURCE;
              type = recv_buf->type;
              msg = recv_buf->buf[0];
              process_message (recv_buf);
            }
          while (src != dnr 
                 || ! ((type == TYPE_MSG && msg == MSG_EOE)
                       || type == TYPE_STKELEM));
          continue;
        }
      
      el = list_first (stack, &it);
      if (! el)
        error ("Nothing on stack. Suspicious!!");
      /* Move deeper in DFS tree if possible. */
      if (! el->uptodate)
        if (update_weight (el, el->next)) 
          /* It is, we are done. */
          {
            do
              {
                ret = MPI_Recv (recv_buf, recv_buf_len, MPI_CHAR,
                                MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &status);
                if (ret != MPI_SUCCESS)
                  mpierror (ret, "MPI_Recv");
              }
            while (status.MPI_SOURCE != 0 
                   || recv_buf->type != TYPE_MSG
                   || recv_buf->buf[0] != MSG_EOC);
            exit (EXIT_SUCCESS);
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
              do
                {
                  ret = MPI_Recv (recv_buf, recv_buf_len, MPI_CHAR,
                                  MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &status);
                  if (ret != MPI_SUCCESS)
                    mpierror (ret, "MPI_Recv");
                }
              while (status.MPI_SOURCE != 0 
                     || recv_buf->type != TYPE_MSG
                     || recv_buf->buf[0] != MSG_EOC);
              exit (EXIT_SUCCESS);
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

  /* Print out the solution. */
  printf ("Weight of the best solution: %d\n", best->weight);
  printf ("Set X:");
  for (i = 0; i < bitmap_size (best->set); ++i)
    {
      int b = bitmap_getbit (best->set, i);
      if (! b)
	printf (" %d", i+1);
    }
  printf ("\n");
  printf ("Set Y:");
  for (i = 0; i < bitmap_size (best->set); ++i)
    {
      int b = bitmap_getbit (best->set, i);
      if (b)
	printf (" %d", i+1);
    }
  printf ("\n");

  MPI_Finalize ();
  exit (EXIT_SUCCESS);
}
