#ifndef GUARD_MAZEGEN_H
#define GUARD_MAZEGEN_H

#define MAX_MAZE_WIDTH      10
#define MAX_MAZE_HEIGHT     10

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

struct Maze *GenerateMazeMap(u16 width, u16 height, const struct MapLayout *template);

#endif // GUARD_MAZEGEN_H
