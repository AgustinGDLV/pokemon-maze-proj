const struct TemplateSet gMazeTemplates[] =
{
    [CAVE_STAIRS_TEMPLATE_SET] = {
        .mapBase = MAP_CAVE_BASE,
        .templates = 
        {
            {
                .mapNumber = MAP_NUM(CAVE_TEMPLATE_1),
                .varietyWeights = {60, 40, 0, 0},
                .adjacencyWeights = {
                    {50, 50, 0}, // NORTH
                    {100, 0, 0}, // EAST
                    {100, 0, 0}, // SOUTH
                    {100, 0, 0}, // WEST
                },
                .totalWeight = 100,
            },
            {
                .mapNumber = MAP_NUM(CAVE_TEMPLATE_3),
                .varietyWeights = {60, 40, 0, 0},
                .adjacencyWeights = {
                    {0, 0, 100}, // NORTH
                    {0, 0, 100}, // EAST
                    {100, 0, 0}, // SOUTH
                    {0, 0, 100}, // WEST
                },
                .totalWeight = 100,
            },
            {
                .mapNumber = MAP_NUM(CAVE_TEMPLATE_4),
                .varietyWeights = {60, 40, 0, 0},
                .adjacencyWeights = {
                    {0, 0, 100}, // NORTH
                    {0, 0, 100}, // EAST
                    {0, 50, 50}, // SOUTH
                    {0, 0, 100}, // WEST
                },
                .totalWeight = 100,
            },
        },
        .templateCount = 3,
        .chunkWidth = 10,
        .chunkHeight = 10,
    }
};
