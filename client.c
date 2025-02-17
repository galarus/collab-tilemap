// a rewrite of the collaborative tile editor with improved code quality
// variables use snake_case.  Types use UpperCamelCase and functions use lowerCamelCase()
// I could simplify things by initializing the TileBoard with a maximum row/col number,
// but I want to include dynamic memory allocation for practice.

#include "raylib.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <czmq.h>
#include <czmq_prelude.h>
#include <zsock.h>
#include <stdbool.h>
#include <uuid/uuid.h>

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

// CONSTANT PROGRAM VARIABLES
// ---------------------------
#define INIT_COLUMNS 32
#define INIT_ROWS 32
#define SCREEN_WIDTH 860
#define SCREEN_HEIGHT 620
#define TILE_SIZE 16
#define SELECTION_BTN_SIZE 32

const int BOARD_WIDTH = TILE_SIZE * INIT_ROWS;
const int BOARD_X = SCREEN_WIDTH / 2 - BOARD_WIDTH / 2;
const int BOARD_Y = SCREEN_HEIGHT / 2 - BOARD_WIDTH / 2;

// NETWORKING
// ----------
uuid_t binuuid;
char uuid[37];
pthread_t sub_thread_id;
pthread_t req_thread_id;
zsock_t *subscriber;
zsock_t *requester;

// CUSTOM TYPEDEFS
// ----------------
typedef enum
{
    BLACK_NUM = 0,
    GREEN_NUM = 1,
    BLUE_NUM = 2,
    GRAY_NUM = 3,
    PURPLE_NUM = 4,
} ColorIndex;

Color tileColors[5] = {BLACK, GREEN, BLUE, GRAY, PURPLE};

// getColor function for bounds checking
Color getColor(ColorIndex idx)
{
    return tileColors[idx];
}

typedef struct
{
    ColorIndex color_num;
    Rectangle rect;
} Tile;

typedef struct
{
    // board width
    int columns;
    // board height
    int rows;
    // 2d array of tiles
    Tile **tiles;
} TileBoard;

// freeTiles
void freeTiles(TileBoard *board)
{
    for (int i = 0; i < board->rows; i++)
    {
        free(board->tiles[i]);
    }
    free(board->tiles);
}

// initTileRectangle
// call to init a raylib rectangle for each tile to help with rendering and collision detection
// prevents creating this struct ad hoc every frame by keeping it in the TileBoard struct
void initTileRectangle(Rectangle *rec, int row_idx, int col_idx)
{
    rec->height = TILE_SIZE;
    rec->width = TILE_SIZE;
    rec->x = BOARD_X + TILE_SIZE * col_idx;
    rec->y = BOARD_Y + TILE_SIZE * row_idx;
};

// getBoardTile
// a way to access tiles from the TileBoard that checks for out of bounds issues
Tile *getBoardTile(TileBoard *board, int row_idx, int col_idx)
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

// isTileColor
// return true if the given tile coordinate is of the given color
bool isTileColor(TileBoard *board, int row_idx, int col_idx, ColorIndex color_num)
{
    Tile *tile = getBoardTile(board, row_idx, col_idx);
    if (tile->color_num == color_num)
    {
        return true;
    }
    else
    {
        return false;
    }
}

// initTileBoard
// call to init with the default number of rows and columns before doing anything else
void initTileBoard(TileBoard *board)
{
    board->columns = INIT_COLUMNS;
    board->rows = INIT_ROWS;
    // allocate memory for 2d array of tiles
    board->tiles = (Tile **)malloc(INIT_ROWS * sizeof(Tile *));
    if (board->tiles == NULL)
    {
        fprintf(stderr, "error allocating tile rows array\n");
        exit(1);
    }
    // for (int i = 0; i < INIT)
    //  allocate memory and initialize values
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
            Tile *board_tile = getBoardTile(board, i, j); // &board->tiles[i][j];
            board_tile->color_num = BLACK_NUM;
            initTileRectangle(&board_tile->rect, i, j);
        }
    }
}

// resizeBoardWidth
// take a new width and reallocate memory for it
// initialize new values if new width is bigger than old width
// updating board[][COLUMNS]
void resizeBoardWidth(TileBoard *board, int new_width)
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
            tile->color_num = BLACK_NUM;
            initTileRectangle(&tile->rect, i, j);
        }
    }
}

// resizeBoardHeight
// take a new height and reallocate memory for it
// initialize new values if new height is bigger than old height
// updating board[ROWS][]
void resizeBoardHeight(TileBoard *board, int new_height)
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

        Tile *temp_malloc = (Tile *)malloc(board->columns * sizeof(Tile));

        if (temp_malloc == NULL)
        {
            fprintf(stderr, "error allocating tile columns array \n");
            exit(1);
        }
        board->tiles[i] = temp_malloc;

        for (int j = 0; j < board->columns; j++)
        {
            Tile *tile = getBoardTile(board, i, j); // &board->tiles[i][j];
            tile->color_num = BLACK_NUM;
            initTileRectangle(&tile->rect, i, j);
        }
    }
}

// MUTABLE PROGRAM VARIABLES
// --------------------------
ColorIndex selected_color_index = PURPLE_NUM; // selected color = 4
Vector2 mouse_pos;
Vector2 mouse_world_pos;
Camera2D camera;
bool text_box_width_edit = false;
bool text_box_height_edit = false;
char width_input_text[16] = {'3', '2', 0};
char height_input_text[16] = {'3', '2', 0};

// BOUNDARY RECTANGLES
// -------------------
Rectangle left_boundary = {
    .x = 0,
    .y = 0,
    .width = BOARD_X,
    .height = SCREEN_HEIGHT};
Rectangle top_boundary = {
    .x = 0,
    .y = 0,
    .width = SCREEN_WIDTH,
    .height = BOARD_Y};
Rectangle bottom_boundary = {
    .x = BOARD_X,
    .y = BOARD_Y + 32 * 16,
    .width = 32 * 16,
    .height = SCREEN_HEIGHT - BOARD_Y + 32 * 16};
Rectangle right_boundary = {
    .x = BOARD_X + 32 * 16,
    .y = 0,
    .width = SCREEN_WIDTH, // lazy width calc
    .height = SCREEN_HEIGHT};

// checkInBoundary
// return false if cursor is outside of board area, true if its in board area
bool checkInBoundary()
{
    if (CheckCollisionPointRec(mouse_pos, left_boundary))
    {
        return false;
    }
    if (CheckCollisionPointRec(mouse_pos, right_boundary))
    {
        return false;
    }
    if (CheckCollisionPointRec(mouse_pos, bottom_boundary))
    {
        return false;
    }
    if (CheckCollisionPointRec(mouse_pos, top_boundary))
    {
        return false;
    }
    return true;
}

// UTILITY FUNCTIONS
// ----------------

// validateDimensionInput
// takes the string input of a text box and returns true if its one or two digits and only positive numbers
// false otherwise
bool validateDimensionInput(char *inputText)
{
    int length = strlen(inputText);
    if (length > 2)
    {
        // must be no more than 2 digits
        return false;
    }
    for (int i = 0; i < length; i++)
    {
        if (!isdigit(inputText[i]))
        {
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
void parseBoardCSV(TileBoard *board, char *boardCSV)
{

    /*
    rows,columns\n
    board,c,s,v
    */
    // extract the first line with the dimensions
    char *token;
    char *rest_board = boardCSV;
    // char *dimensions_str;
    int server_columns = -1;
    int server_rows = -1;
    token = strtok_r(rest_board, "\n", &rest_board);
    char *dimensions_str = token;
    printf("dimension str %s\n", dimensions_str);
    char *rest_dimensions = dimensions_str;
    token = strtok_r(rest_dimensions, ",", &rest_dimensions);
    server_rows = atoi(token);
    printf("token %s\n", token);
    token = strtok_r(rest_dimensions, ",", &rest_dimensions);
    server_columns = atoi(token);
    printf("token %s\n", token);
    //  if (dimensions_token == NULL)
    //{
    //    fprintf(stderr, "failed to parse board dimensions from fetch\n");
    //    exit(1);
    //}
    // server_rows = atoi(dimensions_token);

    // check if different and resize actually!
    if (server_rows != board->rows)
    {
        resizeBoardHeight(board, server_rows);
        snprintf(height_input_text, sizeof(height_input_text), "%d", server_rows);
    }
    if (server_columns != board->columns)
    {
        resizeBoardWidth(board, server_columns);
        snprintf(width_input_text, sizeof(height_input_text), "%d", server_columns);
    }
    // -----

    // line_token = strtok(boardCSV, "\n");

    // printf("line token: %s\n", line_token);
    // line_token = strtok(NULL, "\n");
    //  printf("%s\n", boardCSV);
    // line_token = strtok(NULL, "\n");
    token = strtok_r(rest_board, "\n", &rest_board);
    char *lines[server_columns * server_rows];
    int lineIdx = 0;
    while (token != NULL)
    {
        lines[lineIdx] = token;
        token = strtok_r(rest_board, "\n", &rest_board);
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
        int colorNum = 0;
        int x = 0;
        int y = 0;
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
        Tile *lineTile = getBoardTile(board, y, x); //&board->tiles[x][y];

        Rectangle *rect = &(*lineTile).rect;
        rect->x = BOARD_X + x * TILE_SIZE;
        rect->y = BOARD_Y + y * TILE_SIZE;
        rect->width = TILE_SIZE;
        rect->height = TILE_SIZE;
        lineTile->color_num = colorNum;
    }

    // free(dimensions_str);
}

char *sendReq(char *req_str)
{
    printf("sending command %s \n", req_str);
    zstr_send(requester, req_str);
    // sleep(1);
    char *str = zstr_recv(requester);
    return str;
}
void sendFetchReq(TileBoard *board)
{
    char *result = sendReq("fetch");
    parseBoardCSV(board, result);
    printf("%s\n",result);
    zstr_free(&result);
}
void sendResizeReq(int new_rows, int new_cols)
{
    char command_str[64];
    snprintf(command_str, sizeof(command_str), "%s\nresize\n%d,%d",
             uuid, new_rows, new_cols);
    char *result = sendReq(command_str);
    printf("%s\n", result);
    zstr_free(&result);
}
void sendUpdateReq(int x, int y, ColorIndex color_num)
{
    char command_str[64];
    snprintf(command_str, sizeof(command_str), "%s\nupdate\n%d,%d,%d",
             uuid, x, y, color_num);
    char *result = sendReq(command_str);
    printf("%s\n", result);
    zstr_free(&result);
}

// parseBoardUpdate
// use x,y,color string received from the subscriber to update the board to match with 
// the other users
void parseBoardUpdate(TileBoard* board, char* arg_str){
// parse update string

    char *token;
    token = strtok(arg_str, ",");
    int x = 0;
    int y = 0;
    int color_num = 0;
    int i = 0;
    while (token != NULL) {
      if (i == 0) {
        x = atoi(token);
      }
      if (i == 1) {
        y = atoi(token);
      }
      if (i == 2) {
        color_num = atoi(token);
      }
      token = strtok(NULL, ",");
      i++;
    }
    printf("setting %d, %d to %d\n", x, y, color_num);
    Tile *tile = getBoardTile(board, y, x); // &board->tiles[x][y];
    tile->color_num = color_num;
}

// parseBoardResize
// takes the passed in string argument new_rows,new_columns in order to process a resize
// to match with the other users
void parseBoardResize(TileBoard* board, char* arg_str){
    char* token;
    int new_rows = 32;
    int new_cols = 32;
    token = strtok(arg_str, ",");
    new_rows = atoi(token);
    token = strtok(NULL, ",");
    new_cols = atoi(token);

    if (new_rows != board->rows){
        resizeBoardHeight(board, new_rows);
        snprintf(height_input_text, sizeof(width_input_text), "%d", new_rows);
    }
    if (new_cols != board->columns){
        resizeBoardWidth(board, new_cols);
        snprintf(width_input_text, sizeof(width_input_text), "%d", new_cols);
    }

}

// updateSubThread
// this is passed to pthread_create along with the board address in order to set up 
// subscriptions
void * updateSubThread(void * arg){
  while(1){
    TileBoard* board = (TileBoard*)arg;
    char * sub_buffer;
    // simulate latency
    //sleep(1);
    zsock_recv(subscriber, "s", &sub_buffer);
    printf("sub got %s\n", sub_buffer);
    char* token;
    char* client_id_str;
    char* command_name;
    char* command_args;
    int i = 0;
    token = strtok(sub_buffer, "\n");
    client_id_str = token;
    if (strcmp(client_id_str, uuid) == 0){
        printf("same ID. SKIP\n");
        zstr_free(&sub_buffer);
        continue;
    }
    token = strtok(NULL, "\n");
    command_name = token;
    token = strtok(NULL, "\n");
    command_args = token;
    if (strcmp(command_name, "update") == 0){
        parseBoardUpdate(board, command_args);
    }
    if (strcmp(command_name, "resize") == 0){
        parseBoardResize(board, command_args);

    }
    zstr_free(&sub_buffer);
}
  
  return NULL;
}

int main(void)
{
    // create a client ID

    uuid_generate_random(binuuid);
    uuid_unparse(binuuid, uuid);

    // connect zeromq

    requester = zsock_new(ZMQ_REQ);
    zsock_connect(requester, "tcp://localhost:5555");
    subscriber = zsock_new_sub("tcp://localhost:5556", "");

    
    // initialize our game state
    TileBoard board;
    initTileBoard(&board);
    sendFetchReq(&board); 
    SetTargetFPS(60);

    pthread_create(&sub_thread_id, NULL, updateSubThread, &board);
    // init camera
    Camera2D camera = {0};
    camera.target = (Vector2){BOARD_X + 16 * TILE_SIZE, BOARD_Y + 16 * TILE_SIZE};
    camera.offset = (Vector2){BOARD_X + BOARD_WIDTH / 2.0f, BOARD_Y + BOARD_WIDTH / 2.0f};
    camera.rotation = 0.0f;
    camera.zoom = 0.8f;

    // init selection tile rectangles
    Tile selectionTiles[5];
    for (int i = 0; i < 5; i++)
    {
        int tileX = 20;
        int tileY = 80 + i * 60;
        Tile *selectionTile = &selectionTiles[i];
        selectionTile->color_num = i;
        selectionTile->rect.x = tileX;
        selectionTile->rect.y = tileY;
        selectionTile->rect.width = SELECTION_BTN_SIZE;
        selectionTile->rect.height = SELECTION_BTN_SIZE;
    }

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "raylib collab tile editor - rewrite");
    while (!WindowShouldClose())
    {
        // UPDATE
        // -------
        // mouse position and camera update
        mouse_pos = GetMousePosition();
        mouse_world_pos = GetScreenToWorld2D(mouse_pos, camera);
        camera.zoom += ((float)GetMouseWheelMove() * 0.05f);
        if (IsKeyDown(KEY_RIGHT))
            camera.target.x += 2;
        else if (IsKeyDown(KEY_LEFT))
            camera.target.x -= 2;
        if (IsKeyDown(KEY_UP))
            camera.target.y -= 2;
        else if (IsKeyDown(KEY_DOWN))
            camera.target.y += 2;

        // DRAW
        // ----
        BeginDrawing();
        ClearBackground(RAYWHITE);

        // Draw TileBoard with camera
        BeginMode2D(camera);
        for (int i = 0; i < board.rows; i++)
        {
            for (int j = 0; j < board.columns; j++)
            {
                Tile *tile = getBoardTile(&board, i, j); //&board.tiles[i][j];
                Color color = getColor(tile->color_num);
                DrawRectangleRec(tile->rect, color);
                // check collision while we are here
                if (CheckCollisionPointRec(mouse_world_pos, tile->rect))
                {
                    // draw highlight around targeted tile
                    DrawRectangleLinesEx(tile->rect, 2, YELLOW);
                    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && checkInBoundary() == true)
                    {
                        if (isTileColor(&board, i, j, selected_color_index) == false)
                        {
                            tile->color_num = selected_color_index;
                            printf("painting %d, %d as %d\n", j, i, selected_color_index);
                            sendUpdateReq(j, i, selected_color_index);
                            // setLastPainted(j, i, selected_color_index);
                            // char commandBuffer[32];
                            // snprintf(commandBuffer, sizeof(commandBuffer), "%d,%d,%d", j, i, selectedColorInt);
                            // printf("%s\n", commandBuffer);

                            // pthread_t update_thread_id;
                            // pthread_create(&update_thread_id, NULL, sendReqThread,
                            //                &commandBuffer);
                            //  sendReqThread(commandBuffer);
                        }
                    }
                }
            }
        }
        EndMode2D();

        // Draw UI outside of TileBoard
        // draw the edges of the edit area

        DrawRectangleRec(top_boundary, WHITE);
        DrawRectangleRec(left_boundary, WHITE);
        DrawRectangleRec(bottom_boundary, WHITE);
        DrawRectangleRec(right_boundary, WHITE);

        // Draw color selections
        for (int i = 0; i < 5; i++)
        {
            Tile *selectionTile = &selectionTiles[i];
            Color tileColor = tileColors[i];
            DrawRectangleRec(selectionTile->rect, tileColor);

            if (i == selected_color_index)
            {
                DrawRectangleLinesEx(selectionTile->rect, 6, YELLOW);
            }
            DrawRectangleLinesEx(selectionTile->rect, 2, BLACK);
            if (CheckCollisionPointRec(mouse_pos, selectionTile->rect) &&
                IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                DrawRectangleLinesEx(selectionTile->rect, 4, BLACK);
                selected_color_index = i;
            }
        }

        // map size change
        DrawText("set width: (2 digits)", 20, 385, 15, DARKGRAY);
        if (GuiTextBox((Rectangle){20, 400, 80, 30}, width_input_text, 16, text_box_width_edit))
        {
            text_box_width_edit = !text_box_width_edit;
            if (text_box_width_edit == false)
            {
                bool isValid = validateDimensionInput(width_input_text);
                if (isValid)
                {
                    printf("trigger update width %d\n", isValid);
                    int width_input_num = atoi(width_input_text);
                    resizeBoardWidth(&board, width_input_num);
                    sendResizeReq(board.rows, board.columns);
                }
                else
                {
                    snprintf(width_input_text, sizeof(width_input_text), "%d", board.columns);
                }
            }
        }
        DrawText("set height: (2 digits)", 20, 445, 15, DARKGRAY);
        if (GuiTextBox((Rectangle){20, 460, 80, 30}, height_input_text, 16,
                       text_box_height_edit))
        {
            text_box_height_edit = !text_box_height_edit;
            if (text_box_height_edit == false)
            {
                bool isValid = validateDimensionInput(height_input_text);
                if (isValid)
                {
                    printf("trigger update height %d\n", isValid);
                    int height_input_num = atoi(height_input_text);
                    resizeBoardHeight(&board, height_input_num);
                    sendResizeReq(board.rows, board.columns);
                }
                else
                {
                    snprintf(height_input_text, sizeof(height_input_text), "%d", board.rows);
                }
                //    handleHeightChange
            }
        }

        DrawFPS(10, 600);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            DrawCircle(mouse_pos.x, mouse_pos.y, 24, YELLOW);
            DrawCircle(mouse_pos.x, mouse_pos.y, 16, ORANGE);
            DrawCircle(mouse_pos.x, mouse_pos.y, 8, RED);
        }
        EndDrawing();
    }
    printf("goodbye\n");
    freeTiles(&board);
    zsock_destroy(&requester);
    CloseWindow();
    return 0;
}