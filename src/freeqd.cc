#include <config.h>
#include "system.h"
#include "progname.h"

#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <nanomsg/nn.h>
#include <nanomsg/pipeline.h>

#define DEBUG(X) fprintf(stderr, _("DEBUG: %s\n"), X);

char *date ()
{
  time_t raw = time (&raw);
  struct tm *info = localtime (&raw);
  char *text = asctime (info);
  text[strlen(text)-1] = '\0'; // remove '\n'
  return text;
}

int receiver (const char *url)
{
  int sock = nn_socket (AF_SP, NN_PULL);
  assert (sock >= 0);
  assert (nn_bind (sock, url) >= 0);
  DEBUG("freeqd receiver is ready");
  while (1)
    {
      char *buf = NULL;
      int bytes = nn_recv (sock, &buf, NN_MSG, 0);
      assert (bytes >= 0);
      DEBUG("got message");
      nn_freemsg (buf);
    }
}

int main (const int argc, const char **argv)
{

  return receiver("ipc:///tmp/freeqd.ipc");

}
