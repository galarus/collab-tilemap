#include <czmq.h>
#include <hiredis/hiredis.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <zsock.h>
#include <string.h>

redisContext *redis_context;
redisReply *redis_reply;

zsock_t * publisher;

typedef struct Rectangle {
  int x;
  int y;
  int colorNum;
} Rectangle;

enum { columns = 32, rows = 32 };

Rectangle board[columns][rows];

volatile int keep_running = 1;

void handle_sigint(int sig) {
  keep_running = 0;
  printf("stop running\n");
}

char* boardToCSV (Rectangle board[columns][rows]){
  static char buffer[10*rows*columns];
  buffer[0] = '\0';

  for (int i = 0; i < columns; i++){
    for (int j = 0; j < rows; j++){
      char temp[36];
      sprintf(temp, "%d,%d,%d\n", board[j][i].x, board[j][i].y, board[j][i].colorNum);
      strcat(buffer, temp);
    }
  }
  return buffer;
}

void parseBoardUpdate(char * received_str){
  printf("parsing input %s\n", received_str);
  char * publish_str = strdup(received_str);
  char * token;
  token = strtok(received_str, ",");
  int x;
  int y;
  int colorNum;
  int i = 0;
  while (token != NULL){
    if (i==0){
      x = atoi(token);
    }
    if (i==1){
      y = atoi(token);
    }
    if (i==2){
      colorNum = atoi(token);
    }
    token = strtok(NULL, ",");
    i++;
  }
  printf("setting %d, %d to %d\n", x, y, colorNum);
  Rectangle* currRec = &board[x][y];
  currRec->colorNum = colorNum;
  // board[y][x].colorNum = colorNum;
  //  SEND UPDATE TO DB
  //  send whole state for now - optimize this later

  printf("setting updated board to redis\n");
  //char *board_str = boardToCSV(board);
  //redis_reply = redisCommand(redis_context, "SET board %s", board_str);
  char board_name[16];
  snprintf(board_name, 16, "row:%d", y);
  redis_reply = redisCommand(redis_context, "LSET %s %d %d", board_name, x, colorNum);
  if (redis_reply->type == REDIS_REPLY_ERROR){
    printf("error %s\n", redis_reply->str);
  }
  freeReplyObject(redis_reply);

  // publish update to other subscribers
  zsock_send(publisher, "s", publish_str);
}

// different from server parseBoard - doesnt store exact X/Y/height/width
void parseBoardCSV(char* boardCSV){
  char* token;
  token = strtok(boardCSV, "\n");
  char* lines[columns*rows];
  int lineIdx = 0;
  while (token != NULL){
    lines[lineIdx] = token;
    token = strtok(NULL, "\n");
    lineIdx++;
  }
  for (int i = 0; i < lineIdx; i++){
    char * line = lines[i];
    //printf("%s\n", line);
    char* dataToken;
    //int lineData[3];
    dataToken = strtok(line, ",");
    int dataIdx = 0;
    int colorNum = 0;
    int x;
    int y;
    while (dataToken != NULL){
      switch(dataIdx){
        case 0: // x value
          x = atoi(dataToken);
          break;
        case 1: // y value
          y = atoi(dataToken);
          break;
        case 2: // colorNum
          colorNum = atoi(dataToken);
          break;
      }
    //  lineData[dataIdx] = atoi(dataToken);
      dataToken = strtok(NULL, ",");
      dataIdx++;
    }
 //   Tile * lineTile = &board[x][y];

    Rectangle *rect = &board[x][y];
    rect->x =  x;
    rect->y =  y;
    rect->colorNum = colorNum;
  }
}

void getBoardState(){
  //redis_reply = redisCommand(redis_context, "EXISTS board");

  redis_reply = redisCommand(redis_context, "EXISTS row:0");
  int board_exists = redis_reply->integer;
  freeReplyObject(redis_reply);
  if (board_exists != 1){
  // board doesn't exist. generate it.
    for (int i = 0; i < columns; i++) {
      char board_name[16];
      snprintf(board_name, 16, "row:%d", i);
      for (int j = 0; j < rows; j++) {
        Rectangle *rect = &board[j][i];
        rect->x = j;
        rect->y = i;
        int colorNum = rand() % 4;
        rect->colorNum = colorNum;
        redis_reply = redisCommand(redis_context, "RPUSH %s %d", board_name, colorNum);
        freeReplyObject(redis_reply);
      }
    }
    // set board to redis
    printf("setting generated board to redis\n");
    char * board_str = boardToCSV(board);
  //  redis_reply = redisCommand(redis_context, "SET board %s", board_str);
  } else {
    // board exists. get it from redis.
    printf("Get board from redis\n");
  //  redis_reply = redisCommand(redis_context, "GET board");

    for (int i = 0; i < columns; i++){
      char board_name[16];
      snprintf(board_name, 16, "row:%d", i);
      for (int j = 0; j < rows; j++){

        redis_reply = redisCommand(redis_context, "LINDEX %s %d", board_name, j);
        if (redis_reply->type == REDIS_REPLY_STRING){
          Rectangle *rect = &board[j][i];
          rect->x = j;
          rect->y = i;
          rect->colorNum = atoi(redis_reply->str);
        }
        freeReplyObject(redis_reply);
      }
    }
   // char * board_str = redis_reply->str;
    // populate global board variable
   //parseBoardCSV(board_str);
  }
}

int main (void) {

  // Connect to Redis server
  redis_context = redisConnect("127.0.0.1", 6379);
  if (redis_context == NULL || redis_context->err) {
    if (redis_context) {
      printf("Connection error: %s\n", redis_context->errstr);
      redisFree(redis_context);
    } else {
      printf("Connection error: can't allocate redis context\n");
    }
    exit(1);
  }
  redis_reply = redisCommand(redis_context, "PING %s", "hello world");
  printf("Response: %s\n", redis_reply->str);
  freeReplyObject(redis_reply);
  // Initialization of board
  //
  //--------------------------------------------------------------------------------------
  getBoardState();
  //char* boardCSV = boardToCSV(board);
  //size_t csv_size = strlen(boardCSV) * sizeof(char);
  //printf("%zu \n", csv_size);
  // exit program on ctrl-c
  if (signal(SIGINT, handle_sigint) == SIG_ERR){
    printf("error setting up signal handler \n");
    return 1;
  }
  
  zsock_t *responder = zsock_new (ZMQ_REP);
  int rc = zsock_bind(responder, "tcp://*:5555");
  assert (rc == 5555);
  printf("tcp listening on 5555 \n");

  //publisher socket
  publisher = zsock_new_pub("tcp://*:5556");
  if (!publisher){
    printf("Error: Unable to create publisher socket\n");
    return 1;
  }
  

  while (keep_running){
    char *received_str = zstr_recv (responder);
    if (received_str) {

      printf("received %s \n", received_str);
      if (strcmp(received_str, "fetch") == 0){
        zstr_send(responder, boardToCSV(board));
      }else {
        parseBoardUpdate(received_str);
        zstr_send(responder, "updated board");
      }
      // sleep(1);
      zstr_free(&received_str);
    } else {
      break;
      // end when NULL aka interrupted
    }
    //printf(str);
  }
  zsock_destroy(&responder);
  printf("server stopped gracefully\n");

  redisFree(redis_context);

  return 0;
}
