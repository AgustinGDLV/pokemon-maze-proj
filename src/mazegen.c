#include "global.h"
#include "malloc.h"
#include "random.h"

// ***********************************************************************
// mazegen.c
// -----------------------------------------------------------------------
// This file contains the functions required to randomly generate a maze
// as a series of connections between cells.
// ***********************************************************************

#define MAX_MAZE_WIDTH      10
#define MAX_MAZE_HEIGHT     10

#define EMPTY   0
#define NORTH   (1 << 0)
#define EAST    (1 << 1)
#define SOUTH   (1 << 2)
#define WEST    (1 << 3)

struct Cell
{
    u16 x:4;
    u16 y:4;
    u16 visited:1;      // boolean
    u16 connections:4;  // stored as bitfield
};

struct Maze
{
    u16 width;
    u16 height;
    struct Cell cells[MAX_MAZE_WIDTH][MAX_MAZE_HEIGHT];
};

static void InitMaze(struct Maze *maze);
static u16 GetUnvisitedNeighbors(u16 x, u16 y, struct Maze *maze);
static u16 SelectRandomBit(u16 bitfield);
static void GenerateMaze(struct Maze *maze, u32 width, u32 height);

// Initializes the cells in a Maze struct as unvisited and with 
// correct coordinates.
static void InitMaze(struct Maze *maze)
{
    u32 x, y;

    for (x = 0; x < maze->width; x++)
    {
        for (y = 0; y < maze->height; y++)
        {
            maze->cells[x][y].x = x;
            maze->cells[x][y].y = y;
            maze->cells[x][y].visited = FALSE;
            maze->cells[x][y].connections = EMPTY;
        }
    }
}

// Returns the direction(s) of the unvisited neighbors of a given cell
// in a Maze struct as a bitfield.
static u16 GetUnvisitedNeighbors(u16 x, u16 y, struct Maze *maze)
{
    u16 output = 0;
    
    if (y > 0 && !maze->cells[x][y-1].visited)
        output |= NORTH;
    if (x + 1 < maze->width && !maze->cells[x+1][y].visited)
        output |= EAST;
    if (y + 1 < maze->height && !maze->cells[x][y+1].visited)
        output |= SOUTH;
    if (x > 0 && !maze->cells[x-1][y].visited)
        output |= WEST;
    
    return output;
}

// Returns a random 'on' bit in a given bitfield.
static u16 SelectRandomBit(u16 bitfield)
{
    u16 i, count = 0;
    static u16 options[16];
    
    // Find 'on' bits and store in array to pick from.
    for (i = 0; i < 16; i++)
    {
        if (bitfield & (1 << i))
        {
            options[count] = i + 1;
            count++;
        }
    }
    
    return 1 << (options[Random() % count] - 1);
}

// Generates a maze of given width and height into an empty Maze struct.
static void GenerateMaze(struct Maze *maze, u32 width, u32 height)
{
    u32 x, y, candidates, max, top, visited;
    struct Maze *maze = malloc(sizeof *maze);
    struct Cell *origin, *stack[width * height];

    // init stack
    max = width * height;
    top = 0;
    visited = 1;
    
    // init maze
    maze->width = width;
    maze->height = height;
    InitMaze(maze);
    origin = &maze->cells[0][0];

    // Maze generation ends when every cell has been visited.
    while (visited < (maze->width * maze->height))
    {
        stack[top] = origin;
        origin->visited = TRUE;
        candidates = GetUnvisitedNeighbors(origin->x, origin->y, maze);

        // If there are no unvisited neighbors, pop the stack and recurse
        // to the previous cell. If the stack is empty, break the loop.
        if (!candidates || top >= max)
        {
            top--;
            if (top <= 0)
                break;
            origin = stack[top];
        }
        // If the top of the stack exceeds the stack limit, pop the stack
        // and recurse. This increments the visited cells by more than 1
        // to purposefully create unfilled mazes.
        else if (top > max)
        {
            visited += 3;
            top--;
            origin = stack[top];
        }
        // Otherwise, select a random neighbor to become the new origin
        // point and document this connection.
        else
        {
            switch (SelectRandomBit(candidates))
            {
                case NORTH:
                    origin->connections |= NORTH;
                    origin = &maze->cells[origin->x][origin->y-1];
                    origin->connections |= SOUTH;
                    break;
                case EAST:
                    origin->connections |= EAST;
                    origin = &maze->cells[origin->x+1][origin->y];
                    origin->connections |= WEST;
                    break;
                case SOUTH:
                    origin->connections |= SOUTH;
                    origin = &maze->cells[origin->x][origin->y+1];
                    origin->connections |= NORTH;
                    break;
                case WEST:
                    origin->connections |= WEST;
                    origin = &maze->cells[origin->x-1][origin->y];
                    origin->connections |= EAST;
                    break;
            }
            visited++;
            top++;
        }
    }
}

// Copies a map chunk onto a map layout at a given (x, y).
static void CopyMapLayout(u16 x, u16 y, struct MapChunk *src, struct MapLayout *dest)
{
    u32 i, j;
    for (i = 0; i < src->height; i++)
    {
        for (j = 0; j < src->width; j++)
        {
            dest->map[x+j + (y + i)*src->width] = src->map[j + i*src->width];
        }
    }
}
