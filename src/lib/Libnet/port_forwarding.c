/* port forwarding functions for TORQUE */

#include <pbs_config.h>   /* the master config generated by configure */

#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <netdb.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include "lib_ifl.h"
#include "lib_net.h"
#include <vector>


#include "port_forwarding.h"


/* handy utility to handle forwarding socket connections to another host
 * pass in an initialized pfwdsock struct with sockets to listen on, a function
 * pointer to get a new socket for forwarding, and a hostname and port number to
 * pass to the function pointer, and it will do the rest. The caller probably
 * should fork first since this function is an infinite loop and never returns */

/* __attribute__((noreturn)) - how do I do this portably? */

void port_forwarder(

  std::vector<pfwdsock> *socks,
  int (*connfunc)(char *, long, char *),
  char            *phost,
  int              pport,
  char            *EMsg)  /* O */

  {
  int                n;
  int                n2;
  int                sock;
  int                num_events;
  int                rc;
  struct sockaddr_in from;
  torque_socklen_t   fromlen;
  std::vector<pollfd> PollArray(NUM_SOCKS);

  while (1)
    {
    for (n = 0; n < NUM_SOCKS; n++)
      {
      // clear the entry
      PollArray.at(n).fd = -1;
      PollArray.at(n).events = 0;
      PollArray.at(n).revents = 0;

      if (!socks->at(n).active)
        continue;

      if (socks->at(n).listening)
        {
        PollArray.at(n).fd = socks->at(n).sock;
        PollArray.at(n).events = POLLIN;
        }
      else
        {
        if (socks->at(n).bufavail < BUF_SIZE)
          {
          PollArray.at(n).fd = socks->at(n).sock;
          PollArray.at(n).events = POLLIN;
          }

        if (socks->at(socks->at(n).peer).bufavail - socks->at(socks->at(n).peer).bufwritten > 0)
          {
          PollArray.at(n).fd = socks->at(n).sock;
          PollArray.at(n).events |= POLLOUT;
          }
        }
      }

    num_events = poll(&PollArray[0], NUM_SOCKS, -1);

    if ((num_events == -1) && (errno == EINTR))
      continue;

    if (num_events < 0)
      {
      perror("port forwarding poll()");

      exit(EXIT_FAILURE);
      }

    for (n = 0; (num_events > 0) && (n < NUM_SOCKS); n++)
      {
      // skip entry with no return events
      if (PollArray.at(n).revents == 0)
        continue;

      // decrement the count of events returned
      num_events--;

      if (!socks->at(n).active)
        continue;

      if ((PollArray.at(n).revents & POLLIN))
        {
        if (socks->at(n).listening)
          {
          int newsock = 0, peersock = 0;
          fromlen = sizeof(from);
          
          if ((sock = accept(socks->at(n).sock, (struct sockaddr *) & from, &fromlen)) < 0)
            {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR) || (errno == ECONNABORTED))
              continue;

            close(socks->at(n).sock);

            socks->at(n).active = 0;

            continue;
            }

          newsock = peersock = 0;

          for (n2 = 0; n2 < NUM_SOCKS; n2++)
            {
            if (socks->at(n2).active || ((socks->at(n2).peer != 0) && socks->at(socks->at(n2).peer).active))
              continue;

            if (newsock == 0)
              newsock = n2;
            else if (peersock == 0)
              peersock = n2;
            else
              break;
            }

          socks->at(newsock).sock      = socks->at(peersock).remotesock = sock;
          socks->at(newsock).listening = socks->at(peersock).listening = 0;
          socks->at(newsock).active    = socks->at(peersock).active    = 1;
          socks->at(newsock).peer      = socks->at(peersock).sock      = connfunc(phost, pport, EMsg);
          socks->at(newsock).bufwritten = socks->at(peersock).bufwritten = 0;
          socks->at(newsock).bufavail  = socks->at(peersock).bufavail  = 0;
          socks->at(newsock).buff[0]   = socks->at(peersock).buff[0]   = '\0';
          socks->at(newsock).peer      = peersock;
          socks->at(peersock).peer     = newsock;
          }
        else
          {
          /* non-listening socket to be read */

          rc = read_ac_socket(
                 socks->at(n).sock,
                 socks->at(n).buff + socks->at(n).bufavail,
                 BUF_SIZE - socks->at(n).bufavail);

          if (rc < 1)
            {
            shutdown(socks->at(n).sock, SHUT_RDWR);
            close(socks->at(n).sock);
            socks->at(n).active = 0;
            }
          else
            {
            socks->at(n).bufavail += rc;
            }
          }
        } /* END if read */

      if ((PollArray.at(n).revents & POLLOUT))
        {
        int peer = socks->at(n).peer;

        rc = write_ac_socket(
               socks->at(n).sock,
               socks->at(peer).buff + socks->at(peer).bufwritten,
               socks->at(peer).bufavail - socks->at(peer).bufwritten);

        if (rc < 1)
          {
          shutdown(socks->at(n).sock, SHUT_RDWR);
          close(socks->at(n).sock);
          socks->at(n).active = 0;
          }
        else
          {
          socks->at(peer).bufwritten += rc;
          }
        } /* END if write */
      } /* END foreach fd */

    for (n2 = 0; n2 <= 1; n2++)
      {
      for (n = 0; n < NUM_SOCKS; n++)
        {
        int peer;

        if (!socks->at(n).active || socks->at(n).listening)
          continue;

        peer = socks->at(n).peer;

        if (socks->at(n).bufwritten == socks->at(n).bufavail)
          {
          socks->at(n).bufwritten = socks->at(n).bufavail = 0;
          }

        if (!socks->at(peer).active && (socks->at(peer).bufwritten == socks->at(peer).bufavail))
          {
          shutdown(socks->at(n).sock, SHUT_RDWR);
          close(socks->at(n).sock);

          socks->at(n).active = 0;
          }
        }
      }
    }   /* END while(1) */

    // never executed but should satisfy code profilers looking
    // for a malloc/free pairing
    delete(socks);
  }     /* END port_forwarder() */






/* disable nagle on a socket */

void set_nodelay(

  int fd)

  {
  int opt;
  torque_socklen_t optlen;

  optlen = sizeof(opt);

  if (getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, &optlen) == -1)
    {
    fprintf(stderr, "getsockopt TCP_NODELAY: %.100s", strerror(errno));
    return;
    }

  if (opt == 1)
    {
    fprintf(stderr, "fd %d is TCP_NODELAY", fd);
    return;
    }

  opt = 1;

  if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof opt) == -1)
    fprintf(stderr, "setsockopt TCP_NODELAY: %.100s", strerror(errno));

  return;
  }




/* return a socket to the specified X11 unix socket */

int connect_local_xsocket(

  u_int dnr)

  {
  int sock;

  struct sockaddr_un addr;

  if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
    fprintf(stderr, "socket: %.100s", strerror(errno));
    return -1;
    }

  memset(&addr, 0, sizeof(addr));

  addr.sun_family = AF_UNIX;
  snprintf(addr.sun_path, sizeof addr.sun_path, X_UNIX_PATH, dnr);

  if (connect(sock, (struct sockaddr *) & addr, sizeof(addr)) == 0)
    return sock;

  close(sock);

  fprintf(stderr, "connect %.100s: %.100s", addr.sun_path, strerror(errno));

  return(-1);
  }





int x11_connect_display(

  char *display,
  long  alsounused,
  char *EMsg)        /* O */

  {
#ifndef HAVE_GETADDRINFO
  /* this was added for cygwin which doesn't seem to have a working
   * getaddrinfo() yet.
   * this will have to be figured out later */

  if (EMsg != NULL)
    EMsg[0] = '\0';

  return(-1);

#else

  int display_number, sock = 0;

  char buf[1024], *cp;

  struct addrinfo hints, *ai, *aitop;
#ifdef BIND_OUTBOUND_SOCKETS
  struct sockaddr_in      local;
#endif

  char strport[NI_MAXSERV];

  int gaierr;

  if (EMsg != NULL)
    EMsg[0] = '\0';

  /*
  * Now we decode the value of the DISPLAY variable and make a
  * connection to the real X server.
  */

  /*
  * Check if it is a unix domain socket.  Unix domain displays are in
  * one of the following formats: unix:d[.s], :d[.s], ::d[.s]
  */
  if (strncmp(display, "unix:", 5) == 0 ||
      display[0] == ':')
    {
    /* Connect to the unix domain socket. */
    if (sscanf(strrchr(display, ':') + 1, "%d", &display_number) != 1)
      {
      fprintf(stderr, "Could not parse display number from DISPLAY: %.100s",
              display);
      return -1;
      }

    /* Create a socket. */
    sock = connect_local_xsocket(display_number);

    if (sock < 0)
      return -1;

    /* OK, we now have a connection to the display. */
    return sock;
    }

  /*
  * Connect to an inet socket.  The DISPLAY value is supposedly
  * hostname:d[.s], where hostname may also be numeric IP address.
  */
  snprintf(buf, sizeof(buf), "%s", display);

  cp = strchr(buf, ':');

  if (!cp)
    {
    fprintf(stderr, "Could not find ':' in DISPLAY: %.100s", display);
    return -1;
    }

  *cp = 0;

  /* buf now contains the host name.  But first we parse the display number. */

  if (sscanf(cp + 1, "%d", &display_number) != 1)
    {
    fprintf(stderr, "Could not parse display number from DISPLAY: %.100s",
            display);
    return -1;
    }

  /* Look up the host address */
  memset(&hints, 0, sizeof(hints));

  hints.ai_family = AF_UNSPEC;

  hints.ai_socktype = SOCK_STREAM;

  snprintf(strport, sizeof strport, "%d", 6000 + display_number);

  if ((gaierr = getaddrinfo(buf, strport, &hints, &aitop)) != 0)
    {
    fprintf(stderr, "%100s: unknown host. (%s)", buf, gai_strerror(gaierr));
    return -1;
    }

#ifdef BIND_OUTBOUND_SOCKETS
  if (get_local_address(local) != PBSE_NONE)
    {
    fprintf(stderr, "could not determine local IP address: %s", strerror(errno));
    return(-1);
    }
#endif

  for (ai = aitop; ai; ai = ai->ai_next)
    {
    /* Create a socket. */
    sock = socket(ai->ai_family, SOCK_STREAM, 0);

    if (sock < 0)
      {
      fprintf(stderr, "socket: %.100s", strerror(errno));
      continue;
      }

#ifdef BIND_OUTBOUND_SOCKETS
    /* Bind to the IP address associated with the hostname, in case there are
     * muliple possible source IPs for this destination.*/

    // don't bind localhost addr
    if (!islocalhostaddr(&local))
      {
      if (bind(sock, (struct sockaddr *)&local, sizeof(sockaddr_in)))
        {
        fprintf(stderr, "could not bind local socket: %s", strerror(errno));
        close(sock);
        continue;
        }
      }

#endif

    /* Connect it to the display. */
    if (connect(sock, ai->ai_addr, ai->ai_addrlen) < 0)
      {
      fprintf(stderr, "connect %.100s port %d: %.100s", buf,
              6000 + display_number, strerror(errno));
      close(sock);
      continue;
      }

    /* Success */
    break;
    }

  freeaddrinfo(aitop);

  if (!ai)
    {
    fprintf(stderr, "connect %.100s port %d: %.100s", buf, 6000 + display_number,
            strerror(errno));
    return -1;
    }

  set_nodelay(sock);

  return sock;
#endif /* HAVE_GETADDRINFO */
  }


