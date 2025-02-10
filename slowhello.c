#include "raylib.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <czmq.h>
#include <czmq_prelude.h>
#include <zsock.h>
#include <stdbool.h>

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

pthread_t sub_thread_id;
pthread_t req_thread_id;
zsock_t *subscriber;
zsock_t *requester;

const int screenWidth = 860;
const int screenHeight = 620;
const int tileSize = 16;
const int selectionTileSize = 32;
enum { init_columns = 32, init_rows = 32 };
int rows = init_rows;
int columns = init_columns;
const int boardWidth = tileSize * init_rows;
const int boardX = screenWidth / 2 - boardWidth / 2;
const int boardY = screenHeight / 2 - boardWidth / 2;
typedef struct Tile {
  int colorInt; // from 0 to 4 inclusive. could this be an enum?
  Rectangle rect;
} Tile;

Tile ** board;

Vector2 mousePosition;
Vector2 mouseWorldPosition;
bool textBoxWidthEdit = false;
bool textBoxHeightEdit = false;

char widthInputText[16] = {'3', '2', 0};
char heightInputText[16] = {'3', '2', 0};

Rectangle leftBoundary;
Rectangle rightBoundary;
Rectangle topBoundary;
Rectangle bottomBoundary;

bool checkInBoundary(){
  if (CheckCollisionPointRec(mousePosition, leftBoundary)){
    return false;
  }
  if (CheckCollisionPointRec(mousePosition, rightBoundary)){
    return false;
  }
  if (CheckCollisionPointRec(mousePosition, bottomBoundary)){
    return false;
  }
  if (CheckCollisionPointRec(mousePosition, topBoundary)){
    return false;
  }
  return true;
}

bool validateDimensionInput(char* inputText){
  int length = strlen(inputText);
  if (length > 2){
    // must be no more than 2 digits
    return false;
  }
  for (int i = 0; i < length; i++){
    if (!isdigit(inputText[i])){
      // only digits allowed
      return false;
    }
  }
  return true;
}


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

void* sendReqThread(void * arg) {
  char* command = (char*)arg;
  printf("sending command %s \n", command);
  //sleep(1);
  zstr_send(requester, command);
  //sleep(1);
  char *str = zstr_recv(requester);
  //printf("received response for %s \n", command);
 // printf("%s \n", str);
  if (strcmp(command, "fetch") == 0) {
    parseBoardCSV(str);
  }
  zstr_free(&str);
  return NULL;
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

void * updateSubThread(void * arg){
  while(1){
    char * sub_buffer;
    // simulate latency
    sleep(1);
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

  Camera2D camera = {0};
  camera.target = (Vector2){boardX+16*tileSize, boardY+16*tileSize};
  camera.offset = (Vector2){boardX+boardWidth/2.0f, boardY+boardWidth/2.0f};
  camera.rotation = 0.0f;
  camera.zoom = 1.0f;

  subscriber = zsock_new_sub("tcp://localhost:5556", "");

  requester = zsock_new(ZMQ_REQ);
  
  printf("before thread \n");
  pthread_create(&sub_thread_id, NULL, updateSubThread, NULL);
//  pthread_join(thread_id, NULL);
  printf("after thread\n");

  printf("connecting to hello world server...\n");
  zsock_connect(requester, "tcp://localhost:5555");
  // Initialization
  //--------------------------------------------------------------------------------------

  board = (Tile**)malloc(init_columns * sizeof(Tile*));
  for (int i = 0; i < init_rows; i++){
    board[i] = (Tile *)malloc(init_rows * sizeof(Tile));
  }

  Color tileColors[5] = {BLACK, GREEN, BLUE, GRAY, PURPLE};

  int selectedColorInt = 4;

  // button for fetching whole state
  Rectangle fetchRec;

  leftBoundary.x = 0;
  leftBoundary.y = 0;
  leftBoundary.width = boardX;
  leftBoundary.height = screenHeight;
  topBoundary.x = 0;
  topBoundary.y = 0;
  topBoundary.width = screenWidth;
  topBoundary.height = boardY;
  bottomBoundary.x = boardX;
  bottomBoundary.y = boardY+32*16;
  bottomBoundary.width = 32*16;
  bottomBoundary.height = screenHeight - boardY + 32 * 16; 
  rightBoundary.x = boardX + 32 * 16;
  rightBoundary.y = 0;
  rightBoundary.width = screenWidth; // lazy width calc
  rightBoundary.height = screenHeight;

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

  InitWindow(screenWidth, screenHeight, "raylib collab tile editor - slow latency");

#if defined(PLATFORM_WEB)
  emscripten_set_main_loop(UpdateDrawFrame, 60, 1);
#else

  const char* command = "fetch";
  pthread_create(&req_thread_id, NULL, sendReqThread, (void*)command);
  //sendReqThread("fetch");
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
    mouseWorldPosition = GetScreenToWorld2D(mousePosition, camera);
    camera.zoom += ((float)GetMouseWheelMove() * 0.05f);
    if (IsKeyDown(KEY_RIGHT))
      camera.target.x += 2;
    else if (IsKeyDown(KEY_LEFT))
      camera.target.x -= 2;
    if (IsKeyDown(KEY_UP))
      camera.target.y -= 2;
    else if (IsKeyDown(KEY_DOWN))
      camera.target.y += 2;
    //  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    // }
    // Draw
    //----------------------------------------------------------------------------------
    BeginDrawing();

    ClearBackground(RAYWHITE);

    BeginMode2D(camera);

    // Draw tiles within the board
    for (int i = 0; i < columns; i++) {
      for (int j = 0; j < rows; j++) {
        Tile *tile = &board[j][i];
        int colorInt = tile->colorInt;
        Rectangle *rect = &(*tile).rect;
        Color color = tileColors[colorInt];
        DrawRectangleRec(*rect, color);
        // change clicked tile color
        if (CheckCollisionPointRec(mouseWorldPosition, *rect)) {
          DrawRectangleLinesEx(*rect, 2, YELLOW);
          if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && checkInBoundary() == true) {
            if (isLastPainted(j, i, selectedColorInt) == 1){
              tile->colorInt = selectedColorInt;
              printf("painting %d, %d as %d\n", j, i, selectedColorInt);
              setLastPainted(j, i, selectedColorInt);
              char commandBuffer[10];
              sprintf(commandBuffer, "%d,%d,%d", j, i, selectedColorInt);
              printf("%s\n", commandBuffer);

              pthread_t update_thread_id;
              pthread_create(&update_thread_id, NULL, sendReqThread,
                             &commandBuffer);
              // sendReqThread(commandBuffer);
            }
          }
        }
      }
    }

    EndMode2D();

    // draw the edges of the edit area

    DrawRectangleRec(topBoundary, WHITE);
    DrawRectangleRec(leftBoundary, WHITE);
    DrawRectangleRec(bottomBoundary, WHITE);
    DrawRectangleRec(rightBoundary, WHITE);

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


    DrawRectangleLines(boardX, boardY, boardWidth, boardWidth, BLACK);
    DrawText("Raylib collaborative tilemap editor", 10, 10, 20, DARKGRAY);

    // reset button
    // not implemented on server yet
    //GuiButton(fetchRec, "Reset");

    // map size change
    DrawText("set width: (2 digits)", 20, 385, 15, DARKGRAY);
    if (GuiTextBox((Rectangle){20, 400, 80, 30}, widthInputText, 16, textBoxWidthEdit)){
      textBoxWidthEdit = !textBoxWidthEdit;
      if (textBoxWidthEdit == false) {
        bool isValid = validateDimensionInput(widthInputText);
        if (isValid) {
          printf("trigger update width %d\n", isValid);
          int widthInputNum = atoi(widthInputText);
        } else {
          snprintf(widthInputText, sizeof(widthInputText), "%d", columns);
        }
      }
    }
    DrawText("set height: (2 digits)", 20, 445, 15, DARKGRAY);
    if (GuiTextBox((Rectangle){20, 460, 80, 30}, heightInputText, 16,
                   textBoxHeightEdit)) {
      textBoxHeightEdit = !textBoxHeightEdit;
      if (textBoxHeightEdit == false){
        bool isValid = validateDimensionInput(heightInputText);
        if (isValid) {
          printf("trigger update height %d\n", isValid);
          int heightInputNum = atoi(heightInputText);
        } else {
          snprintf(heightInputText, sizeof(heightInputText), "%d", rows);
        }
        //    handleHeightChange
      }
    }

    DrawFPS(10, 600);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      DrawCircle(mousePosition.x, mousePosition.y, 24, YELLOW);
      DrawCircle(mousePosition.x, mousePosition.y, 16, ORANGE);
      DrawCircle(mousePosition.x, mousePosition.y, 8, RED);

      // reset -- not implemented
     // if (CheckCollisionPointRec(mouseWorldPosition, fetchRec)) {
       // printf("mouse down\n");
        
     //}
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
