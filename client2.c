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

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

// CONSTANT PROGRAM VARIABLES
// ---------------------------
const int SCREEN_WIDTH = 860;
const int SCREEN_HEIGHT = 620;
const int TILE_SIZE = 16;
const int SELECTION_BTN_SIZE = 32;
enum
{
    INIT_COLUMNS = 32, // width
    INIT_ROWS = 32     // height
}; // enum to enable array initialization
const int BOARD_WIDTH = TILE_SIZE * INIT_ROWS;
const int BOARD_X = SCREEN_WIDTH / 2 - BOARD_WIDTH / 2;
const int BOARD_Y = SCREEN_HEIGHT / 2 - BOARD_WIDTH / 2;

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
    int columns;
    int rows;
    Tile tiles[INIT_ROWS][INIT_COLUMNS];
} TileBoard;

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
    for (int i = 0; i < INIT_ROWS; i++)
    {
        for (int j = 0; j < INIT_COLUMNS; j++)
        {
            Tile *board_tile = getBoardTile(board, i, j); // &board->tiles[i][j];
            board_tile->color_num = BLACK_NUM;
            initTileRectangle(&board_tile->rect, i, j);
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
/*
// store last painted tile
Vector2 last_coord = {
    .x = -1,
    .y = -1
};
// store last painted color
ColorIndex last_color;

// setLastPainted - update last_coord and last_color to what was painted
void setLastPainted(int x, int y, int colorNum){
  last_coord.x = x;
  last_coord.y = y;
  last_color = colorNum;
}

// isLastPainted - return true if we just painted this location with this color, else return false
bool isLastPainted(int x, int y, ColorIndex color_num) {
  if (x == last_coord.x && y == last_coord.y && color_num == last_color){
    return true;
  }
  return false;
}
*/
// UTILITY FUNCTIONS
// ----------------

// validateDimensionInput
// takes the string input of a text box and returns true if its one or two digits and only positive numbers
// false otherwise
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


int main(void)
{
    // initialize our game state
    SetTargetFPS(60);
    TileBoard board;
    initTileBoard(&board);
    // init camera
    Camera2D camera = {0};
    camera.target = (Vector2){BOARD_X + 16 * TILE_SIZE, BOARD_Y + 16 * TILE_SIZE};
    camera.offset = (Vector2){BOARD_X + BOARD_WIDTH / 2.0f, BOARD_Y + BOARD_WIDTH / 2.0f};
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

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
                    if (width_input_num > board.columns)
                    {
                       // reallocBoard(widthInputNum, rows);
                    }
                    board.columns = width_input_num;
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
                    if (height_input_num > board.rows)
                    {
                        //reallocBoard(board.columns, height_input_num);
                    }
                    board.rows = height_input_num;
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
}