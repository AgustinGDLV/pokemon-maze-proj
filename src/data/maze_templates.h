const struct TemplateSet gMazeTemplates[] =
{
    [CAVE_TEMPLATE_SET] = {
        .mapBase = MAP_CAVE_BASE,
        .templates = 
        {
            {
                .mapNumber = MAP_NUM(CAVE_TEMPLATE_2),
                .varietyWeights = {50, 15, 15, 15},
                .totalWeight = 95,
            },
        },
        .adjacencyWeights =
        {
            {100, 0, 0, 0},
        },
        .templateCount = 1,
        .chunkWidth = 10,
        .chunkHeight = 10,
    }
};
