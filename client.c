#include <czmq.h>

int main (void) {
  printf("connecting to hello world server...\n");
  zsock_t *requester = zsock_new (ZMQ_REQ);
  zsock_connect (requester, "tcp://localhost:5555");

  int request_nbr;
  for (request_nbr = 0; request_nbr != 10; request_nbr++){
    printf("sending hello %d \n", request_nbr);
    zstr_send(requester, "hello!!!");
    char *str = zstr_recv(requester);
    printf("received world %d \n", request_nbr);
    zstr_free(&str);
  }
  zsock_destroy(&requester);
  return 0;
}
