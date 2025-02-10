#include "raylib.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <czmq.h>
#include <czmq_prelude.h>
#include <zsock.h>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

zsock_t * subscriber;

const int screenWidth = 860;
const int screenHeight = 620;
const int tileSize = 16;
const int selectionTileSize = 32;
enum { columns = 32, rows = 32 };

const int boardWidth = tileSize * rows;
const int boardX = screenWidth / 2 - boardWidth / 2;
const int boardY = screenHeight / 2 - boardWidth / 2;
typedef struct Tile {
  int colorInt; // from 0 to 4 inclusive
  Rectangle rect;
} Tile;

Tile board[columns][rows];

Vector2 mousePosition;

// parseBoardCSV - input: string
// takes the server response and populates the tile board
// not an actual csv because the first line is missing
// we assume to know what the rows will be
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
    Tile * lineTile = &board[x][y];

    Rectangle *rect = &(*lineTile).rect;
    rect->x = boardX + x * tileSize;
    rect->y = boardY + y * tileSize;
    rect->width = tileSize;
    rect->height = tileSize;
    lineTile->colorInt = colorNum;
  }
}

int sendReq(zsock_t *requester, char * command) {
  printf("sending command %s \n", command);
  zstr_send(requester, command);
  char *str = zstr_recv(requester);
  //printf("received response for %s \n", command);
 // printf("%s \n", str);
  if (strcmp(command, "fetch") == 0) {
    parseBoardCSV(str);
  }
  zstr_free(&str);
  return 0;
}

// prevent repeated updates on the same tile
int lastPainted[3];
void setLastPainted(int x, int y, int colorNum){
  lastPainted[0] = x;
  lastPainted[1] = y;
  lastPainted[2] = colorNum;
}
int isLastPainted(int x, int y, int colorNum) {
  if (x == lastPainted[0] && y == lastPainted[1] && colorNum == lastPainted[2]){
    return 0;
  }
  return 1;
}

void * updateSubThread(void * vargp){
  while(1){
    char * sub_buffer;
    zsock_recv(subscriber, "s", &sub_buffer);
    printf("sub got %s\n", sub_buffer);

    // parse update string

    char *token;
    token = strtok(sub_buffer, ",");
    int x;
    int y;
    int colorNum;
    int i = 0;
    while (token != NULL) {
      if (i == 0) {
        x = atoi(token);
      }
      if (i == 1) {
        y = atoi(token);
      }
      if (i == 2) {
        colorNum = atoi(token);
      }
      token = strtok(NULL, ",");
      i++;
    }
    printf("setting %d, %d to %d\n", x, y, colorNum);
    Tile *currTile = &board[x][y];
    currTile->colorInt = colorNum;
    Rectangle *rect = &(*currTile).rect;
    rect->x = boardX + x * tileSize;
    rect->y = boardY + y * tileSize;
    rect->width = tileSize;
    rect->height = tileSize;

    zstr_free(&sub_buffer);
  }
  
  return NULL;
}
  
int main(void) {
  pthread_t thread_id;
  
  

  subscriber = zsock_new_sub("tcp://localhost:5556", "");
  
  printf("before thread \n");
  pthread_create(&thread_id, NULL, updateSubThread, NULL);
//  pthread_join(thread_id, NULL);
  printf("after thread\n");

  printf("connecting to hello world server...\n");
  zsock_t *requester = zsock_new(ZMQ_REQ);
  zsock_connect(requester, "tcp://localhost:5555");


  // Initialization
  //--------------------------------------------------------------------------------------

  Color tileColors[5] = {BLACK, GREEN, BLUE, GRAY, PURPLE};

  int selectedColorInt = 4;

  // button for fetching whole state
  Rectangle fetchRec;

  fetchRec.x = 20;
  fetchRec.y = 500;
  fetchRec.width = 100;
  fetchRec.height = 80;

  // init selection tile rectangles
  Tile selectionTiles[5];
  for (int i = 0; i < 5; i++) {
    int tileX = 20;
    int tileY = 80 + i * 60;
    Tile *selectionTile = &selectionTiles[i];
    selectionTile->colorInt = i;
    selectionTile->rect.x = tileX;
    selectionTile->rect.y = tileY;
    selectionTile->rect.width = selectionTileSize;
    selectionTile->rect.height = selectionTileSize;
  }

  // Initialization
  //--------------------------------------------------------------------------------------
  for (int i = 0; i < columns; i++) {
    for (int j = 0; j < rows; j++) {
      Tile *tile = &board[j][i];
      Rectangle *rect = &(*tile).rect;
      rect->x = boardX + j * tileSize;
      rect->y = boardY + i * tileSize;
      rect->width = tileSize;
      rect->height = tileSize;

      //int colorNum = rand() % 4;
      tile->colorInt = 0;
    }
  }

  InitWindow(screenWidth, screenHeight, "raylib [core] example - basic window");

#if defined(PLATFORM_WEB)
  emscripten_set_main_loop(UpdateDrawFrame, 60, 1);
#else

  sendReq(requester, "fetch");
  SetTargetFPS(60); // Set our game to run at 60 frames-per-second
  //--------------------------------------------------------------------------------------

  // Main game loop
  while (!WindowShouldClose()) // Detect window close button or ESC key
  {
    // Update
    //----------------------------------------------------------------------------------
    // TODO: Update your variables here
    //----------------------------------------------------------------------------------
    //
    mousePosition = GetMousePosition();
    //  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    // }
    // Draw
    //----------------------------------------------------------------------------------
    BeginDrawing();

    ClearBackground(RAYWHITE);

    // Draw color selections
    for (int i = 0; i < 5; i++) {
      Tile *selectionTile = &selectionTiles[i];
      Color tileColor = tileColors[i];
      DrawRectangleRec(selectionTile->rect, tileColor);

      if (i == selectedColorInt) {
        DrawRectangleLinesEx(selectionTile->rect, 6, YELLOW);
      }
      DrawRectangleLinesEx(selectionTile->rect, 2, BLACK);
      if (CheckCollisionPointRec(mousePosition, selectionTile->rect) &&
          IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        DrawRectangleLinesEx(selectionTile->rect, 4, BLACK);
        selectedColorInt = i;
      }
    }

    // Draw tiles within the board
    for (int i = 0; i < columns; i++) {
      for (int j = 0; j < rows; j++) {
        Tile *tile = &board[j][i];
        int colorInt = tile->colorInt;
        Rectangle *rect = &(*tile).rect;
        Color color = tileColors[colorInt];
        DrawRectangleRec(*rect, color);
        // change clicked tile color
        if (CheckCollisionPointRec(mousePosition, *rect)) {
          DrawRectangleLinesEx(*rect, 2, YELLOW);
          if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            if (isLastPainted(j, i, selectedColorInt) == 1){
              tile->colorInt = selectedColorInt;
              printf("painting %d, %d as %d\n", j, i, selectedColorInt);
              setLastPainted(j, i, selectedColorInt);
              char commandBuffer[10];
              sprintf(commandBuffer, "%d,%d,%d", j, i, selectedColorInt);
              printf("%s\n", commandBuffer);
              sendReq(requester, commandBuffer);
            }
          }
        }
      }
    }

    DrawRectangleLines(boardX, boardY, boardWidth, boardWidth, BLACK);
    DrawText("Raylib collaborative tilemap editor", 10, 10, 20, DARKGRAY);

    // reset button
    DrawRectangleRec(fetchRec, RED);
    DrawText("Reset", fetchRec.x, fetchRec.y, 10, BLACK);

    DrawFPS(10, 600);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      DrawCircle(mousePosition.x, mousePosition.y, 24, YELLOW);
      DrawCircle(mousePosition.x, mousePosition.y, 16, ORANGE);
      DrawCircle(mousePosition.x, mousePosition.y, 8, RED);

      // reset
      if (CheckCollisionPointRec(mousePosition, fetchRec)) {
       // printf("mouse down\n");
        
      }
    }

    EndDrawing();
    //----------------------------------------------------------------------------------
  }

#endif

  // De-Initialization
  //--------------------------------------------------------------------------------------
  CloseWindow(); // Close window and OpenGL context
  //--------------------------------------------------------------------------------------

  zsock_destroy(&requester);
  return 0;
}
