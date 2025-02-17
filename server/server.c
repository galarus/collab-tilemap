#include <czmq.h>
#include <hiredis/hiredis.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <zsock.h>
#include <string.h>

// CONSTANT PROGRAM VARIABLES
// --------------------------

#define INIT_COLUMNS 32
#define INIT_ROWS 32

// redisContext* redis_context;
// redisReply* redis_reply;

zsock_t *publisher;
zsock_t *responder;

// Tile struct that is different from client because it does not need to store the rectangle width/height
typedef struct Tile
{
  int x;
  int y;
  int color_num;
} Tile;

// Server Board struct that is similar to the client TileBoard, but the tiles do not contain rectangles with width/height
typedef struct
{
  int rows;
  int columns;
  Tile **tiles;
} Board;

// getBoardTile
// a way to access tiles from the TileBoard that checks for out of bounds issues
Tile *getBoardTile(Board *board, int row_idx, int col_idx)
{
  if (row_idx > board->rows)
  {
    fprintf(stderr, "out of bounds access board rows\n");
    exit(1);
  }
  if (col_idx > board->columns)
  {
    fprintf(stderr, "out of bounds access board columns\n");
    exit(1);
  }
  return &board->tiles[row_idx][col_idx];
}

// initBoard
// call to init with the default number of rows and columns before doing anything else
void initBoard(Board *board)
{
  board->columns = INIT_COLUMNS;
  board->rows = INIT_ROWS;
  board->tiles = (Tile **)malloc(INIT_ROWS * sizeof(Tile *));
  if (board->tiles == NULL)
  {
    fprintf(stderr, "error allocating tile rows array\n");
    exit(1);
  }
  // allocate memory and initialize values
  for (int i = 0; i < INIT_ROWS; i++)
  {
    board->tiles[i] = (Tile *)malloc(INIT_COLUMNS * sizeof(Tile));
    if (board->tiles[i] == NULL)
    {
      fprintf(stderr, "error allocating tile columns array\n");
      exit(1);
    }
    for (int j = 0; j < INIT_COLUMNS; j++)
    {
      Tile *tile = getBoardTile(board, i, j); // &board->tiles[i][j];
      int color_num = rand() % 4;
      tile->color_num = color_num;
      tile->x = j;
      tile->y = i;
    }
  }
}

// resizeBoardWidth
// take a new width and reallocate memory for it
// initialize new values if new width is bigger than old width
// updating board[][COLUMNS]
void resizeBoardWidth(Board *board, int new_width)
{
  int old_width = board->columns;
  board->columns = new_width;

  // resize every row to new width
  for (int i = 0; i < board->rows; i++)
  {
    board->tiles[i] = (Tile *)realloc(board->tiles[i], new_width * sizeof(Tile));
    // initialize data for new columns
    for (int j = old_width; j < new_width; j++)
    {
      Tile *tile = getBoardTile(board, i, j);
      tile->color_num = 0;
      tile->x = j;
      tile->y = i;
    }
  }
}

// resizeBoardHeight
// take a new height and reallocate memory for it
// initialize new values if new height is bigger than old height
// pudating board[ROWS][]
void resizeBoardHeight(Board *board, int new_height)
{
  int old_rows = board->rows;
  // free old row data
  if (new_height < old_rows)
  {
    for (int i = new_height; i < old_rows; i++)
    {
      free(board->tiles[i]);
    }
  }

  board->rows = new_height;
  // init memory for new rows
  Tile **temp_realloc = (Tile **)realloc(board->tiles, new_height * sizeof(Tile *));
  if (temp_realloc == NULL)
  {
    fprintf(stderr, "error realloc height/rows\n");
    exit(1);
  }
  board->tiles = temp_realloc;

  // initialize data for new rows
  for (int i = old_rows; i < new_height; i++)
  {
    board->tiles[i] = (Tile *)malloc(board->columns * sizeof(Tile));
    if (board->tiles[i] == NULL)
    {
      fprintf(stderr, "error allocating tile columns array \n");
      exit(1);
    }
    for (int j = 0; j < board->columns; j++)
    {
      Tile *tile = getBoardTile(board, i, j); // &board->tiles[i][j];
      tile->color_num = 0;
      tile->x = j;
      tile->y = i;
    }
  }
}

// variable to store the running state of the program and enable stopping it
volatile int keep_running = 1;
// handleSigint - stops the program gracefully
void handleSigint(int sig)
{
  keep_running = 0;
  printf("stop running\n");
}

// boardToCSV
// takes the board and converts the entire thing to a CSV in order to send the whole state to the client
char *boardToCSV(Board *board)
{
  // 10 bytes per line with rows*boards tiles + dimensions
  char *buffer = (char *)malloc(10 * board->rows * board->columns * sizeof(char) + 10);
  if (buffer == NULL)
  {
    fprintf(stderr, "error malloc boardToCSV buffer\n");
    exit(1);
  }
  buffer[0] = '\0';
  // start with a line containing rows,columns
  char temp[10];
  sprintf(temp, "%d,%d\n", board->rows, board->columns);
  strcat(buffer, temp);

  for (int i = 0; i < board->columns; i++)
  {
    for (int j = 0; j < board->rows; j++)
    {
      char temp[36];
      sprintf(temp, "%d,%d,%d\n", board->tiles[j][i].x, board->tiles[j][i].y, board->tiles[j][i].color_num);
      strcat(buffer, temp);
    }
  }
  return buffer;
}

// parseBoardUpdate
// take received update string, parse it, and apply update to the board tiles state
void parseBoardUpdate(Board *board, char *received_str)
{
  printf("parsing input %s\n", received_str);
  // char * publish_str = strdup(received_str);
  char *token;
  token = strtok(received_str, ",");
  int x = -1;
  int y = -1;
  int color_num = -1;
  int i = 0;
  while (token != NULL)
  {
    if (i == 0)
    {
      x = atoi(token);
    }
    if (i == 1)
    {
      y = atoi(token);
    }
    if (i == 2)
    {
      color_num = atoi(token);
    }
    token = strtok(NULL, ",");
    i++;
  }
  printf("setting %d, %d to %d\n", x, y, color_num);
  Tile *tile = getBoardTile(board, y, x); // &board->tiles[y][x];
  tile->color_num = color_num;

}

// parseBoardResize
// take received resize string, parse it, and apply resize to the board tile state
void parseBoardResize(Board *board, char *received_str)
{
  printf("parsing resize string %s\n", received_str);
  char *token;
  token = strtok(received_str, ",");
  int new_rows = 32;
  int new_cols = 32;
  int i = 0;
  while (token != NULL)
  {
    if (i == 0)
    {
      new_rows = atoi(token);
    }
    if (i == 1)
    {
      new_cols = atoi(token);
    }
    token = strtok(NULL, ",");
    i++;
  }
  if (board->rows != new_rows)
  {
    resizeBoardHeight(board, new_rows);
  }
  if (board->columns != new_cols)
  {
    resizeBoardWidth(board, new_cols);
  }
}

// parseCommand
void parseCommand(Board *board, char *command_str)
{
  /*
    client_id\n
    command\n
    c,s,v
  */
  zsock_send(publisher, "s", command_str);
  char *token;
  char *client_id_str;
  char *command_name;
  char *command_args;
  int i = 0;
  token = strtok(command_str, "\n");
  while (token != NULL)
  {
    if (i == 0)
    {
      client_id_str = token;
    }
    if (i == 1)
    {
      command_name = token;
    }
    if (i == 2)
    {
      command_args = token;
    }
    token = strtok(NULL, "\n");
    i++;
  }
  printf("%s: %s: %s\n", client_id_str, command_name, command_args);

  if (strcmp(command_name, "update") == 0)
  {
    parseBoardUpdate(board, command_args);
  }
  if (strcmp(command_name, "resize") == 0)
  {
    parseBoardResize(board, command_args);
  }
}

// different from server parseBoard - doesnt store exact X/Y/height/width
void parseBoardCSV(Board *board, char *boardCSV)
{
  char *token;
  token = strtok(boardCSV, "\n");
  char **lines = (char **)malloc(board->columns * board->rows * sizeof(char *)); //[columns*rows];
  if (lines == NULL)
  {
    fprintf(stderr, "error malloc in parseBoardCSV\n");
    exit(1);
  }
  int lineIdx = 0;
  while (token != NULL)
  {
    lines[lineIdx] = token;
    token = strtok(NULL, "\n");
    lineIdx++;
  }
  for (int i = 0; i < lineIdx; i++)
  {
    char *line = lines[i];
    // printf("%s\n", line);
    char *dataToken;
    // int lineData[3];
    dataToken = strtok(line, ",");
    int dataIdx = 0;
    int colorNum = -1;
    int x = -1;
    int y = -1;
    while (dataToken != NULL)
    {
      switch (dataIdx)
      {
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

    Tile *tile = &board->tiles[x][y];
    tile->x = x;
    tile->y = y;
    tile->color_num = colorNum;
  }
}


int main(void)
{
  Board board;
  initBoard(&board);

  // Initialization of board
  //
  //--------------------------------------------------------------------------------------
  // getBoardState(&board);
  // char* boardCSV = boardToCSV(board);
  // size_t csv_size = strlen(boardCSV) * sizeof(char);
  // printf("%zu \n", csv_size);
  // exit program on ctrl-c
  if (signal(SIGINT, handleSigint) == SIG_ERR)
  {
    printf("error setting up signal handler \n");
    return 1;
  }

  responder = zsock_new(ZMQ_REP);
  int rc = zsock_bind(responder, "tcp://*:5555");
  assert(rc == 5555);
  printf("tcp req-resp listening on 5555 \n");

  // publisher socket
  publisher = zsock_new_pub("tcp://*:5556");
  if (!publisher)
  {
    printf("Error: Unable to create publisher socket\n");
    return 1;
  }
  printf("tcp pub-sub listening on 5556\n");

  while (keep_running)
  {
    char *received_str = zstr_recv(responder);
    if (received_str)
    {

      printf("received %s \n", received_str);
      if (strcmp(received_str, "fetch") == 0)
      {
        zstr_send(responder, boardToCSV(&board));
      }
      else
      {
        parseCommand(&board, received_str);
        /*
        command\n
        client_id\n
        c,s,v
        */
        zstr_send(responder, "received command");
        //}
        // sleep(1);

      }
      zstr_free(&received_str);
    }
  }
  printf("server stopped gracefully\n");
  zsock_destroy(&responder);

  return 0;
}
