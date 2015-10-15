
/* Copyright 1998 by the Massachusetts Institute of Technology.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in
 * advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 */

#include "ares_setup.h"

#include "ares.h"
#include "ares_nowarn.h"
#include "ares_private.h"

int ares_fds(ares_channel channel, fd_set *read_fds, fd_set *write_fds)
{
  struct server_state *server;
  ares_socket_t nfds;
  int i;

  /* Are there any active queries? */
  int active_queries = !ares__is_list_empty(&(channel->all_queries));

  nfds = 0;
  for (i = 0; i < channel->nservers; i++)
    {
      server = &channel->servers[i];
      /* We only need to register interest in UDP sockets if we have
       * outstanding queries.
       */
      if (active_queries && server->udp_socket != ARES_SOCKET_BAD)
        {
          FD_SET(server->udp_socket, read_fds);
          if (server->udp_socket >= nfds)
            nfds = server->udp_socket + 1;
        }
      /* We always register for TCP events, because we want to know
       * when the other side closes the connection, so we don't waste
       * time trying to use a broken connection.
       */
      if (server->tcp_socket != ARES_SOCKET_BAD)
       {
         FD_SET(server->tcp_socket, read_fds);
         if (server->qhead)
           FD_SET(server->tcp_socket, write_fds);
         if (server->tcp_socket >= nfds)
           nfds = server->tcp_socket + 1;
       }
    }
  return (int)nfds;
}

static int next_pollfd(struct ares_pollfds *old_fds)
{
  struct ares_pollfds new_fds = *old_fds;
  new_fds.nfds++;
  if (new_fds.nfds == old_fds->cap) {
    new_fds.cap *= 2;
    new_fds.fds = realloc(new_fds.fds, new_fds.cap * sizeof(*new_fds.fds));
    if (!new_fds.fds)
      return ARES_ENOMEM;
  }
  *old_fds = new_fds;
  return 0;
}

static struct pollfd *curr_pollfd(struct ares_pollfds *fds)
{
  return fds->fds + fds->nfds;
}

int ares_poll_fds(ares_channel channel, struct ares_pollfds *fds)
{
  struct server_state *server;
  int i;
  int err;

  /* Are there any active queries? */
  int active_queries = !ares__is_list_empty(&(channel->all_queries));

  if (fds->cap == 0) {
    fds->cap = 4;
    fds->fds = calloc(fds->cap, sizeof(*fds->fds));
    if (!fds->fds)
      return ARES_ENOMEM;
  }
  fds->nfds = 0;
  for (i = 0; i < channel->nservers; i++)
    {
      server = &channel->servers[i];
      /* We only need to register interest in UDP sockets if we have
       * outstanding queries.
       */
      if (active_queries && server->udp_socket != ARES_SOCKET_BAD)
        {
          struct pollfd *p = curr_pollfd(fds);
          p->fd = server->udp_socket;
          p->events = POLLIN;
          p->revents = 0;
          err = next_pollfd(fds);
          if (err)
            return err;
        }
      /* We always register for TCP events, because we want to know
       * when the other side closes the connection, so we don't waste
       * time trying to use a broken connection.
       */
      if (server->tcp_socket != ARES_SOCKET_BAD)
        {
          struct pollfd *p = curr_pollfd(fds);
          p->fd = server->tcp_socket;
          p->events = POLLIN;
          p->revents = 0;
          if (server->qhead)
            p->events |= POLLOUT;
          err = next_pollfd(fds);
          if (err)
            return err;
       }
    }
  return 0;
}
