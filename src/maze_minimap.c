#include "global.h"
#include "bg.h"
#include "sprite.h"
#include "main.h"
#include "battle.h"
#include "decompress.h"
#include "event_data.h"
#include "event_object_movement.h"
#include "field_screen_effect.h"
#include "field_weather.h"
#include "gpu_regs.h"
#include "international_string_util.h"
#include "item_menu.h"
#include "sound.h"
#include "mazegen.h"
#include "malloc.h"
#include "menu.h"
#include "mgba_printf/mgba.h"
#include "overworld.h"
#include "random.h"
#include "palette.h"
#include "save.h"
#include "scanline_effect.h"
#include "script.h"
#include "sprite.h"
#include "string_util.h"
#include "trig.h"
#include "util.h"
#include "window.h"
#include "constants/rgb.h"
#include "constants/songs.h"

// credit to Sbird and Skeli

#define EMPTY   0
#define NORTH   (1 << 0)
#define EAST    (1 << 1)
#define SOUTH   (1 << 2)
#define WEST    (1 << 3)

#define TAG_MAZE_STAR 0x2710

#define POS_TO_SCR_ADDR(x,y) (32*(y) + (x))
#define SCR_MAP_ENTRY(tile, pal, hflip, vflip) (tile | (hflip ? (1<<10) : 0) | (vflip ? (1 << 11) : 0) | (pal << 12))

enum Windows
{
	WIN_FLOOR,
	WINDOW_COUNT,
};

enum CellTypes
{
    CELL_EMPTY,
    CELL_NORMAL,
    CELL_ENDPOINT,
    CELL_EXIT,
};

struct MazeMinimap
{
	u32* tilemapPtr;
    u16 floor;
};

// const rom data
static const u32 sMazeMinimapBgGfx[]    = INCBIN_U32("graphics/interface/maze_minimap.4bpp.lz");
static const u32 sMazeMinimapBgPal[]    = INCBIN_U32("graphics/interface/maze_minimap.gbapal.lz");
static const u32 sMazeMinimapBgMap[]    = INCBIN_U32("graphics/interface/maze_minimap.bin.lz");
static const u8 sMazeStarGfx[]          = INCBIN_U8("graphics/interface/maze_star.4bpp");
static const u16 sMazeStarPal[]         = INCBIN_U16("graphics/interface/maze_star.gbapal");

static const u8 sText_Floor[]           = _("Floor ");

static const struct WindowTemplate sMazeMinimapWinTemplates[WINDOW_COUNT + 1] =
{
	[WIN_FLOOR] =
	{
		.bg = 1,
		.tilemapLeft = 0,
		.tilemapTop = 0,
		.width = 8,
		.height = 2,
		.paletteNum = 15,
		.baseBlock = 1,
	},
	DUMMY_WIN_TEMPLATE
};

static const struct BgTemplate sMazeMinimapBgTemplates[] =
{
	{ // Minimap Background
		.bg = 2,
		.charBaseIndex = 0,
		.mapBaseIndex = 31,
		.screenSize = 0,
		.paletteMode = 0,
		.priority = 2,
		.baseTile = 0,
	},
	{ // Text
		.bg = 1,
		.charBaseIndex = 2,
		.mapBaseIndex = 6,
		.screenSize = 0,
		.paletteMode = 0,
		.priority = 1,
		.baseTile = 0,
	},
	{ // Instructions
		.bg = 0,
		.charBaseIndex = 1,
		.mapBaseIndex = 24,
		.screenSize = 0,
		.paletteMode = 0,
		.priority = 0,
		.baseTile = 0,
	}
};

static const struct OamData sStarOAM =
{
	.affineMode = ST_OAM_AFFINE_OFF,
	.objMode = ST_OAM_OBJ_NORMAL,
	.shape = SPRITE_SHAPE(16x16),
	.size = SPRITE_SIZE(16x16),
	.priority = 2, //On BG 2
};

static void SpriteCB_MazeStar(struct Sprite *sprite);
static const struct SpriteTemplate sMazeStarSpriteTemplate =
{
	.tileTag = TAG_MAZE_STAR,
	.paletteTag = TAG_MAZE_STAR,
	.oam = &sStarOAM,
	.anims = gDummySpriteAnimTable,
	.images = NULL,
	.affineAnims = gDummySpriteAffineAnimTable,
	.callback = SpriteCB_MazeStar,
};

static const struct SpriteSheet sMazeStarSpriteSheet = {
    sMazeStarGfx, sizeof(sMazeStarGfx), TAG_MAZE_STAR
};
static const struct SpritePalette sMazeStarSpritePalette = {
    sMazeStarPal, TAG_MAZE_STAR
};

// functions
static void PrintInstructions(void);
static void CleanWindows(void);
static void CommitWindows(void);
static void LoadMazeMinimapGfx(void);
static void ClearTasksAndGraphicalStructs(void);
static void ClearVramOamPlttRegs(void);
static void Task_MazeMinimapFadeOut(u8 taskId);
static void Task_MazeMinimapWaitForKeypress(u8 taskId);
static void Task_MazeMinimapFadeIn(u8 taskId);
static void InitMazeMinimap(void);
static void ShowCells(void);
static void ShowConnections(void);
static void ShowStar(void);
static bool8 GetMazeData(void);

EWRAM_DATA static struct MazeMinimap *sMazeMinimap = NULL;

// code
static void MainCB2_MazeMinimap(void)
{
	RunTasks();
	AnimateSprites();
	BuildOamBuffer();
	UpdatePaletteFade();
}

static void VBlankCB_MazeMinimap(void)
{
	LoadOam();
	ProcessSpriteCopyRequests();
    DoScheduledBgTilemapCopiesToVram();
	TransferPlttBuffer();
}

void CB2_MazeMinimap(void)
{
    switch (gMain.state) {
        default:
        case 0:
            SetVBlankCallback(NULL); 
            ClearVramOamPlttRegs();
            gMain.state++;
            break;
        case 1:
            ClearTasksAndGraphicalStructs();
            gMain.state++;
            break;
        case 2:
            sMazeMinimap->tilemapPtr = AllocZeroed(BG_SCREEN_SIZE);
            ResetBgsAndClearDma3BusyFlags(0);
            InitBgsFromTemplates(0, sMazeMinimapBgTemplates, 3);
            SetBgTilemapBuffer(2, sMazeMinimap->tilemapPtr);
            gMain.state++;
            break;
        case 3:
            LoadMazeMinimapGfx();
            gMain.state++;
            break;
        case 4:
            if (IsDma3ManagerBusyWithBgCopy() != TRUE)
            {
                ShowBg(0);
                ShowBg(1);
                ShowBg(2);
                CopyBgTilemapBufferToVram(2);
                gMain.state++;
            }
            break;
        case 5:
            InitWindows(sMazeMinimapWinTemplates);
            DeactivateAllTextPrinters();
            gMain.state++;
            break;
        case 6:
            BeginNormalPaletteFade(0xFFFFFFFF, 0, 16, 0, RGB_BLACK);
            gMain.state++;
            break;
        case 7:
            SetVBlankCallback(VBlankCB_MazeMinimap);
            InitMazeMinimap();
            CreateTask(Task_MazeMinimapFadeIn, 0);
            SetMainCallback2(MainCB2_MazeMinimap);
            break;
	}
}

static void Task_MazeMinimapFadeOut(u8 taskId)
{
	if (!gPaletteFade.active)
	{
        SetMainCallback2(CB2_ReturnToFieldContinueScript);
		Free(sMazeMinimap->tilemapPtr);
		Free(sMazeMinimap);
		FreeAllWindowBuffers();
		sMazeMinimap = NULL;
		DestroyTask(taskId);
	}
}

static void Task_MazeMinimapWaitForKeypress(u8 taskId)
{
	u8 i;
	u16 id;
	const u8* name;

    if (gMain.newKeys & B_BUTTON)
	{
		gSpecialVar_Result = FALSE;

		PlaySE(SE_RG_CARD_FLIP);
		BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, RGB_BLACK);
		gTasks[taskId].func = Task_MazeMinimapFadeOut;
    }
}

static void Task_MazeMinimapFadeIn(u8 taskId)
{
	if (!gPaletteFade.active)
	{
		gTasks[taskId].func = Task_MazeMinimapWaitForKeypress;
	}
}

static void PrintInstructions(void)
{
	const u8 colour[] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_WHITE, TEXT_COLOR_DARK_GRAY};
	/*{
		.bgColor = 0, //Transparent
		.fgColor = 1, //White
		.shadowColor = 2,
	};*/

	StringCopy(gStringVar1, sText_Floor);
	ConvertIntToDecimalStringN(gStringVar2, 1, STR_CONV_MODE_LEFT_ALIGN, 3);
	StringAppend(gStringVar1, gStringVar2);
	AddTextPrinterParameterized3(WIN_FLOOR, 0, 4, 0, colour, 0, gStringVar1);
}

static void DrawCellOnBg(u8 x, u8 y, enum CellTypes cellType)
{
    u16 *tilemapPtr = GetBgTilemapBuffer(2);
    if (cellType == CELL_EXIT) // maze exit is red
    {
        tilemapPtr[POS_TO_SCR_ADDR(x, y)] = SCR_MAP_ENTRY(0x13, 0, FALSE, FALSE);
        tilemapPtr[POS_TO_SCR_ADDR(x + 1, y)] = SCR_MAP_ENTRY(0x14, 0, FALSE, FALSE);
        tilemapPtr[POS_TO_SCR_ADDR(x, y + 1)] = SCR_MAP_ENTRY(0x15, 0, FALSE, FALSE);
        tilemapPtr[POS_TO_SCR_ADDR(x + 1, y + 1)] = SCR_MAP_ENTRY(0x16, 0, FALSE, FALSE);
    }
    else if (cellType == CELL_ENDPOINT) // endpoints + origin are green
    {
        tilemapPtr[POS_TO_SCR_ADDR(x, y)] = SCR_MAP_ENTRY(0x08, 0, FALSE, FALSE);
        tilemapPtr[POS_TO_SCR_ADDR(x + 1, y)] = SCR_MAP_ENTRY(0x09, 0, FALSE, FALSE);
        tilemapPtr[POS_TO_SCR_ADDR(x, y + 1)] = SCR_MAP_ENTRY(0x0F, 0, FALSE, FALSE);
        tilemapPtr[POS_TO_SCR_ADDR(x + 1, y + 1)] = SCR_MAP_ENTRY(0x10, 0, FALSE, FALSE);
    }
    else if (cellType == CELL_NORMAL)
    {
        tilemapPtr[POS_TO_SCR_ADDR(x, y)] = SCR_MAP_ENTRY(0x05, 0, FALSE, FALSE);
        tilemapPtr[POS_TO_SCR_ADDR(x + 1, y)] = SCR_MAP_ENTRY(0x06, 0, FALSE, FALSE);
        tilemapPtr[POS_TO_SCR_ADDR(x, y + 1)] = SCR_MAP_ENTRY(0x0C, 0, FALSE, FALSE);
        tilemapPtr[POS_TO_SCR_ADDR(x + 1, y + 1)] = SCR_MAP_ENTRY(0x0D, 0, FALSE, FALSE);
    }
}

static void DrawVerticalConnectionOnBg(u8 x, u8 y)
{
    u16 *tilemapPtr = GetBgTilemapBuffer(2);
    tilemapPtr[POS_TO_SCR_ADDR(x, y)] = SCR_MAP_ENTRY(0x11, 0, FALSE, FALSE);
    tilemapPtr[POS_TO_SCR_ADDR(x + 1, y)] = SCR_MAP_ENTRY(0x11, 0, TRUE, FALSE);
}

static void DrawHorizontalConnectionOnBg(u8 x, u8 y)
{
    u16 *tilemapPtr = GetBgTilemapBuffer(2);
    tilemapPtr[POS_TO_SCR_ADDR(x, y)] = SCR_MAP_ENTRY(0x07, 0, FALSE, FALSE);
    tilemapPtr[POS_TO_SCR_ADDR(x, y + 1)] = SCR_MAP_ENTRY(0x0E, 0, FALSE, FALSE);
}

static u8 GetCellType(u8 x, u8 y, struct Cell **endpoints)
{
    if (gMazeStruct->cells[x][y].connections == EMPTY)
        return CELL_EMPTY;
    else if (x == endpoints[0]->x && y == endpoints[0]->y)
        return CELL_EXIT;
    else if (gMazeStruct->cells[x][y].endpoint || (x == 0 && y == 0))
        return CELL_ENDPOINT;
    else
        return CELL_NORMAL;
}

static void ShowCells(void)
{
    s32 x, y;
    struct Cell **endpoints;
    endpoints = GetMazeEndpoints(gMazeStruct);
	for (x = 0; x < gMazeStruct->width; ++x)
        for (y = 0; y < gMazeStruct->height; ++y)       
            DrawCellOnBg(4 + x*3, 3 + y*3, GetCellType(x, y, endpoints));
    ScheduleBgCopyTilemapToVram(2);
}

static void ShowConnections(void)
{
    s32 x, y;

    for (x = 0; x < gMazeStruct->width; ++x)
    {
        for (y = 0; y < gMazeStruct->height; ++y)
        {
            if (gMazeStruct->cells[x][y].connections & NORTH)
                DrawVerticalConnectionOnBg(4 + x*3, 2 + y*3);
            if (gMazeStruct->cells[x][y].connections & SOUTH)
                DrawVerticalConnectionOnBg(4 + x*3, 5 + y*3);
            if (gMazeStruct->cells[x][y].connections & EAST)
                DrawHorizontalConnectionOnBg(6 + x*3, 3 + y*3);
            if (gMazeStruct->cells[x][y].connections & WEST)
                DrawHorizontalConnectionOnBg(3 + x*3, 3 + y*3);
        }
    }
}

static void ShowStar(void)
{
    u8 pal, spriteIndex;
    u8 x = gSaveBlock1Ptr->pos.x / (10);
    u8 y = gSaveBlock1Ptr->pos.y / (10);
	
    pal = LoadSpritePalette(&sMazeStarSpritePalette);
    LoadSpriteSheet(&sMazeStarSpriteSheet);
    spriteIndex = CreateSprite(&sMazeStarSpriteTemplate, 8*(5 + 3*x), 8*(4 + 3*y), 0);
}

static void SpriteCB_MazeStar(struct Sprite *sprite)
{
    /*if (!gPaletteFade.active)
    {
        if (sprite->data[2] == 0)
            sprite->data[7] = (sprite->oam.paletteNum * 16) + 256;
        if (sprite->data[2] > 256)
            sprite->data[2] = 0;

        if (sprite->data[2] < 128)
        {
            sprite->data[6] = Sin(sprite->data[2], 16);
            BlendPalette(sprite->data[7], 16, sprite->data[6], RGB_WHITE);
        }
        sprite->data[2] += 4;
    }*/
}

// Cleans the windows
static void CleanWindows(void)
{
    u8 i;
	for (i = 0; i < WINDOW_COUNT; i++)
		FillWindowPixelBuffer(i, PIXEL_FILL(0));
}
// Display commited windows
static void CommitWindows(void)
{
    u8 i;
	for (i = 0; i < WINDOW_COUNT; i++)
	{
		CopyWindowToVram(i, 3);
		PutWindowTilemap(i);
	}
}

static void InitMazeMinimap(void)
{
    // Clean windows.
	CleanWindows();
	CommitWindows();

    ShowCells();
    ShowConnections();
    ShowStar();
    PrintInstructions();
    
	// Display newly commited windows.
	CommitWindows();
}

static void LoadMazeMinimapGfx(void)
{	
    DecompressAndCopyTileDataToVram(2, &sMazeMinimapBgGfx, 0, 0, 0);
	LZDecompressWram(sMazeMinimapBgMap, sMazeMinimap->tilemapPtr);
	LoadCompressedPalette(sMazeMinimapBgPal, 0, 0x20);
	ListMenuLoadStdPalAt(0xC0, 1);
	Menu_LoadStdPalAt(0xF0);
}

static void ClearTasksAndGraphicalStructs(void)
{
	ScanlineEffect_Stop();
	ResetTasks();
	ResetSpriteData();
	ResetTempTileDataBuffers();
	ResetPaletteFade();
	FreeAllSpritePalettes();
}

static void ClearVramOamPlttRegs(void)
{
	DmaFill16(3, 0, VRAM, VRAM_SIZE);
	DmaFill32(3, 0, OAM, OAM_SIZE);
	DmaFill16(3, 0, PLTT, PLTT_SIZE);
	SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
	SetGpuReg(REG_OFFSET_BG3CNT, DISPCNT_MODE_0);
	SetGpuReg(REG_OFFSET_BG2CNT, DISPCNT_MODE_0);
	SetGpuReg(REG_OFFSET_BG1CNT, DISPCNT_MODE_0);
	SetGpuReg(REG_OFFSET_BG0CNT, DISPCNT_MODE_0);
	SetGpuReg(REG_OFFSET_BG3HOFS, DISPCNT_MODE_0);
	SetGpuReg(REG_OFFSET_BG3VOFS, DISPCNT_MODE_0);
	SetGpuReg(REG_OFFSET_BG2HOFS, DISPCNT_MODE_0);
	SetGpuReg(REG_OFFSET_BG2VOFS, DISPCNT_MODE_0);
	SetGpuReg(REG_OFFSET_BG1HOFS, DISPCNT_MODE_0);
	SetGpuReg(REG_OFFSET_BG1VOFS, DISPCNT_MODE_0);
	SetGpuReg(REG_OFFSET_BG0HOFS, DISPCNT_MODE_0);
	SetGpuReg(REG_OFFSET_BG0VOFS, DISPCNT_MODE_0);
}

void ShowMazeMinimap(void)
{
    gSpecialVar_Result = FALSE;
    sMazeMinimap = AllocZeroed(sizeof(struct MazeMinimap));
    if (GetMazeData() && sMazeMinimap != NULL)
	{
        PlaySE(SE_RG_CARD_FLIPPING);
        PlayRainStoppingSoundEffect();
		SetMainCallback2(CB2_MazeMinimap);
	}
}

static bool8 GetMazeData(void)
{
	return TRUE;
}