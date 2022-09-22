#include "global.h"
#include "fieldmap.h"
#include "malloc.h"
#include "mazegen.h"
#include "mgba_printf/mgba.h"
#include "random.h"

// ***********************************************************************
// mazegen.c
// -----------------------------------------------------------------------
// This file contains the functions required to randomly generate a maze
// and to stitch it together as a map in-game.
// ***********************************************************************

#define EMPTY   0
#define NORTH   (1 << 0)
#define EAST    (1 << 1)
#define SOUTH   (1 << 2)
#define WEST    (1 << 3)

struct MapChunk {
    u16 width;
    u16 height;
    u16 *map;
};

// Initializes the cells in a Maze struct as unvisited and with 
// correct coordinates.
static void InitMaze(struct Maze *maze)
{
    u32 x, y;

    for (x = 0; x < maze->width; ++x)
    {
        for (y = 0; y < maze->height; ++y)
        {
            maze->cells[x][y].x = x;
            maze->cells[x][y].y = y;
            maze->cells[x][y].distance = 0;
            maze->cells[x][y].visited = FALSE;
            maze->cells[x][y].endpoint = FALSE;
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
    s32 i, count = 0;
    u16 options[16];
    
    // Find 'on' bits and store in array to pick from.
    for (i = 0; i < 16; ++i)
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
static void GenerateMaze(struct Maze *maze, u16 width, u16 height)
{
    u32 x, y, candidates, max, top, visited;
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
        origin->distance = top;
        candidates = GetUnvisitedNeighbors(origin->x, origin->y, maze);

        // If there are no unvisited neighbors, pop the stack and recurse
        // to the previous cell. If the stack is empty, break the loop.
        if (!candidates || top >= max)
        {
            if ((origin->connections & (origin->connections - 1)) == 0) // single bit = endpoint
                origin->endpoint = TRUE;
            else
                origin->endpoint = FALSE;

            top--; // pop the stack
            if (top <= 0)
                break;
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

    // the final cell is an endpoint
    origin->distance = top;
    origin->endpoint = TRUE;
}

// Returns an array containing the endpoints of a maze, sorted by distance in
// descending order.
struct Cell **GetMazeEndpoints(struct Maze *maze)
{
    s32 x, y, i, j;
    s32 distance, top = 0;
    struct Cell *temp;
    struct Cell **output = malloc(MAX_SPECIAL_ROOMS * sizeof **output);

    for (i = 0; i < MAX_SPECIAL_ROOMS; i++) // Initialize output as null pointers.
        output[i] = NULL;

    for (x = 0; x < maze->width; ++x) // Pick out endpoints from the maze.
    {
        for (y = 0; y < maze->width; ++y)
        {
            if (maze->cells[x][y].endpoint && top < MAX_SPECIAL_ROOMS)
            {
                output[top] = &maze->cells[x][y];
                top++;
            }
        }
    }

    for (i = 0; i < top; ++i) // Sort the list of endpoints by distance.
    {
        j = i;
        while (j > 0 && output[i]->distance > output[j-1]->distance)
        {
            SWAP(output[j], output[j-1], temp)
            j--;
        }
    }

    return output;
}

// Copies a rectangular area of a map layout and stores it in a MapChunk struct.
static void CopyMapChunk(u16 x, u16 y, u16 width, u16 height, const struct MapLayout *src, struct MapChunk *dest)
{
    s32 i, j;

    dest->width = width;
    dest->height = height;
    dest->map = malloc(sizeof(u16) * width * height);

    for (i = 0; i < height; ++i)
    {
        for (j = 0; j < width; ++j)
        {
            dest->map[j + width * i] = src->map[src->width * (y + i) + x + j];
        }
    }
}

// Pastes a rectangular area of a map layout over gBackupMapLayout at
// the given (x, y) and frees the map stored in the MapChunk.
static void PasteMapChunk(u16 x, u16 y, struct MapChunk *chunk)
{
    u16 *src, *dest;
    int i;
    src = chunk->map;
    dest = gBackupMapLayout.map;
    dest += gBackupMapLayout.width * (7 + y) + x + MAP_OFFSET;
    for (i = 0; i < chunk->height; ++i)
    {
        CpuCopy16(src, dest, chunk->width * 2);
        dest += gBackupMapLayout.width;
        src += chunk->width;
    }
    Free(chunk->map);
}

// The coordinates of map chunks on a map template.
static const u16 sMapChunkCoordinateTable[][2] = {
    [EMPTY] = {0, 0},
    [NORTH] = {0, 1},
    [EAST] = {0, 2},
    [SOUTH] = {0, 3},
    [WEST] = {1, 0},
    [NORTH | EAST] = {1, 1},
    [NORTH | SOUTH] = {1, 2},
    [NORTH | WEST] = {1, 3},
    [EAST | SOUTH] = {2, 0},
    [EAST | WEST] = {2, 1},
    [SOUTH | WEST] = {2, 2},
    [NORTH | EAST | SOUTH] = {2, 3},
    [NORTH | EAST | WEST] = {3, 0},
    [NORTH | SOUTH | WEST] = {3, 1},
    [EAST | SOUTH | WEST] = {3, 2},
    [NORTH | EAST | SOUTH | WEST] = {3, 3},
};

// Generates a maze from a template layout containing map chunks. The width
// and height describe the "chunks" that make up the map.
struct Maze *GenerateMazeMap(u16 width, u16 height, const struct MapLayout *template)
{
    s32 x, y, chunkWidth, chunkHeight, connections;
    static struct Maze maze;
    struct MapChunk chunk;

    GenerateMaze(&maze, width, height);
    chunkWidth = 10;
    chunkHeight = 10;

    for (x = 0; x < width; ++x)
    {
        for (y = 0; y < height; ++y)
        {
            connections = maze.cells[x][y].connections;
            CopyMapChunk(sMapChunkCoordinateTable[connections][0] * chunkWidth,   \
                                    sMapChunkCoordinateTable[connections][1] * chunkHeight, \
                                    chunkWidth, chunkHeight, template, &chunk);
            PasteMapChunk(x * chunkWidth, y * chunkHeight, &chunk);
        }
    }

    return &maze;
}
