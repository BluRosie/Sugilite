#include "global.h"
#include "main.h"
#include "battle.h"
#include "battle_anim.h"
#include "battle_factory.h"
#include "battle_interface.h"
#include "battle_message.h"
#include "battle_tent.h"
#include "bg.h"
#include "contest.h"
#include "contest_effect.h"
#include "data.h"
#include "daycare.h"
#include "decompress.h"
#include "dynamic_placeholder_text_util.h"
#include "event_data.h"
#include "frontier_util.h"
#include "gpu_regs.h"
#include "graphics.h"
#include "international_string_util.h"
#include "item.h"
#include "link.h"
#include "m4a.h"
#include "malloc.h"
#include "menu.h"
#include "menu_helpers.h"
#include "party_menu.h"
#include "palette.h"
#include "pokeball.h"
#include "pokemon.h"
#include "pokemon_icon.h"
#include "pokemon_storage_system.h"
#include "pokemon_summary_screen.h"
#include "region_map.h"
#include "scanline_effect.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "strings.h"
#include "task.h"
#include "text.h"
#include "tv.h"
#include "window.h"
#include "constants/items.h"
#include "constants/moves.h"
#include "constants/party_menu.h"
#include "constants/region_map_sections.h"
#include "constants/rgb.h"
#include "constants/songs.h"
#include "constants/battle_config.h"

#define MOVE_SELECTOR_SPRITES_COUNT 2
#define HP_BAR_SPRITES_COUNT 9
#define EXP_BAR_SPRITES_COUNT 11

#define MOVE_SLOT_DELTA     28

enum PokemonSummaryScreenDataSpriteIds
{
    SPRITE_ARR_ID_MON,
    SPRITE_ARR_ID_BALL,
    SPRITE_ARR_ID_STATUS,
    SPRITE_ARR_ID_POKERUS_CURED_SYMBOL,
    SPRITE_ARR_ID_SHINY_STAR,
    SPRITE_ARR_ID_MON_ICON,
    SPRITE_ARR_ID_TYPE,
    SPRITE_ARR_ID_TYPE_2,
    SPRITE_ARR_ID_MOVE_1_TYPE,
    SPRITE_ARR_ID_MOVE_2_TYPE,
    SPRITE_ARR_ID_MOVE_3_TYPE,
    SPRITE_ARR_ID_MOVE_4_TYPE,
    SPRITE_ARR_ID_MOVE_5_TYPE,  // move to learn
    SPRITE_ARR_ID_MOVE_SELECTOR1,
    SPRITE_ARR_ID_MOVE_SELECTOR2 = SPRITE_ARR_ID_MOVE_SELECTOR1 + MOVE_SELECTOR_SPRITES_COUNT,
    SPRITE_ARR_ID_COUNT = SPRITE_ARR_ID_MOVE_SELECTOR2 + MOVE_SELECTOR_SPRITES_COUNT
};

#define TILE_EMPTY_HEART  0x1201
#define TILE_FILLED_HEART 0x1200
#define TILE_EMPTY        0x1000

enum
{
    SUMMARY_BG_INFO_PAGE,
    SUMMARY_BG_SKILLS_PAGE,
    SUMMARY_BG_BATTLE_MOVES_PAGE,
    SUMMARY_BG_CONTEST_MOVES_PAGE,
    SUMMARY_BG_BATTLE_MOVES_DETAILS_PAGE,
    SUMMARY_BG_CONTEST_MOVES_DETAILS_PAGE,
    SUMMARY_BG_EGG_INFO_PAGE,
    SUMMARY_BG_COUNT,
};

static EWRAM_DATA struct PokemonSummaryScreenData
{
    /*0x00*/ union {
        struct Pokemon *mons;
        struct BoxPokemon *boxMons;
    } monList;
    /*0x04*/ MainCallback callback;
    /*0x0C*/ struct Pokemon currentMon;
    /*0x70*/ struct PokeSummary
    {
        u16 species; // 0x0
        u16 species2; // 0x2
        u8 isEgg; // 0x4
        u8 level; // 0x5
        u8 ribbonCount; // 0x6
        u8 ailment; // 0x7
        u8 abilityNum; // 0x8
        u8 metLocation; // 0x9
        u8 metLevel; // 0xA
        u8 metGame; // 0xB
        u32 pid; // 0xC
        u32 exp; // 0x10
        u16 moves[MAX_MON_MOVES]; // 0x14
        u8 pp[MAX_MON_MOVES]; // 0x1C
        u32 currentHP; // 0x20
        u32 maxHP; // 0x22
        u16 stat[5];
        u16 atk; // 0x24
        u16 def;
        u16 speed;
        u16 spatk;
        u16 spdef;
        u16 item; // 0x2E
        u16 friendship; // 0x30
        u8 OTGender; // 0x32
        u8 nature; // 0x33
        u8 ppBonuses; // 0x34
        u8 sanity; // 0x35
        u8 OTName[17]; // 0x36
        u32 OTID; // 0x48
    } summary;
    u16 bgTilemapBuffers[2][SUMMARY_BG_COUNT][0x400];
    u8 mode;
    bool8 isBoxMon;
    u8 curMonIndex;
    u8 maxMonIndex;
    u8 currPageIndex;
    u8 minPageIndex;
    u8 maxPageIndex;
    bool8 lockMonFlag; // This is used to prevent the player from changing pokemon in the move deleter select, etc, but it is not needed because the input is handled differently there
    u16 newMove;
    u8 firstMoveIndex;
    u8 secondMoveIndex;
    bool8 lockMovesFlag; // This is used to prevent the player from changing position of moves in a battle or when trading.
    u8 bgDisplayOrder; // Determines the order page backgrounds are loaded while scrolling between them
    u8 filler40CA;
    u8 windowIds[8];
    u8 spriteIds[SPRITE_ARR_ID_COUNT];
    bool8 unk40EF;
    s16 switchCounter; // Used for various switch statement cases that decompress/load graphics or pokemon data
    u8 unk_filler4[6];
    u8 splitIconSpriteId;
} *sMonSummaryScreen = NULL;

EWRAM_DATA u8 gLastViewedMonIndex = 0;
static EWRAM_DATA u8 sMoveSlotToReplace = 0;
ALIGNED(4) static EWRAM_DATA u8 sAnimDelayTaskId = 0;
static EWRAM_DATA struct HealthBar
{
    struct Sprite * sprites[HP_BAR_SPRITES_COUNT];
    u16 spritePositions[HP_BAR_SPRITES_COUNT];
    u16 tileTag;
    u16 palTag;
} *sHealthBar = NULL;

static EWRAM_DATA struct ExpBar
{
    struct Sprite * sprites[EXP_BAR_SPRITES_COUNT];
    u16 spritePositions[EXP_BAR_SPRITES_COUNT];
    u16 tileTag;
    u16 palTag;
} *sExpBar = NULL;

// forward declarations
static bool8 LoadGraphics(void);
static void CB2_InitSummaryScreen(void);
static void InitBGs(void);
static bool8 DecompressGraphics(void);
static void CopyMonToSummaryStruct(struct Pokemon* a);
static bool8 ExtractMonDataToSummaryStruct(struct Pokemon* a);
static void SetDefaultTilemaps(void);
static void CloseSummaryScreen(u8 taskId);
static void Task_HandleInput(u8 taskId);
static void ChangeSummaryPokemon(u8 taskId, s8 a);
static void Task_ChangeSummaryMon(u8 taskId);
static s8 AdvanceMonIndex(s8 delta);
static s8 AdvanceMultiBattleMonIndex(s8 delta);
static bool8 IsValidToViewInMulti(struct Pokemon* mon);
static void ChangePage(u8 taskId, s8 a);
static void PssScrollRight(u8 taskId);
static void PssScrollRightEnd(u8 taskId);
static void PssScrollLeft(u8 taskId);
static void PssScrollLeftEnd(u8 taskId);
static void SwitchToMoveSelection(u8 taskId);
static void Task_HandleInput_MoveSelect(u8 taskId);
static bool8 HasMoreThanOneMove(void);
static void ChangeSelectedMove(s16 *taskData, s8 direction, u8 *moveIndexPtr);
static void CloseMoveSelectMode(u8 taskId);
static void SwitchToMovePositionSwitchMode(u8 a);
static void Task_HandleInput_MovePositionSwitch(u8 taskId);
static void ExitMovePositionSwitchMode(u8 taskId, bool8 swapMoves);
static void SwapMonMoves(struct Pokemon *mon, u8 moveIndex1, u8 moveIndex2);
static void SwapBoxMonMoves(struct BoxPokemon *mon, u8 moveIndex1, u8 moveIndex2);
static void Task_SetHandleReplaceMoveInput(u8 taskId);
static void Task_HandleReplaceMoveInput(u8 taskId);
static bool8 CanReplaceMove(void);
static void ShowCantForgetHMsWindow(u8 taskId);
static void Task_HandleInputCantForgetHMsMoves(u8 taskId);
static void TilemapFiveMovesDisplay(void);
static void CreateSetShinyStar(struct Pokemon *mon);
static void LimitEggSummaryPageDisplay(void);
static void ResetWindows(void);
static void PrintTextOnWindow(u8 windowId, const u8 *string, u8 x, u8 y, u8 lineSpacing, u8 colorId);
static void PrintPSSHeader(u8 page);
static void PrintPokemonName(bool32 movePage);
static void PrintMonLevel(struct PokeSummary *summary);
static void PrintMonNickname(bool32);
static void PrintGenderSymbol(struct Pokemon *mon, u16 a);
static void PutPageWindowTilemaps(u8 a);
static void ClearPageWindowTilemaps(u8 a);
static void RemoveWindowByIndex(u8 a);
static void PrintChangeMonPageText(u8 a);
static void CreateChangePageTask(u8 a);
static void PrintInfoPageText(void);
static void Task_PrintInfoPage(u8 taskId);
static void PrintMonDexNum(void);
static void PrintMonSpecies(void);
static void PrintMonOTName(void);
static void PrintMonOTID(void);
static void BufferMonTrainerMemo(void);
static void PrintMonTrainerMemo(void);
static void BufferNatureString(void);
static void GetMetLevelString(u8 *a);
static bool8 DoesMonOTMatchOwner(void);
static bool8 DidMonComeFromGBAGames(void);
static bool8 IsInGamePartnerMon(void);
static void PrintEggName(void);
static void PrintEggMemo(void);
static void PrintHeldItemName(void);
static void ClearHeldItemText(void);
static void Task_PrintSkillsPage(u8 taskId);
static void PrintSkillsPageText(void);
static void PrintMonStats(void);
static void PrintHP(void);
static void PrintExp(void);
static void PrintMonAbilityName(void);
static void PrintMonAbilityDescription(void);
static void PrintBattleMoves(void);
static void Task_PrintBattleMoves(u8 taskId);
static void PrintMoveNameAndPP(u8 a);
static void PrintMoveDetails(u16 a);
static void PrintNewMoveDetailsOrCancelText(void);
static void SwapMovesNamesPP(u8 moveIndex1, u8 moveIndex2);
static void PrintHMMovesCantBeForgotten(void);
static void ResetSpriteIds(void);
static void SetSpriteInvisibility(u8 spriteArrayId, bool8 invisible);
static void HideTypeSprites(void);
static void HideMoveSelectorSprites(void);
static void SwapMonSpriteIconSpriteVisibility(bool8 invisible);
static void ToggleSpritesForMovesPage(void);
static void SetTypeIcons(void);
static void CreateMoveTypeIcons(void);
static void SetMonTypeIcons(u8 page);
static void SetMoveTypeIcons(void);
static void SetNewMoveTypeIcon(void);
static void SwapMovesTypeSprites(u8 moveIndex1, u8 moveIndex2);
static u8 LoadMonGfxAndSprite(struct Pokemon *a, s16 *b);
static u8 CreateMonSprite(struct Pokemon *unused);
static void SpriteCB_Pokemon(struct Sprite *);
static void StopPokemonAnimations(void);
static void CreateCaughtBallSprite(struct Pokemon *mon);
static void CreateMonIconSprite(void);
static void CreateSetStatusSprite(void);
static void CreateMoveSelectorSprites(u8 idArrayStart);
static void SpriteCb_MoveSelector(struct Sprite *sprite);
static void DestroyMoveSelectorSprites(u8 firstArrayId);
static void SetMainMoveSelectorColor(u8 whichColor);
static void KeepMoveSelectorVisible(u8 firstSpriteId);
static u8 AddWindowFromTemplateList(const struct WindowTemplate *template, u8 templateId);
static void CreateHealthBarSprites(u16 tileTag, u16 palTag);
static void ConfigureHealthBarSprites(void);
static void DestroyHealthBarSprites(void);
static void SetHealthBarSprites(u8 invisible);
static void CreateExpBarSprites(u16 tileTag, u16 palTag);
static void ConfigureExpBarSprites(void);
static void DestroyExpBarSprites(void);
static void SetExpBarSprites(u8 invisible);

// const rom data
#include "data/text/move_descriptions.h"
#include "data/text/nature_names.h"

static const u32 sSummaryScreenBitmap[]                    = INCBIN_U32("graphics/summary_screen/tiles.4bpp.lz");
static const u32 sSummaryScreenPalette[]                   = INCBIN_U32("graphics/summary_screen/pal.gbapal.lz");
static const u32 sSummaryScreenTextPalette[]               = INCBIN_U32("graphics/summary_screen/text.gbapal.lz");
static const u32 sPageInfo_BgTilemap[]                     = INCBIN_U32("graphics/summary_screen/pg1_info/bg.bin.lz");
static const u32 sPageSkills_BgTilemap[]                   = INCBIN_U32("graphics/summary_screen/pg2_skills/bg.bin.lz");
static const u32 sPageSkills_LabelsTilemap[]               = INCBIN_U32("graphics/summary_screen/pg2_skills/labels.bin.lz");
static const u32 sPageMoves_BgTilemap[]                    = INCBIN_U32("graphics/summary_screen/pg3_battle_moves/bg.bin.lz");
static const u32 sPageMoves_BgTilemap_FiveMoves[]          = INCBIN_U32("graphics/summary_screen/pg3_battle_moves/five_moves.bin.lz");

static const u32 sSplitIcons_Gfx[]         = INCBIN_U32("graphics/summary_screen/sprites/split_icons.4bpp.lz");
static const u32 sSplitIcons_Pal[]         = INCBIN_U32("graphics/summary_screen/sprites/split_icons.gbapal.lz");
static const u32 sShinyStar_Gfx[]          = INCBIN_U32("graphics/summary_screen/sprites/shiny_star.4bpp.lz");
static const u32 sExpBar_Gfx[]             = INCBIN_U32("graphics/summary_screen/sprites/exp_bar.4bpp.lz");
static const u32 sHealthBar_Gfx[]          = INCBIN_U32("graphics/summary_screen/sprites/health_bar_green.4bpp.lz");
static const u16 sHealthBar_Green_Pal[]    = INCBIN_U16("graphics/summary_screen/sprites/health_bar_green.gbapal");
static const u16 sHealthBar_Yellow_Pal[]   = INCBIN_U16("graphics/summary_screen/sprites/health_bar_yellow.gbapal");
static const u16 sHealthBar_Red_Pal[]      = INCBIN_U16("graphics/summary_screen/sprites/health_bar_red.gbapal");
static const u32 sMoveSelector_Gfx[]       = INCBIN_U32("graphics/summary_screen/sprites/move_selector.4bpp.lz");

static const struct BgTemplate sBgTemplates[] =
{
    {
        .bg = 0,
        .charBaseIndex = 0,
        .mapBaseIndex = 31,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 0,
        .baseTile = 0,
    },
    {
        .bg = 1,
        .charBaseIndex = 2,
        .mapBaseIndex = 27,
        .screenSize = 1,
        .paletteMode = 0,
        .priority = 1,
        .baseTile = 0,
    },
    {
        .bg = 2,
        .charBaseIndex = 2,
        .mapBaseIndex = 25,
        .screenSize = 1,
        .paletteMode = 0,
        .priority = 2,
        .baseTile = 0,
    },
    {
        .bg = 3,
        .charBaseIndex = 2,
        .mapBaseIndex = 29,
        .screenSize = 1,
        .paletteMode = 0,
        .priority = 3,
        .baseTile = 0,
    },
};

static const s8 sMultiBattleOrder[] = {0, 2, 3, 1, 4, 5};

enum HeaderWindowIds {
    WIN_HEADER,
    WIN_NAME,
};

static const struct WindowTemplate sPssHeaderWindowTemplates[] =
{
    [WIN_HEADER] = {
        .bg = 0,
        .tilemapLeft = 1,
        .tilemapTop = 0,
        .width = 28,
        .height = 2,
        .paletteNum = 14,
        .baseBlock = 1,
    },
    [WIN_NAME] = {
        .bg = 0,
        .tilemapLeft = 0,
        .tilemapTop = 2,
        .width = 15,
        .height = 2,
        .paletteNum = 14,
        .baseBlock = 58,
    },
    DUMMY_WIN_TEMPLATE,
};


enum InfoPageWindowIds {
    WIN_INFO,
    WIN_MEMO,
};
static const struct WindowTemplate sPageInfoTemplate[] =
{
    [WIN_INFO] = {
        .bg = 0,
        .tilemapLeft = 20,
        .tilemapTop = 2,
        .width = 10,
        .height = 12,
        .paletteNum = 14,
        .baseBlock = 90,
    },
    [WIN_MEMO] = {
        .bg = 0,
        .tilemapLeft = 1,
        .tilemapTop = 14,
        .width = 28,
        .height = 6,
        .paletteNum = 14,
        .baseBlock = 210,
    },
};

enum StatsPageWindowIds {
    WIN_HP,
    WIN_STATS,
    WIN_ABILITY_DESC,
    WIN_ABILITY_NAME,
    WIN_EXP,
};
static const struct WindowTemplate sPageSkillsTemplate[] =
{
    [WIN_HP] = {
        .bg = 0,
        .tilemapLeft = 20,
        .tilemapTop = 2,
        .width = 10,
        .height = 2,
        .paletteNum = 14,
        .baseBlock = 90,
    },
    [WIN_STATS] = {
        .bg = 0,
        .tilemapLeft = 25,
        .tilemapTop = 4,
        .width = 6,
        .height = 9,
        .paletteNum = 14,
        .baseBlock = 110,
    },
    [WIN_ABILITY_DESC] = {
        .bg = 0,
        .tilemapLeft = 2,
        .tilemapTop = 17,
        .width = 28,
        .height = 3,
        .paletteNum = 14,
        .baseBlock = 165,
    },
    [WIN_ABILITY_NAME] = {
        .bg = 0,
        .tilemapLeft = 9,
        .tilemapTop = 15,
        .width = 11,
        .height = 3,
        .paletteNum = 14,
        .baseBlock = 250,
    },
    [WIN_EXP] = {
        .bg = 0,
        .tilemapLeft = 8,
        .tilemapTop = 13,
        .width = 21,
        .height = 3,
        .paletteNum = 14,
        .baseBlock = 290,
    },
};

enum MovePageWindowIds {
    WIN_MOVE_NAMES,
    WIN_MOVE_DETAILS,
    WIN_MOVE_DESCRIPTION,
};
static const struct WindowTemplate sPageMovesTemplate[] =
{
    [WIN_MOVE_NAMES] = {
        .bg = 0,
        .tilemapLeft = 20,
        .tilemapTop = 2,
        .width = 14,
        .height = 18,
        .paletteNum = 14,
        .baseBlock = 90
    },
    [WIN_MOVE_DETAILS] = {
        .bg = 0,
        .tilemapLeft = 6,
        .tilemapTop = 7,
        .width = 5,
        .height = 4,
        .paletteNum = 14,
        .baseBlock = 350,
    },
    [WIN_MOVE_DESCRIPTION] = {
        .bg = 0,
        .tilemapLeft = 1,
        .tilemapTop = 12,
        .width = 14,
        .height = 8,
        .paletteNum = 14,
        .baseBlock = 370,
    },
};

enum PokemonSummaryScreenTextColors
{
    PSS_TEXT_COLOR_DARK_GRAY,
    PSS_TEXT_COLOR_WHITE,
    PSS_TEXT_COLOR_BLUE,
    PSS_TEXT_COLOR_RED,
    PSS_TEXT_COLOR_WHITE_LIGHT_SHADOW,
    PSS_TEXT_COLOR_DARK_GRAY_LIGHT_SHADOW,
    PSS_TEXT_COLOR_PP_WHITE,
    PSS_TEXT_COLOR_PP_YELLOW,
    PSS_TEXT_COLOR_PP_ORANGE,
    PSS_TEXT_COLOR_PP_RED,
};
static const u8 sTextColors[][3] =
{
    [PSS_TEXT_COLOR_DARK_GRAY]              = { 0,  2, 10},
    [PSS_TEXT_COLOR_WHITE]                  = { 0,  1,  2},
    [PSS_TEXT_COLOR_BLUE]                   = { 0,  8,  9},
    [PSS_TEXT_COLOR_RED]                    = { 0,  4,  5},
    [PSS_TEXT_COLOR_WHITE_LIGHT_SHADOW]     = { 0,  1, 11},
    [PSS_TEXT_COLOR_DARK_GRAY_LIGHT_SHADOW] = { 0,  2, 11},
    [PSS_TEXT_COLOR_PP_WHITE]               = { 0,  1, 12},
    [PSS_TEXT_COLOR_PP_YELLOW]              = { 0,  7, 12},
    [PSS_TEXT_COLOR_PP_ORANGE]              = { 0,  5, 12},
    [PSS_TEXT_COLOR_PP_RED]                 = { 0,  4, 12},
};

static void (*const sChangedMonTextPrintFunctions[])(void) =
{
    [PSS_PAGE_INFO]          = PrintInfoPageText,
    [PSS_PAGE_SKILLS]        = PrintSkillsPageText,
    [PSS_PAGE_BATTLE_MOVES]  = PrintBattleMoves,
};

static void (*const sChangePageTextPrinterTasks[])(u8 taskId) =
{
    [PSS_PAGE_INFO]          = Task_PrintInfoPage,
    [PSS_PAGE_SKILLS]        = Task_PrintSkillsPage,
    [PSS_PAGE_BATTLE_MOVES]  = Task_PrintBattleMoves,
};

static const u8 sHPLayout[] = _("{DYNAMIC 0}/{DYNAMIC 1}");
static const u8 sMovesPPLayout[] = _("{PP}{DYNAMIC 0}/{DYNAMIC 1}");

#define TAG_MOVE_SELECTOR 30000
#define TAG_MON_STATUS 30001
#define TAG_MOVE_TYPES 30002
#define TAG_SPLIT_ICONS 30003
#define TAG_SHINY_STAR 30005
#define TAG_HEALTH_BAR 0x78
#define TAG_EXP_BAR 0x82

static const struct OamData sOamData_SplitIcons =
{
    .size = SPRITE_SIZE(32x16),
    .shape = SPRITE_SHAPE(32x16),
    .priority = 0,
};

static const struct CompressedSpriteSheet sSpriteSheet_SplitIcons =
{
    .data = sSplitIcons_Gfx,
    .size = 32*16*3/2,
    .tag = TAG_SPLIT_ICONS,
};

static const struct CompressedSpritePalette sSplitIconsSpritePal =
{
    .data = sSplitIcons_Pal,
    .tag = TAG_MOVE_SELECTOR
};

static const union AnimCmd sSpriteAnim_SplitIcon0[] =
{
    ANIMCMD_FRAME(0, 0),
    ANIMCMD_END
};

static const union AnimCmd sSpriteAnim_SplitIcon1[] =
{
    ANIMCMD_FRAME(8, 0),
    ANIMCMD_END
};

static const union AnimCmd sSpriteAnim_SplitIcon2[] =
{
    ANIMCMD_FRAME(16, 0),
    ANIMCMD_END
};

static const union AnimCmd *const sSpriteAnimTable_SplitIcons[] =
{
    sSpriteAnim_SplitIcon0,
    sSpriteAnim_SplitIcon1,
    sSpriteAnim_SplitIcon2,
};

static const struct SpriteTemplate sSpriteTemplate_SplitIcons =
{
    .tileTag = TAG_SPLIT_ICONS,
    .paletteTag = TAG_MOVE_SELECTOR,
    .oam = &sOamData_SplitIcons,
    .anims = sSpriteAnimTable_SplitIcons,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};

static const struct OamData sOamData_ShinyStar =
{
    .size = SPRITE_SIZE(16x16),
    .shape = SPRITE_SHAPE(16x16),
    .priority = 0,
};

static const struct CompressedSpriteSheet sSpriteSheet_ShinyStar =
{
    .data = sShinyStar_Gfx,
    .size = 16*16*3/2,
    .tag = TAG_SHINY_STAR,
};
static const union AnimCmd sSpriteAnim_ShinyStar[] =
{
    ANIMCMD_FRAME(0, 0),
    ANIMCMD_END
};

static const union AnimCmd *const sSpriteAnimTable_ShinyStar[] =
{
    sSpriteAnim_ShinyStar,
};

static const struct SpriteTemplate sSpriteTemplate_ShinyStar =
{
    .tileTag = TAG_SHINY_STAR,
    .paletteTag = TAG_MOVE_SELECTOR,
    .oam = &sOamData_ShinyStar,
    .anims = sSpriteAnimTable_ShinyStar,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};

static const struct OamData sOamData_MoveTypes =
{
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = 0,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(32x16),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(32x16),
    .tileNum = 0,
    .priority = 1,
    .paletteNum = 0,
    .affineParam = 0,
};
static const union AnimCmd sSpriteAnim_TypeNormal[] = {
    ANIMCMD_FRAME(TYPE_NORMAL * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeFighting[] = {
    ANIMCMD_FRAME(TYPE_FIGHTING * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeFlying[] = {
    ANIMCMD_FRAME(TYPE_FLYING * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypePoison[] = {
    ANIMCMD_FRAME(TYPE_POISON * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeGround[] = {
    ANIMCMD_FRAME(TYPE_GROUND * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeRock[] = {
    ANIMCMD_FRAME(TYPE_ROCK * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeBug[] = {
    ANIMCMD_FRAME(TYPE_BUG * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeGhost[] = {
    ANIMCMD_FRAME(TYPE_GHOST * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeSteel[] = {
    ANIMCMD_FRAME(TYPE_STEEL * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeMystery[] = {
    ANIMCMD_FRAME(TYPE_MYSTERY * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeFire[] = {
    ANIMCMD_FRAME(TYPE_FIRE * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeWater[] = {
    ANIMCMD_FRAME(TYPE_WATER * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeGrass[] = {
    ANIMCMD_FRAME(TYPE_GRASS * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeElectric[] = {
    ANIMCMD_FRAME(TYPE_ELECTRIC * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypePsychic[] = {
    ANIMCMD_FRAME(TYPE_PSYCHIC * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeIce[] = {
    ANIMCMD_FRAME(TYPE_ICE * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeDragon[] = {
    ANIMCMD_FRAME(TYPE_DRAGON * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeDark[] = {
    ANIMCMD_FRAME(TYPE_DARK * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeFairy[] = {
    ANIMCMD_FRAME(TYPE_FAIRY * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_CategoryCool[] = {
    ANIMCMD_FRAME((CONTEST_CATEGORY_COOL + NUMBER_OF_MON_TYPES) * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_CategoryBeauty[] = {
    ANIMCMD_FRAME((CONTEST_CATEGORY_BEAUTY + NUMBER_OF_MON_TYPES) * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_CategoryCute[] = {
    ANIMCMD_FRAME((CONTEST_CATEGORY_CUTE + NUMBER_OF_MON_TYPES) * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_CategorySmart[] = {
    ANIMCMD_FRAME((CONTEST_CATEGORY_SMART + NUMBER_OF_MON_TYPES) * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_CategoryTough[] = {
    ANIMCMD_FRAME((CONTEST_CATEGORY_TOUGH + NUMBER_OF_MON_TYPES) * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd *const sSpriteAnimTable_MoveTypes[NUMBER_OF_MON_TYPES + CONTEST_CATEGORIES_COUNT] = {
    sSpriteAnim_TypeNormal,
    sSpriteAnim_TypeFighting,
    sSpriteAnim_TypeFlying,
    sSpriteAnim_TypePoison,
    sSpriteAnim_TypeGround,
    sSpriteAnim_TypeRock,
    sSpriteAnim_TypeBug,
    sSpriteAnim_TypeGhost,
    sSpriteAnim_TypeSteel,
    sSpriteAnim_TypeMystery,
    sSpriteAnim_TypeFire,
    sSpriteAnim_TypeWater,
    sSpriteAnim_TypeGrass,
    sSpriteAnim_TypeElectric,
    sSpriteAnim_TypePsychic,
    sSpriteAnim_TypeIce,
    sSpriteAnim_TypeDragon,
    sSpriteAnim_TypeDark,
    sSpriteAnim_TypeFairy,
    sSpriteAnim_CategoryCool,
    sSpriteAnim_CategoryBeauty,
    sSpriteAnim_CategoryCute,
    sSpriteAnim_CategorySmart,
    sSpriteAnim_CategoryTough,
};

const struct CompressedSpriteSheet gSpriteSheet_MoveTypes =
{
    .data = gMoveTypes_Gfx,
    .size = (NUMBER_OF_MON_TYPES + CONTEST_CATEGORIES_COUNT) * 0x100,
    .tag = TAG_MOVE_TYPES
};
const struct SpriteTemplate gSpriteTemplate_MoveTypes =
{
    .tileTag = TAG_MOVE_TYPES,
    .paletteTag = TAG_MOVE_TYPES,
    .oam = &sOamData_MoveTypes,
    .anims = sSpriteAnimTable_MoveTypes,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};
const u8 gMoveTypeToOamPaletteNum[NUMBER_OF_MON_TYPES + CONTEST_CATEGORIES_COUNT] =
{
    [TYPE_NORMAL] = 13,
    [TYPE_FIGHTING] = 13,
    [TYPE_FLYING] = 14,
    [TYPE_POISON] = 14,
    [TYPE_GROUND] = 13,
    [TYPE_ROCK] = 13,
    [TYPE_BUG] = 15,
    [TYPE_GHOST] = 14,
    [TYPE_STEEL] = 13,
    [TYPE_MYSTERY] = 15,
    [TYPE_FIRE] = 13,
    [TYPE_WATER] = 14,
    [TYPE_GRASS] = 15,
    [TYPE_ELECTRIC] = 13,
    [TYPE_PSYCHIC] = 14,
    [TYPE_ICE] = 14,
    [TYPE_DRAGON] = 15,
    [TYPE_DARK] = 13,
    [TYPE_FAIRY] = 14,
    [NUMBER_OF_MON_TYPES + CONTEST_CATEGORY_COOL] = 13,
    [NUMBER_OF_MON_TYPES + CONTEST_CATEGORY_BEAUTY] = 14,
    [NUMBER_OF_MON_TYPES + CONTEST_CATEGORY_CUTE] = 14,
    [NUMBER_OF_MON_TYPES + CONTEST_CATEGORY_SMART] = 15,
    [NUMBER_OF_MON_TYPES + CONTEST_CATEGORY_TOUGH] = 13,
};
static const struct OamData sOamData_MoveSelector =
{
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = 0,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(64x32),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(64x32),
    .tileNum = 0,
    .priority = 1,
    .paletteNum = 0,
    .affineParam = 0,
};
static const union AnimCmd sSpriteAnim_MoveSelectorLeftRed[] = {
    ANIMCMD_FRAME(0),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_MoveSelectorRightRed[] = {
    ANIMCMD_FRAME(64),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_MoveSelectorLeftBlue[] = {
    ANIMCMD_FRAME(32),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_MoveSelectorRightBlue[] = {
    ANIMCMD_FRAME(96),
    ANIMCMD_END
};
// All except left, middle and right are unused
static const union AnimCmd *const sSpriteAnimTable_MoveSelector[] = {
    sSpriteAnim_MoveSelectorLeftRed,
    sSpriteAnim_MoveSelectorRightRed,
    sSpriteAnim_MoveSelectorLeftBlue,
    sSpriteAnim_MoveSelectorRightBlue
};
static const struct CompressedSpriteSheet sMoveSelectorSpriteSheet =
{
    .data = sMoveSelector_Gfx,
    .size = 0x1000,
    .tag = TAG_MOVE_SELECTOR
};
static const struct SpriteTemplate sMoveSelectorSpriteTemplate =
{
    .tileTag = TAG_MOVE_SELECTOR,
    .paletteTag = TAG_MOVE_SELECTOR,
    .oam = &sOamData_MoveSelector,
    .anims = sSpriteAnimTable_MoveSelector,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};
static const struct OamData sOamData_StatusCondition =
{
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = 0,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(32x8),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(32x8),
    .tileNum = 0,
    .priority = 0,
    .paletteNum = 0,
    .affineParam = 0,
};
static const union AnimCmd sSpriteAnim_StatusPoison[] = {
    ANIMCMD_FRAME(0, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_StatusParalyzed[] = {
    ANIMCMD_FRAME(4, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_StatusSleep[] = {
    ANIMCMD_FRAME(8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_StatusFrozen[] = {
    ANIMCMD_FRAME(12, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_StatusBurn[] = {
    ANIMCMD_FRAME(16, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_StatusPokerus[] = {
    ANIMCMD_FRAME(20, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_StatusFaint[] = {
    ANIMCMD_FRAME(24, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd *const sSpriteAnimTable_StatusCondition[] = {
    sSpriteAnim_StatusPoison,
    sSpriteAnim_StatusParalyzed,
    sSpriteAnim_StatusSleep,
    sSpriteAnim_StatusFrozen,
    sSpriteAnim_StatusBurn,
    sSpriteAnim_StatusPokerus,
    sSpriteAnim_StatusFaint,
};
static const struct CompressedSpriteSheet sStatusIconsSpriteSheet =
{
    .data = gStatusGfx_Icons,
    .size = 0x380,
    .tag = TAG_MON_STATUS
};
static const struct CompressedSpritePalette sStatusIconsSpritePalette =
{
    .data = gStatusPal_Icons,
    .tag = TAG_MON_STATUS
};
static const struct SpriteTemplate sSpriteTemplate_StatusCondition =
{
    .tileTag = TAG_MON_STATUS,
    .paletteTag = TAG_MON_STATUS,
    .oam = &sOamData_StatusCondition,
    .anims = sSpriteAnimTable_StatusCondition,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};

static const struct OamData sOamData_ExpHealthBars = {
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = FALSE,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(8x8),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(8x8),
    .tileNum = 0,
    .priority = 0,
    .paletteNum = 0
};

static const union AnimCmd sSpriteAnim_ExpHealthBarDivisionEmpty[] = 
{
    ANIMCMD_FRAME(0, 20),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd sSpriteAnim_ExpHealthBarDivision1[] = 
{
    ANIMCMD_FRAME(1, 20),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd sSpriteAnim_ExpHealthBarDivision2[] = 
{
    ANIMCMD_FRAME(2, 20),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd sSpriteAnim_ExpHealthBarDivision3[] = 
{
    ANIMCMD_FRAME(3, 20),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd sSpriteAnim_ExpHealthBarDivision4[] = 
{
    ANIMCMD_FRAME(4, 20),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd sSpriteAnim_ExpHealthBarDivision5[] = 
{
    ANIMCMD_FRAME(5, 20),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd sSpriteAnim_ExpHealthBarDivision6[] = 
{
    ANIMCMD_FRAME(6, 20),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd sSpriteAnim_ExpHealthBarDivision7[] = 
{
    ANIMCMD_FRAME(7, 20),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd sSpriteAnim_ExpHealthBarDivisionFull[] = 
{
    ANIMCMD_FRAME(8, 20),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd sSpriteAnim_ExpHealthBarNameLeft[] = 
{
    ANIMCMD_FRAME(9, 20),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd sSpriteAnim_ExpHealthBarNameRight[] = 
{
    ANIMCMD_FRAME(10, 20),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd sSpriteAnim_ExpHealthBarEnd[] = 
{
    ANIMCMD_FRAME(11, 20),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd * const sSpriteAnimTable_ExpHealthBars[] =
{
    sSpriteAnim_ExpHealthBarDivisionEmpty,
    sSpriteAnim_ExpHealthBarDivision1,
    sSpriteAnim_ExpHealthBarDivision2,
    sSpriteAnim_ExpHealthBarDivision3,
    sSpriteAnim_ExpHealthBarDivision4,
    sSpriteAnim_ExpHealthBarDivision5,
    sSpriteAnim_ExpHealthBarDivision6,
    sSpriteAnim_ExpHealthBarDivision7,
    sSpriteAnim_ExpHealthBarDivisionFull,
    sSpriteAnim_ExpHealthBarNameLeft,
    sSpriteAnim_ExpHealthBarNameRight,
    sSpriteAnim_ExpHealthBarEnd
};

static const u16 * const sHealthBarPals[] =
{
    sHealthBar_Green_Pal,
    sHealthBar_Yellow_Pal,
    sHealthBar_Red_Pal,
};

// code
static u8 ShowSplitIcon(u32 split)
{
    if (sMonSummaryScreen->splitIconSpriteId == SPRITE_NONE)
        sMonSummaryScreen->splitIconSpriteId = CreateSprite(&sSpriteTemplate_SplitIcons, 100, 65, 0);
    gSprites[sMonSummaryScreen->splitIconSpriteId].invisible = FALSE;
    StartSpriteAnim(&gSprites[sMonSummaryScreen->splitIconSpriteId], split);
    return sMonSummaryScreen->splitIconSpriteId;
}

static void DestroySplitIcon(void)
{
    if (sMonSummaryScreen->splitIconSpriteId != SPRITE_NONE)
        DestroySprite(&gSprites[sMonSummaryScreen->splitIconSpriteId]);
    sMonSummaryScreen->splitIconSpriteId = SPRITE_NONE;
}

void ShowPokemonSummaryScreen(u8 mode, void *mons, u8 monIndex, u8 maxMonIndex, void (*callback)(void))
{
    sMonSummaryScreen = AllocZeroed(sizeof(*sMonSummaryScreen));
    sMonSummaryScreen->mode = mode;
    sMonSummaryScreen->monList.mons = mons;
    sMonSummaryScreen->curMonIndex = monIndex;
    sMonSummaryScreen->maxMonIndex = maxMonIndex;
    sMonSummaryScreen->callback = callback;

    if (mode == PSS_MODE_BOX)
        sMonSummaryScreen->isBoxMon = TRUE;
    else
        sMonSummaryScreen->isBoxMon = FALSE;

    switch (mode)
    {
    case PSS_MODE_NORMAL:
    case PSS_MODE_BOX:
        sMonSummaryScreen->minPageIndex = 0;
        sMonSummaryScreen->maxPageIndex = PSS_PAGE_COUNT - 1;
        break;
    case PSS_MODE_LOCK_MOVES:
        sMonSummaryScreen->minPageIndex = 0;
        sMonSummaryScreen->maxPageIndex = PSS_PAGE_COUNT - 1;
        sMonSummaryScreen->lockMovesFlag = TRUE;
        break;
    case PSS_MODE_SELECT_MOVE:
        sMonSummaryScreen->minPageIndex = PSS_PAGE_BATTLE_MOVES;
        sMonSummaryScreen->maxPageIndex = PSS_PAGE_COUNT - 1;
        sMonSummaryScreen->lockMonFlag = TRUE;
        break;
    }

    sMonSummaryScreen->currPageIndex = sMonSummaryScreen->minPageIndex;
    sMonSummaryScreen->splitIconSpriteId = SPRITE_NONE;
    SummaryScreen_SetAnimDelayTaskId(TASK_NONE);

    if (gMonSpritesGfxPtr == NULL)
        sub_806F2AC(0, 0);

    SetMainCallback2(CB2_InitSummaryScreen);
}

void ShowSelectMovePokemonSummaryScreen(struct Pokemon *mons, u8 monIndex, u8 maxMonIndex, void (*callback)(void), u16 newMove)
{
    ShowPokemonSummaryScreen(PSS_MODE_SELECT_MOVE, mons, monIndex, maxMonIndex, callback);
    sMonSummaryScreen->newMove = newMove;
}

void ShowPokemonSummaryScreenSet40EF(u8 mode, struct BoxPokemon *mons, u8 monIndex, u8 maxMonIndex, void (*callback)(void))
{
    ShowPokemonSummaryScreen(mode, mons, monIndex, maxMonIndex, callback);
    sMonSummaryScreen->unk40EF = TRUE;
}

static void MainCB2(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}

static void VBlank(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void CB2_InitSummaryScreen(void)
{
    while (MenuHelpers_CallLinkSomething() != TRUE && LoadGraphics() != TRUE && MenuHelpers_LinkSomething() != TRUE);
}

static bool8 LoadGraphics(void)
{
    switch (gMain.state)
    {
    case 0:
        SetVBlankHBlankCallbacksToNull();
        ResetVramOamAndBgCntRegs();
        ClearScheduledBgCopiesToVram();
        gMain.state++;
        break;
    case 1:
        ScanlineEffect_Stop();
        gMain.state++;
        break;
    case 2:
        ResetPaletteFade();
        gPaletteFade.bufferTransferDisabled = 1;
        gMain.state++;
        break;
    case 3:
        ResetSpriteData();
        gMain.state++;
        break;
    case 4:
        FreeAllSpritePalettes();
        gMain.state++;
        break;
    case 5:
        InitBGs();
        sMonSummaryScreen->switchCounter = 0;
        gMain.state++;
        break;
    case 6:
        if (DecompressGraphics() != FALSE)
            gMain.state++;
        break;
    case 7:
        ResetWindows();
        gMain.state++;
        break;
    case 8:
        gMain.state++;
        break;
    case 9:
        CopyMonToSummaryStruct(&sMonSummaryScreen->currentMon);
        sMonSummaryScreen->switchCounter = 0;
        gMain.state++;
        break;
    case 10:
        if (ExtractMonDataToSummaryStruct(&sMonSummaryScreen->currentMon) != 0)
            gMain.state++;
        break;
    case 11:
        PrintPSSHeader(PSS_PAGE_INFO);
        gMain.state++;
        break;
    case 12:
        PrintChangeMonPageText(sMonSummaryScreen->currPageIndex);
        gMain.state++;
        break;
    case 13:
        gMain.state++;
        break;
    case 14:
        PutPageWindowTilemaps(sMonSummaryScreen->currPageIndex);
        gMain.state++;
        break;
    case 15:
        ResetSpriteIds();
        CreateMoveTypeIcons();
        sMonSummaryScreen->switchCounter = 0;
        gMain.state++;
        break;
    case 16:
        sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MON] = LoadMonGfxAndSprite(&sMonSummaryScreen->currentMon, &sMonSummaryScreen->switchCounter);
        if (sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MON] != SPRITE_NONE)
        {
            sMonSummaryScreen->switchCounter = 0;
            gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MON]].invisible = sMonSummaryScreen->currPageIndex >= PSS_PAGE_BATTLE_MOVES;
            gMain.state++;
        }
        break;
    case 17:
        CreateCaughtBallSprite(&sMonSummaryScreen->currentMon);
        gMain.state++;
        break;
    case 18:
        CreateSetStatusSprite();
        gMain.state++;
        break;
    case 19:
        gMain.state++;
        break;
    case 20:
        CreateSetShinyStar(&sMonSummaryScreen->currentMon);
        gMain.state++;
        break;
    case 21:
        SetTypeIcons();
        gMain.state++;
        break;
    case 22:
        LoadMonIconPalettes();
        CreateMonIconSprite();
        ToggleSpritesForMovesPage();
        gMain.state++;
        break;
    case 23:
        CreateHealthBarSprites(TAG_HEALTH_BAR, TAG_HEALTH_BAR);
        gMain.state++;
    case 24:
        CreateExpBarSprites(TAG_EXP_BAR, TAG_HEALTH_BAR);
        gMain.state++;
    case 25:
        if (sMonSummaryScreen->mode != PSS_MODE_SELECT_MOVE)
            CreateTask(Task_HandleInput, 0);
        else
            CreateTask(Task_SetHandleReplaceMoveInput, 0);
        gMain.state++;
        break;
    case 26:
        BlendPalettes(PALETTES_ALL, 16, 0);
        gMain.state++;
        break;
    case 27:
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        gPaletteFade.bufferTransferDisabled = 0;
        gMain.state++;
        break;
    default:
        SetVBlankCallback(VBlank);
        SetMainCallback2(MainCB2);
        return TRUE;
    }
    return FALSE;
}

static void InitBGs(void)
{
    ResetBgsAndClearDma3BusyFlags(0);
    InitBgsFromTemplates(0, sBgTemplates, ARRAY_COUNT(sBgTemplates));
    SetBgTilemapBuffer(1, sMonSummaryScreen->bgTilemapBuffers[0][sMonSummaryScreen->currPageIndex]);
    SetBgTilemapBuffer(2, sMonSummaryScreen->bgTilemapBuffers[1][sMonSummaryScreen->currPageIndex]);
    ResetAllBgsCoordinates();
    ScheduleBgCopyTilemapToVram(1);
    ScheduleBgCopyTilemapToVram(2);
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
    SetGpuReg(REG_OFFSET_BLDCNT, 0);
    ShowBg(0);
    //ShowBg(1);
    ShowBg(2);
}

static bool8 DecompressGraphics(void)
{
    switch (sMonSummaryScreen->switchCounter)
    {
    case 0:
        ResetTempTileDataBuffers();
        DecompressAndCopyTileDataToVram(1, &sSummaryScreenBitmap, 0, 0, 0);
        sMonSummaryScreen->switchCounter++;
        break;
    case 1:
        if (FreeTempTileDataBuffersIfPossible() != 1)
        {
            LZDecompressWram(sPageInfo_BgTilemap,     sMonSummaryScreen->bgTilemapBuffers[1][SUMMARY_BG_INFO_PAGE]);
            sMonSummaryScreen->switchCounter++;
        }
        break;
    case 2:
        LZDecompressWram(sPageSkills_BgTilemap,     sMonSummaryScreen->bgTilemapBuffers[1][SUMMARY_BG_SKILLS_PAGE]);
        sMonSummaryScreen->switchCounter++;
        break;
    case 3:
        if (sMonSummaryScreen->mode == PSS_MODE_SELECT_MOVE)
            LZDecompressWram(sPageMoves_BgTilemap_FiveMoves, sMonSummaryScreen->bgTilemapBuffers[1][SUMMARY_BG_BATTLE_MOVES_PAGE]);
        else
            LZDecompressWram(sPageMoves_BgTilemap, sMonSummaryScreen->bgTilemapBuffers[1][SUMMARY_BG_BATTLE_MOVES_PAGE]);
        sMonSummaryScreen->switchCounter++;
        break;
    case 4:
        sMonSummaryScreen->switchCounter++;
        break;
    case 5:
        sMonSummaryScreen->switchCounter++;
        break;
    case 6:
        sMonSummaryScreen->switchCounter++;
        break;
    case 7:
        LZDecompressWram(sPageInfo_BgTilemap,        sMonSummaryScreen->bgTilemapBuffers[1][SUMMARY_BG_EGG_INFO_PAGE]);
        sMonSummaryScreen->switchCounter++;
        break;
    case 8:
        LoadCompressedPalette(sSummaryScreenPalette, 0, 32 * 3);
        LoadCompressedPalette(sSummaryScreenTextPalette, 224, 0x20);
        sMonSummaryScreen->switchCounter++;
        break;
    case 9:
        LoadCompressedSpriteSheet(&gSpriteSheet_MoveTypes);
        sMonSummaryScreen->switchCounter++;
        break;
    case 10:
        LoadCompressedSpriteSheet(&sMoveSelectorSpriteSheet);
        //LoadCompressedSpriteSheet(&sSpriteSheet_PokerusCuredSymbol);
        LoadCompressedSpriteSheet(&sSpriteSheet_ShinyStar);
        sMonSummaryScreen->switchCounter++;
        break;
    case 11:
        LoadCompressedSpriteSheet(&sStatusIconsSpriteSheet);
        sMonSummaryScreen->switchCounter++;
        break;
    case 12:
        LoadCompressedSpritePalette(&sStatusIconsSpritePalette);
        sMonSummaryScreen->switchCounter++;
        break;
    case 13:
        //LoadCompressedSpritePalette(&sMoveSelectorSpritePal);
        LoadCompressedSpritePalette(&sSplitIconsSpritePal);
        sMonSummaryScreen->switchCounter++;
        break;
    case 14:
        LoadCompressedPalette(gMoveTypes_Pal, 0x1D0, 0x60);
        LoadCompressedSpriteSheet(&sSpriteSheet_SplitIcons);
        sMonSummaryScreen->switchCounter = 0;
        return TRUE;
    }
    return FALSE;
}

static void CopyMonToSummaryStruct(struct Pokemon *mon)
{
    if (!sMonSummaryScreen->isBoxMon)
    {
        struct Pokemon *partyMon = sMonSummaryScreen->monList.mons;
        *mon = partyMon[sMonSummaryScreen->curMonIndex];
    }
    else
    {
        struct BoxPokemon *boxMon = sMonSummaryScreen->monList.boxMons;
        BoxMonToMon(&boxMon[sMonSummaryScreen->curMonIndex], mon);
    }
}

static bool8 ExtractMonDataToSummaryStruct(struct Pokemon *mon)
{
    u32 i;
    struct PokeSummary *sum = &sMonSummaryScreen->summary;
    // Spread the data extraction over multiple frames.
    switch (sMonSummaryScreen->switchCounter)
    {
    case 0:
        sum->species = GetMonData(mon, MON_DATA_SPECIES);
        sum->species2 = GetMonData(mon, MON_DATA_SPECIES2);
        sum->exp = GetMonData(mon, MON_DATA_EXP);
        sum->level = GetMonData(mon, MON_DATA_LEVEL);
        sum->abilityNum = GetMonData(mon, MON_DATA_ABILITY_NUM);
        sum->item = GetMonData(mon, MON_DATA_HELD_ITEM);
        sum->pid = GetMonData(mon, MON_DATA_PERSONALITY);
        sum->sanity = GetMonData(mon, MON_DATA_SANITY_IS_BAD_EGG);

        if (sum->sanity)
            sum->isEgg = TRUE;
        else
            sum->isEgg = GetMonData(mon, MON_DATA_IS_EGG);

        break;
    case 1:
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            sum->moves[i] = GetMonData(mon, MON_DATA_MOVE1+i);
            sum->pp[i] = GetMonData(mon, MON_DATA_PP1+i);
        }
        sum->ppBonuses = GetMonData(mon, MON_DATA_PP_BONUSES);
        break;
    case 2:
        if (sMonSummaryScreen->monList.mons == gPlayerParty || sMonSummaryScreen->mode == PSS_MODE_BOX || sMonSummaryScreen->unk40EF == TRUE)
        {
            sum->nature = GetNature(mon);
            sum->currentHP = GetMonData(mon, MON_DATA_HP);
            sum->maxHP = GetMonData(mon, MON_DATA_MAX_HP);
            sum->stat[0] = GetMonData(mon, MON_DATA_ATK);
            sum->stat[1] = GetMonData(mon, MON_DATA_DEF);
            sum->stat[4] = GetMonData(mon, MON_DATA_SPEED);
            sum->stat[3] = GetMonData(mon, MON_DATA_SPATK);
            sum->stat[2] = GetMonData(mon, MON_DATA_SPDEF);
        }
        else
        {
            sum->nature = GetNature(mon);
            sum->currentHP = GetMonData(mon, MON_DATA_HP);
            sum->maxHP = GetMonData(mon, MON_DATA_MAX_HP);
            sum->stat[0] = GetMonData(mon, MON_DATA_ATK2);
            sum->stat[1] = GetMonData(mon, MON_DATA_DEF2);
            sum->stat[4] = GetMonData(mon, MON_DATA_SPEED2);
            sum->stat[3] = GetMonData(mon, MON_DATA_SPATK2);
            sum->stat[2] = GetMonData(mon, MON_DATA_SPDEF2);
        }
        break;
    case 3:
        GetMonData(mon, MON_DATA_OT_NAME, sum->OTName);
        ConvertInternationalString(sum->OTName, GetMonData(mon, MON_DATA_LANGUAGE));
        sum->ailment = GetMonAilment(mon);
        sum->OTGender = GetMonData(mon, MON_DATA_OT_GENDER);
        sum->OTID = GetMonData(mon, MON_DATA_OT_ID);
        sum->metLocation = GetMonData(mon, MON_DATA_MET_LOCATION);
        sum->metLevel = GetMonData(mon, MON_DATA_MET_LEVEL);
        sum->metGame = GetMonData(mon, MON_DATA_MET_GAME);
        sum->friendship = GetMonData(mon, MON_DATA_FRIENDSHIP);
        break;
    default:
        sum->ribbonCount = GetMonData(mon, MON_DATA_RIBBON_COUNT);
        return TRUE;
    }
    sMonSummaryScreen->switchCounter++;
    return FALSE;
}

static void FreeSummaryScreen(void)
{
    FreeAllWindowBuffers();
    Free(sMonSummaryScreen);
}

static void BeginCloseSummaryScreen(u8 taskId)
{
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    gTasks[taskId].func = CloseSummaryScreen;
}

static void CloseSummaryScreen(u8 taskId)
{
    if (MenuHelpers_CallLinkSomething() != TRUE && !gPaletteFade.active)
    {
        SetMainCallback2(sMonSummaryScreen->callback);
        gLastViewedMonIndex = sMonSummaryScreen->curMonIndex;
        SummaryScreen_DestroyAnimDelayTask();
        DestroyHealthBarSprites();
        DestroyExpBarSprites();
        ResetSpriteData();
        FreeAllSpritePalettes();
        StopCryAndClearCrySongs();
        m4aMPlayVolumeControl(&gMPlayInfo_BGM, 0xFFFF, 0x100);
        if (gMonSpritesGfxPtr == NULL)
            sub_806F47C(0);
        FreeSummaryScreen();
        DestroyTask(taskId);
    }
}

static void Task_HandleInput(u8 taskId)
{
    if (MenuHelpers_CallLinkSomething() != TRUE && !gPaletteFade.active)
    {
        if (JOY_NEW(DPAD_UP))
        {
            ChangeSummaryPokemon(taskId, -1);
        }
        else if (JOY_NEW(DPAD_DOWN))
        {
            ChangeSummaryPokemon(taskId, 1);
        }
        else if ((JOY_NEW(DPAD_LEFT)) || GetLRKeysPressed() == MENU_L_PRESSED)
        {
            ChangePage(taskId, -1);
        }
        else if ((JOY_NEW(DPAD_RIGHT)) || GetLRKeysPressed() == MENU_R_PRESSED)
        {
            ChangePage(taskId, 1);
        }
        else if (JOY_NEW(A_BUTTON))
        {
            if (sMonSummaryScreen->currPageIndex != PSS_PAGE_SKILLS)
            {
                if (sMonSummaryScreen->currPageIndex == PSS_PAGE_INFO)
                {
                    StopPokemonAnimations();
                    PlaySE(SE_SELECT);
                    BeginCloseSummaryScreen(taskId);
                }
                else
                {
                    PlaySE(SE_SELECT);
                    SwitchToMoveSelection(taskId);
                }
            }
        }
        else if (JOY_NEW(B_BUTTON))
        {
            StopPokemonAnimations();
            PlaySE(SE_SELECT);
            BeginCloseSummaryScreen(taskId);
        }
    }
}

static void ChangeSummaryPokemon(u8 taskId, s8 delta)
{
    s8 monId;

    if (!sMonSummaryScreen->lockMonFlag)
    {
        if (sMonSummaryScreen->isBoxMon == TRUE)
        {
            if (sMonSummaryScreen->currPageIndex != PSS_PAGE_INFO)
            {
                if (delta == 1)
                    delta = 0;
                else
                    delta = 2;
            }
            else
            {
                if (delta == 1)
                    delta = 1;
                else
                    delta = 3;
            }
            monId = AdvanceStorageMonIndex(sMonSummaryScreen->monList.boxMons, sMonSummaryScreen->curMonIndex, sMonSummaryScreen->maxMonIndex, delta);
        }
        else if (IsMultiBattle() == TRUE)
        {
            monId = AdvanceMultiBattleMonIndex(delta);
        }
        else
        {
            monId = AdvanceMonIndex(delta);
        }

        if (monId != -1)
        {
            PlaySE(SE_SELECT);
            sMonSummaryScreen->curMonIndex = monId;
            gTasks[taskId].data[0] = 0;
            gTasks[taskId].func = Task_ChangeSummaryMon;
        }
    }
}

static void Task_ChangeSummaryMon(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    switch (data[0])
    {
    case 0:
        StopCryAndClearCrySongs();
        break;
    case 1:
        SummaryScreen_DestroyAnimDelayTask();
        DestroySpriteAndFreeResources(&gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MON]]);
        break;
    case 2:
        DestroySpriteAndFreeResources(&gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_BALL]]);
        break;
    case 3:
        CopyMonToSummaryStruct(&sMonSummaryScreen->currentMon);
        sMonSummaryScreen->switchCounter = 0;
        break;
    case 4:
        if (ExtractMonDataToSummaryStruct(&sMonSummaryScreen->currentMon) == FALSE)
            return;
        break;
    case 5:
        CreateCaughtBallSprite(&sMonSummaryScreen->currentMon);
        break;
    case 6:
        if (sMonSummaryScreen->summary.ailment != AILMENT_NONE)
            CreateSetStatusSprite();
        else
            SetSpriteInvisibility(SPRITE_ARR_ID_STATUS, TRUE);
        break;
    case 7:
        break;
    case 8:
        CreateSetShinyStar(&sMonSummaryScreen->currentMon);
        break;
    case 9:
        sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MON] = LoadMonGfxAndSprite(&sMonSummaryScreen->currentMon, &data[1]);
        if (sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MON] == SPRITE_NONE)
            return;
        gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MON]].data[2] = 1;
        gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MON]].invisible = (sMonSummaryScreen->currPageIndex >= PSS_PAGE_BATTLE_MOVES);
        data[1] = 0;
        break;
    case 10:
        SetTypeIcons();
        break;
    case 11:
        FreeAndDestroyMonIconSprite(&gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MON_ICON]]);
        CreateMonIconSprite();
        ToggleSpritesForMovesPage();
        break;
    case 12:
        ConfigureHealthBarSprites();
        break;
    case 13:
        ConfigureExpBarSprites();
        break;
    case 14:
        PrintPSSHeader(sMonSummaryScreen->currPageIndex);
        break;
    case 15:
        PrintChangeMonPageText(sMonSummaryScreen->currPageIndex);
        LimitEggSummaryPageDisplay();
        break;
    case 16:
        gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MON]].data[2] = 0;
        break;
    default:
        if (MenuHelpers_CallLinkSomething() == 0)
        {
            data[0] = 0;
            gTasks[taskId].func = Task_HandleInput;
        }
        return;
    }
    data[0]++;
}

static s8 AdvanceMonIndex(s8 delta)
{
    struct Pokemon *mon = sMonSummaryScreen->monList.mons;

    if (sMonSummaryScreen->currPageIndex == PSS_PAGE_INFO)
    {
        if (delta == -1 && sMonSummaryScreen->curMonIndex == 0)
            return -1;
        else if (delta == 1 && sMonSummaryScreen->curMonIndex >= sMonSummaryScreen->maxMonIndex)
            return -1;
        else
            return sMonSummaryScreen->curMonIndex + delta;
    }
    else
    {
        s8 index = sMonSummaryScreen->curMonIndex;

        do
        {
            index += delta;
            if (index < 0 || index > sMonSummaryScreen->maxMonIndex)
                return -1;
        } while (GetMonData(&mon[index], MON_DATA_IS_EGG));
        return index;
    }
}

static s8 AdvanceMultiBattleMonIndex(s8 delta)
{
    struct Pokemon *mons = sMonSummaryScreen->monList.mons;
    s8 index, arrId = 0;
    u8 i;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (sMultiBattleOrder[i] == sMonSummaryScreen->curMonIndex)
        {
            arrId = i;
            break;
        }
    }

    while (TRUE)
    {
        const s8 *order = sMultiBattleOrder;

        arrId += delta;
        if (arrId < 0 || arrId >= PARTY_SIZE)
            return -1;
        index = order[arrId];
        if (IsValidToViewInMulti(&mons[index]) == TRUE)
            return index;
    }
}

static bool8 IsValidToViewInMulti(struct Pokemon* mon)
{
    if (GetMonData(mon, MON_DATA_SPECIES) == SPECIES_NONE)
        return FALSE;
    else if (sMonSummaryScreen->curMonIndex != 0 || !GetMonData(mon, MON_DATA_IS_EGG))
        return TRUE;
    else
        return FALSE;
}

static void ChangePage(u8 taskId, s8 delta)
{
    struct PokeSummary *summary = &sMonSummaryScreen->summary;
    s16 *data = gTasks[taskId].data;

    if (summary->isEgg)
        return;
    else if (delta == -1 && sMonSummaryScreen->currPageIndex == sMonSummaryScreen->minPageIndex)
        return;
    else if (delta == 1 && sMonSummaryScreen->currPageIndex == sMonSummaryScreen->maxPageIndex)
        return;
    
    if ((sMonSummaryScreen->currPageIndex == PSS_PAGE_SKILLS && delta == 1)
      || (sMonSummaryScreen->currPageIndex == PSS_PAGE_BATTLE_MOVES && delta == -1))
    {
        FillWindowPixelBuffer(WIN_NAME, PIXEL_FILL(0));
        CopyWindowToVram(WIN_NAME, 2);
    }
      
    PlaySE(SE_SELECT);
    ClearPageWindowTilemaps(sMonSummaryScreen->currPageIndex);
    sMonSummaryScreen->currPageIndex += delta;
    data[0] = 0;
    if (delta == 1)
        SetTaskFuncWithFollowupFunc(taskId, PssScrollRight, gTasks[taskId].func);
    else
        SetTaskFuncWithFollowupFunc(taskId, PssScrollLeft, gTasks[taskId].func);
    
    CreateChangePageTask(sMonSummaryScreen->currPageIndex);
    HideTypeSprites();
    ToggleSpritesForMovesPage();
    HideMoveSelectorSprites();
    SetHealthBarSprites(sMonSummaryScreen->currPageIndex != PSS_PAGE_SKILLS);
    SetExpBarSprites(sMonSummaryScreen->currPageIndex != PSS_PAGE_SKILLS);
}

static void PssScrollRight(u8 taskId) // Scroll right
{
    s16 *data = gTasks[taskId].data;
    if (data[0] == 0)
    {
        ScheduleBgCopyTilemapToVram(1);
        ScheduleBgCopyTilemapToVram(2);
        SetBgTilemapBuffer(1, sMonSummaryScreen->bgTilemapBuffers[0][sMonSummaryScreen->currPageIndex]);
        SetBgTilemapBuffer(2, sMonSummaryScreen->bgTilemapBuffers[1][sMonSummaryScreen->currPageIndex]);
        //ShowBg(1);
        ShowBg(2);
    }
    data[0] += 32;
    if (data[0] > 0xFF)
        gTasks[taskId].func = PssScrollRightEnd;
}

static void PssScrollRightEnd(u8 taskId) // display right
{
    s16 *data = gTasks[taskId].data;
    sMonSummaryScreen->bgDisplayOrder ^= 1;
    data[1] = 0;
    data[0] = 0;
    
    PutPageWindowTilemaps(sMonSummaryScreen->currPageIndex);
    SetTypeIcons();
    SwitchTaskToFollowupFunc(taskId);
}

static void PssScrollLeft(u8 taskId) // Scroll left
{
    s16 *data = gTasks[taskId].data;
    if (data[0] == 0)
    {
        ScheduleBgCopyTilemapToVram(1);
        ScheduleBgCopyTilemapToVram(2);
        SetBgTilemapBuffer(1, sMonSummaryScreen->bgTilemapBuffers[0][sMonSummaryScreen->currPageIndex]);
        SetBgTilemapBuffer(2, sMonSummaryScreen->bgTilemapBuffers[1][sMonSummaryScreen->currPageIndex]);
        ShowBg(2);
    }
    data[0] += 32;
    if (data[0] > 0xFF)
        gTasks[taskId].func = PssScrollLeftEnd;
}

static void PssScrollLeftEnd(u8 taskId) // display left
{
    s16 *data = gTasks[taskId].data;
    sMonSummaryScreen->bgDisplayOrder ^= 1;
    data[1] = 0;
    data[0] = 0;
    
    PutPageWindowTilemaps(sMonSummaryScreen->currPageIndex);
    SetTypeIcons();
    SwitchTaskToFollowupFunc(taskId);
}

static void SwitchToMoveSelection(u8 taskId)
{
    u16 move;

    sMonSummaryScreen->firstMoveIndex = 0;
    move = sMonSummaryScreen->summary.moves[sMonSummaryScreen->firstMoveIndex];
    PrintMoveDetails(move);
    SwapMonSpriteIconSpriteVisibility(TRUE);
    PrintNewMoveDetailsOrCancelText();
    SetNewMoveTypeIcon();

    ShowBg(2);
    CreateMoveSelectorSprites(SPRITE_ARR_ID_MOVE_SELECTOR1);
    gTasks[taskId].func = Task_HandleInput_MoveSelect;
}

static void Task_HandleInput_MoveSelect(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    if (MenuHelpers_CallLinkSomething() != 1)
    {
        if (JOY_NEW(DPAD_UP))
        {
            data[0] = 4;
            ChangeSelectedMove(data, -1, &sMonSummaryScreen->firstMoveIndex);
        }
        else if (JOY_NEW(DPAD_DOWN))
        {
            data[0] = 4;
            ChangeSelectedMove(data, 1, &sMonSummaryScreen->firstMoveIndex);
        }
        else if (JOY_NEW(A_BUTTON))
        {
            if (sMonSummaryScreen->lockMovesFlag == TRUE
             || (sMonSummaryScreen->newMove == MOVE_NONE && sMonSummaryScreen->firstMoveIndex == MAX_MON_MOVES))
            {
                PlaySE(SE_SELECT);
                CloseMoveSelectMode(taskId);
            }
            else if (HasMoreThanOneMove() == TRUE)
            {
                PlaySE(SE_SELECT);
                SwitchToMovePositionSwitchMode(taskId);
            }
            else
            {
                PlaySE(SE_FAILURE);
            }
        }
        else if (JOY_NEW(B_BUTTON))
        {
            PlaySE(SE_SELECT);
            CloseMoveSelectMode(taskId);
        }
    }
}

static bool8 HasMoreThanOneMove(void)
{
    u8 i;
    for (i = 1; i < MAX_MON_MOVES; i++)
    {
        if (sMonSummaryScreen->summary.moves[i] != 0)
            return TRUE;
    }
    return FALSE;
}

static void ChangeSelectedMove(s16 *taskData, s8 direction, u8 *moveIndexPtr)
{
    s8 i, newMoveIndex;
    u16 move;

    PlaySE(SE_SELECT);
    newMoveIndex = *moveIndexPtr;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        newMoveIndex += direction;
        if (newMoveIndex > taskData[0])
            newMoveIndex = 0;
        else if (newMoveIndex < 0)
            newMoveIndex = taskData[0];

        if (newMoveIndex == MAX_MON_MOVES)
        {
            move = sMonSummaryScreen->newMove;
            break;
        }
        move = sMonSummaryScreen->summary.moves[newMoveIndex];
        if (move != 0)
            break;
    }
    ScheduleBgCopyTilemapToVram(0);
    ScheduleBgCopyTilemapToVram(1);
    ScheduleBgCopyTilemapToVram(2);
    PrintMoveDetails(move);

    if (*moveIndexPtr != MAX_MON_MOVES
        && newMoveIndex == MAX_MON_MOVES
        && sMonSummaryScreen->newMove == MOVE_NONE)
        DestroySplitIcon();

    *moveIndexPtr = newMoveIndex;
    // Get rid of the 'flicker' effect(while idle) when scrolling.
    if (moveIndexPtr == &sMonSummaryScreen->firstMoveIndex)
        KeepMoveSelectorVisible(SPRITE_ARR_ID_MOVE_SELECTOR1);
    else
        KeepMoveSelectorVisible(SPRITE_ARR_ID_MOVE_SELECTOR2);
}

static void ClearCancelText(void)
{
    u8 windowId = AddWindowFromTemplateList(sPageMovesTemplate, WIN_MOVE_NAMES);
    FillWindowPixelRect(windowId, PIXEL_FILL(0), 0, 4 * MOVE_SLOT_DELTA + 5, 170, 16);
    CopyWindowToVram(windowId, 2);
}

static void CloseMoveSelectMode(u8 taskId)
{
    DestroyMoveSelectorSprites(SPRITE_ARR_ID_MOVE_SELECTOR1);
    PrintMoveDetails(0);
    ClearCancelText();
    if (sMonSummaryScreen->firstMoveIndex != MAX_MON_MOVES)
        DestroySplitIcon();
    
    ScheduleBgCopyTilemapToVram(0);
    //ScheduleBgCopyTilemapToVram(1);
    ScheduleBgCopyTilemapToVram(2);

    SetBgTilemapBuffer(1, sMonSummaryScreen->bgTilemapBuffers[0][SUMMARY_BG_BATTLE_MOVES_PAGE]);
    SetBgTilemapBuffer(2, sMonSummaryScreen->bgTilemapBuffers[1][SUMMARY_BG_BATTLE_MOVES_PAGE]);
    ShowBg(2);
    gTasks[taskId].func = Task_HandleInput;
}

static void SwitchToMovePositionSwitchMode(u8 taskId)
{
    sMonSummaryScreen->secondMoveIndex = sMonSummaryScreen->firstMoveIndex;
    SetMainMoveSelectorColor(1);
    CreateMoveSelectorSprites(SPRITE_ARR_ID_MOVE_SELECTOR2);
    gTasks[taskId].func = Task_HandleInput_MovePositionSwitch;
}

static void Task_HandleInput_MovePositionSwitch(u8 taskId)
{
    s16* data = gTasks[taskId].data;

    if (MenuHelpers_CallLinkSomething() != TRUE)
    {
        if (JOY_NEW(DPAD_UP))
        {
            data[0] = 3;
            ChangeSelectedMove(&data[0], -1, &sMonSummaryScreen->secondMoveIndex);
        }
        else if (JOY_NEW(DPAD_DOWN))
        {
            data[0] = 3;
            ChangeSelectedMove(&data[0], 1, &sMonSummaryScreen->secondMoveIndex);
        }
        else if (JOY_NEW(A_BUTTON))
        {
            if (sMonSummaryScreen->firstMoveIndex == sMonSummaryScreen->secondMoveIndex)
                ExitMovePositionSwitchMode(taskId, FALSE);
            else
                ExitMovePositionSwitchMode(taskId, TRUE);
        }
        else if (JOY_NEW(B_BUTTON))
        {
            ExitMovePositionSwitchMode(taskId, FALSE);
        }
    }
}

static void ExitMovePositionSwitchMode(u8 taskId, bool8 swapMoves)
{
    u16 move;

    PlaySE(SE_SELECT);
    SetMainMoveSelectorColor(0);
    DestroyMoveSelectorSprites(SPRITE_ARR_ID_MOVE_SELECTOR2);

    if (swapMoves == TRUE)
    {
        if (!sMonSummaryScreen->isBoxMon)
        {
            struct Pokemon *mon = sMonSummaryScreen->monList.mons;
            SwapMonMoves(&mon[sMonSummaryScreen->curMonIndex], sMonSummaryScreen->firstMoveIndex, sMonSummaryScreen->secondMoveIndex);
        }
        else
        {
            struct BoxPokemon *boxMon = sMonSummaryScreen->monList.boxMons;
            SwapBoxMonMoves(&boxMon[sMonSummaryScreen->curMonIndex], sMonSummaryScreen->firstMoveIndex, sMonSummaryScreen->secondMoveIndex);
        }
        CopyMonToSummaryStruct(&sMonSummaryScreen->currentMon);
        SwapMovesNamesPP(sMonSummaryScreen->firstMoveIndex, sMonSummaryScreen->secondMoveIndex);
        SwapMovesTypeSprites(sMonSummaryScreen->firstMoveIndex, sMonSummaryScreen->secondMoveIndex);
        sMonSummaryScreen->firstMoveIndex = sMonSummaryScreen->secondMoveIndex;
    }

    move = sMonSummaryScreen->summary.moves[sMonSummaryScreen->firstMoveIndex];
    PrintMoveDetails(move);
    ScheduleBgCopyTilemapToVram(1);
    ScheduleBgCopyTilemapToVram(2);
    gTasks[taskId].func = Task_HandleInput_MoveSelect;
}

static void SwapMonMoves(struct Pokemon *mon, u8 moveIndex1, u8 moveIndex2)
{
    struct PokeSummary* summary = &sMonSummaryScreen->summary;

    u16 move1 = summary->moves[moveIndex1];
    u16 move2 = summary->moves[moveIndex2];
    u8 move1pp = summary->pp[moveIndex1];
    u8 move2pp = summary->pp[moveIndex2];
    u8 ppBonuses = summary->ppBonuses;

    // Calculate PP bonuses
    u8 ppUpMask1 = gPPUpGetMask[moveIndex1];
    u8 ppBonusMove1 = (ppBonuses & ppUpMask1) >> (moveIndex1 * 2);
    u8 ppUpMask2 = gPPUpGetMask[moveIndex2];
    u8 ppBonusMove2 = (ppBonuses & ppUpMask2) >> (moveIndex2 * 2);
    ppBonuses &= ~ppUpMask1;
    ppBonuses &= ~ppUpMask2;
    ppBonuses |= (ppBonusMove1 << (moveIndex2 * 2)) + (ppBonusMove2 << (moveIndex1 * 2));

    // Swap the moves
    SetMonData(mon, MON_DATA_MOVE1 + moveIndex1, &move2);
    SetMonData(mon, MON_DATA_MOVE1 + moveIndex2, &move1);
    SetMonData(mon, MON_DATA_PP1 + moveIndex1, &move2pp);
    SetMonData(mon, MON_DATA_PP1 + moveIndex2, &move1pp);
    SetMonData(mon, MON_DATA_PP_BONUSES, &ppBonuses);

    summary->moves[moveIndex1] = move2;
    summary->moves[moveIndex2] = move1;

    summary->pp[moveIndex1] = move2pp;
    summary->pp[moveIndex2] = move1pp;

    summary->ppBonuses = ppBonuses;
}

static void SwapBoxMonMoves(struct BoxPokemon *mon, u8 moveIndex1, u8 moveIndex2)
{
    struct PokeSummary* summary = &sMonSummaryScreen->summary;

    u16 move1 = summary->moves[moveIndex1];
    u16 move2 = summary->moves[moveIndex2];
    u8 move1pp = summary->pp[moveIndex1];
    u8 move2pp = summary->pp[moveIndex2];
    u8 ppBonuses = summary->ppBonuses;

    // Calculate PP bonuses
    u8 ppUpMask1 = gPPUpGetMask[moveIndex1];
    u8 ppBonusMove1 = (ppBonuses & ppUpMask1) >> (moveIndex1 * 2);
    u8 ppUpMask2 = gPPUpGetMask[moveIndex2];
    u8 ppBonusMove2 = (ppBonuses & ppUpMask2) >> (moveIndex2 * 2);
    ppBonuses &= ~ppUpMask1;
    ppBonuses &= ~ppUpMask2;
    ppBonuses |= (ppBonusMove1 << (moveIndex2 * 2)) + (ppBonusMove2 << (moveIndex1 * 2));

    // Swap the moves
    SetBoxMonData(mon, MON_DATA_MOVE1 + moveIndex1, &move2);
    SetBoxMonData(mon, MON_DATA_MOVE1 + moveIndex2, &move1);
    SetBoxMonData(mon, MON_DATA_PP1 + moveIndex1, &move2pp);
    SetBoxMonData(mon, MON_DATA_PP1 + moveIndex2, &move1pp);
    SetBoxMonData(mon, MON_DATA_PP_BONUSES, &ppBonuses);

    summary->moves[moveIndex1] = move2;
    summary->moves[moveIndex2] = move1;

    summary->pp[moveIndex1] = move2pp;
    summary->pp[moveIndex2] = move1pp;

    summary->ppBonuses = ppBonuses;
}

static void Task_SetHandleReplaceMoveInput(u8 taskId)
{
    SetNewMoveTypeIcon();
    CreateMoveSelectorSprites(SPRITE_ARR_ID_MOVE_SELECTOR1);
    gTasks[taskId].func = Task_HandleReplaceMoveInput;
}

static void Task_HandleReplaceMoveInput(u8 taskId)
{
    s16* data = gTasks[taskId].data;

    if (MenuHelpers_CallLinkSomething() != TRUE)
    {
        if (gPaletteFade.active != TRUE)
        {
            if (JOY_NEW(DPAD_UP))
            {
                data[0] = 4;
                ChangeSelectedMove(data, -1, &sMonSummaryScreen->firstMoveIndex);
            }
            else if (JOY_NEW(DPAD_DOWN))
            {
                data[0] = 4;
                ChangeSelectedMove(data, 1, &sMonSummaryScreen->firstMoveIndex);
            }
            else if (JOY_NEW(DPAD_LEFT) || GetLRKeysPressed() == MENU_L_PRESSED)
            {
                ChangePage(taskId, -1);
            }
            else if (JOY_NEW(DPAD_RIGHT) || GetLRKeysPressed() == MENU_R_PRESSED)
            {
                ChangePage(taskId, 1);
            }
            else if (JOY_NEW(A_BUTTON))
            {
                if (CanReplaceMove() == TRUE)
                {
                    StopPokemonAnimations();
                    PlaySE(SE_SELECT);
                    sMoveSlotToReplace = sMonSummaryScreen->firstMoveIndex;
                    gSpecialVar_0x8005 = sMoveSlotToReplace;
                    BeginCloseSummaryScreen(taskId);
                }
                else
                {
                    PlaySE(SE_FAILURE);
                    ShowCantForgetHMsWindow(taskId);
                }
            }
            else if (JOY_NEW(B_BUTTON))
            {
                StopPokemonAnimations();
                PlaySE(SE_SELECT);
                sMoveSlotToReplace = MAX_MON_MOVES;
                gSpecialVar_0x8005 = MAX_MON_MOVES;
                BeginCloseSummaryScreen(taskId);
            }
        }
    }
}

static bool8 CanReplaceMove(void)
{
    if (sMonSummaryScreen->firstMoveIndex == MAX_MON_MOVES
        || sMonSummaryScreen->newMove == MOVE_NONE
        || IsMoveHm(sMonSummaryScreen->summary.moves[sMonSummaryScreen->firstMoveIndex]) != TRUE)
        return TRUE;
    else
        return FALSE;
}

static void ShowCantForgetHMsWindow(u8 taskId)
{
    gSprites[sMonSummaryScreen->splitIconSpriteId].invisible = TRUE;
    ScheduleBgCopyTilemapToVram(0);
    PrintHMMovesCantBeForgotten();
    gTasks[taskId].func = Task_HandleInputCantForgetHMsMoves;
}

// This redraws the power/accuracy window when the player scrolls out of the "HM Moves can't be forgotten" message
static void Task_HandleInputCantForgetHMsMoves(u8 taskId)
{
    s16* data = gTasks[taskId].data;
    u16 move;

    if (gMain.newKeys & DPAD_UP)
    {
        data[1] = 1;
        data[0] = 4;
        ChangeSelectedMove(&data[0], -1, &sMonSummaryScreen->firstMoveIndex);
        data[1] = 0;
        gTasks[taskId].func = Task_HandleReplaceMoveInput;
    }
    else if (gMain.newKeys & DPAD_DOWN)
    {
        data[1] = 1;
        data[0] = 4;
        ChangeSelectedMove(&data[0], 1, &sMonSummaryScreen->firstMoveIndex);
        data[1] = 0;
        gTasks[taskId].func = Task_HandleReplaceMoveInput;
    }
    else if (gMain.newKeys & DPAD_RIGHT || GetLRKeysPressed() == MENU_R_PRESSED)
    {
        move = sMonSummaryScreen->summary.moves[sMonSummaryScreen->firstMoveIndex];
        gTasks[taskId].func = Task_HandleReplaceMoveInput;
        ChangePage(taskId, 1);
    }
    else if (gMain.newKeys & (A_BUTTON | B_BUTTON))
    {
        move = sMonSummaryScreen->summary.moves[sMonSummaryScreen->firstMoveIndex];
        PrintMoveDetails(move);
        ScheduleBgCopyTilemapToVram(0);
        gTasks[taskId].func = Task_HandleReplaceMoveInput;
    }
}

u8 GetMoveSlotToReplace(void)
{
    return sMoveSlotToReplace;
}

static void TilemapFiveMovesDisplay(void)
{
    u16 *dst;
    u16 x, y, id;
    dst = sMonSummaryScreen->bgTilemapBuffers[0][SUMMARY_BG_BATTLE_MOVES_DETAILS_PAGE];
    
    id = 384;
    for (y = 0; y < 3; y++)
    {
        for (x = 0; x < 21; x++)
        {
            dst[id + (32*y) + x] = sPageMoves_BgTilemap_FiveMoves[21*y + x];
        }
    }
}

static void CreateHealthBarSprites(u16 tileTag, u16 palTag)
{
    u8 i;
    u8 spriteId;
    void * gfxBufferPtr;
    u32 curHp;
    u32 maxHp;
    u8 hpBarPalTagOffset = 0;
    
    sHealthBar = AllocZeroed(sizeof(struct HealthBar));
    gfxBufferPtr = AllocZeroed(0x20 * 12);
    LZ77UnCompWram(sHealthBar_Gfx, gfxBufferPtr);
    
    curHp = sMonSummaryScreen->summary.currentHP;
    maxHp = sMonSummaryScreen->summary.maxHP;
    
    if (maxHp / 4 > curHp)
        hpBarPalTagOffset = 2;
    else if (maxHp / 2 > curHp)
        hpBarPalTagOffset = 1;
    
    if (gfxBufferPtr != NULL)
    {
        struct SpriteSheet sheet = {
            .data = gfxBufferPtr,
            .size = 0x20 * 12,
            .tag = tileTag
        };
        
        struct SpritePalette greenPal = {.data = sHealthBarPals[0], .tag = palTag};
        struct SpritePalette yellowPal = {.data = sHealthBarPals[1], .tag = palTag + 1};
        struct SpritePalette redPal = {.data = sHealthBarPals[2], .tag = palTag + 2};
        
        LoadSpriteSheet(&sheet);
        LoadSpritePalette(&greenPal);
        LoadSpritePalette(&yellowPal);
        LoadSpritePalette(&redPal);
    }
    
    for (i = 0; i < HP_BAR_SPRITES_COUNT; i++)
    {
        struct SpriteTemplate template = {
            .tileTag = tileTag,
            .paletteTag = palTag + hpBarPalTagOffset,
            .oam = &sOamData_ExpHealthBars,
            .anims = sSpriteAnimTable_ExpHealthBars,
            .images = NULL,
            .affineAnims = gDummySpriteAffineAnimTable,
            .callback = SpriteCallbackDummy,
        };
        
        sHealthBar->spritePositions[i] = i * 8 + 172;
        spriteId = CreateSprite(&template, sHealthBar->spritePositions[i], 36, 0);
        sHealthBar->sprites[i] = &gSprites[spriteId];
        sHealthBar->sprites[i]->invisible = FALSE;
        sHealthBar->sprites[i]->oam.priority = 1;
        sHealthBar->tileTag = tileTag;
        sHealthBar->palTag = palTag;
        StartSpriteAnim(sHealthBar->sprites[i], 8);
    }
    
    ConfigureHealthBarSprites();
    SetHealthBarSprites(1);
    
    FREE_AND_SET_NULL(gfxBufferPtr);
}

static void ConfigureHealthBarSprites(void)
{
    u8 numWholeHpBarTiles = 0;
    u8 i;
    u8 animNum;
    u8 two = 2;
    u8 hpBarPalOffset = 0;
    u32 curHp;
    u32 maxHp;
    s64 v0;
    s64 v1;
    
    struct PokeSummary *summary = &sMonSummaryScreen->summary;
    
    if (summary->isEgg)
        return;
        
    curHp = sMonSummaryScreen->summary.currentHP;
    maxHp = sMonSummaryScreen->summary.maxHP;
    
    if (maxHp / 5 >= curHp)
        hpBarPalOffset = 2;
    else if (maxHp / 2 >= curHp)
        hpBarPalOffset = 1;
    
    switch (GetHPBarLevel(curHp, maxHp))
    {
    case 3:
    default:
        hpBarPalOffset = 0;
        break;
    case 2:
        hpBarPalOffset = 1;
        break;
    case 1:
        hpBarPalOffset = 2;
        break;
    }
    
    for (i = 0; i < HP_BAR_SPRITES_COUNT; i++)
        sHealthBar->sprites[i]->oam.paletteNum = IndexOfSpritePaletteTag(TAG_HEALTH_BAR) + hpBarPalOffset;
    
    if (curHp == maxHp)
    {    
        for (i = two; i < 8; i++)
            StartSpriteAnim(sHealthBar->sprites[i], 8);
    }
    else
    {
        v0 = (maxHp << 2) / 6;
        v1 = (curHp << 2);

        while (TRUE)
        {
            if (v1 <= v0)
                break;
            v1 -= v0;
            numWholeHpBarTiles++;
        }
        
        numWholeHpBarTiles += two;
        
        for (i = two; i < numWholeHpBarTiles; i++)
            StartSpriteAnim(sHealthBar->sprites[i], 8);
        
        animNum = (v1 * 6) / v0;
        StartSpriteAnim(sHealthBar->sprites[numWholeHpBarTiles], animNum);
        
        for (i = numWholeHpBarTiles + 1; i < 8; i++)
            StartSpriteAnim(sHealthBar->sprites[i], 0);
    }
    
    StartSpriteAnim(sHealthBar->sprites[0], 9);
    StartSpriteAnim(sHealthBar->sprites[1], 10);
    StartSpriteAnim(sHealthBar->sprites[8], 11);
}

static void DestroyHealthBarSprites(void)
{
    u8 i;
    
    for (i = 0; i < HP_BAR_SPRITES_COUNT; i++)
        if (sHealthBar->sprites[i] != NULL)
            DestroySpriteAndFreeResources(sHealthBar->sprites[i]);
    
    FREE_AND_SET_NULL(sHealthBar);
}

static void SetHealthBarSprites(u8 invisible)
{
    u8 i;
    
    for (i = 0; i < HP_BAR_SPRITES_COUNT; i++)
        sHealthBar->sprites[i]->invisible = invisible;
}

static void CreateExpBarSprites(u16 tileTag, u16 palTag)
{
    u8 i;
    u8 spriteId;
    void * gfxBufferPtr;

    sExpBar = AllocZeroed(sizeof(struct ExpBar));
    gfxBufferPtr = AllocZeroed(0x20 * 12);

    LZ77UnCompWram(sExpBar_Gfx, gfxBufferPtr);
    if (gfxBufferPtr != NULL)
    {
        struct SpriteSheet sheet = {
            .data = gfxBufferPtr,
            .size = 0x20 * 12,
            .tag = tileTag
        };

        struct SpritePalette palette = {.data = sHealthBar_Green_Pal, .tag = palTag};
        LoadSpriteSheet(&sheet);
        LoadSpritePalette(&palette);
    }

    for (i = 0; i < EXP_BAR_SPRITES_COUNT; i++)
    {
        struct SpriteTemplate template = {
            .tileTag = tileTag,
            .paletteTag = palTag,
            .oam = &sOamData_ExpHealthBars,
            .anims = sSpriteAnimTable_ExpHealthBars,
            .images = NULL,
            .affineAnims = gDummySpriteAffineAnimTable,
            .callback = SpriteCallbackDummy,
        };

        sExpBar->spritePositions[i] = i * 8 + 156;
        spriteId = CreateSprite(&template, sExpBar->spritePositions[i], 132, 0);
        sExpBar->sprites[i] = &gSprites[spriteId];
        sExpBar->sprites[i]->oam.priority = 1;
        sExpBar->tileTag = tileTag;
        sExpBar->palTag = palTag;
    }

    ConfigureExpBarSprites();
    SetExpBarSprites(1);

    FREE_AND_SET_NULL(gfxBufferPtr);
}

static void ConfigureExpBarSprites(void)
{
    u8 numWholeExpBarTiles = 0;
    u8 i;
    u8 level;
    u32 exp;
    u32 totalExpToNextLevel;
    u32 curExpToNextLevel;
    u16 species;
    s64 v0;
    s64 v1;
    u8 animNum;
    u8 two = 2;
    
    struct PokeSummary *summary = &sMonSummaryScreen->summary;
    
    if (summary->isEgg)
        return;
    
    exp = GetMonData(&sMonSummaryScreen->currentMon, MON_DATA_EXP);
    level = GetMonData(&sMonSummaryScreen->currentMon, MON_DATA_LEVEL);
    species = GetMonData(&sMonSummaryScreen->currentMon, MON_DATA_SPECIES);

    if (level < 100)
    {
        totalExpToNextLevel = gExperienceTables[gBaseStats[species].growthRate][level + 1] - gExperienceTables[gBaseStats[species].growthRate][level];
        curExpToNextLevel = exp - gExperienceTables[gBaseStats[species].growthRate][level];
        v0 = ((totalExpToNextLevel << 2) / 8);
        v1 = (curExpToNextLevel << 2);

        while (TRUE)
        {
            if (v1 <= v0)
                break;
            v1 -= v0;
            numWholeExpBarTiles++;
        }

        numWholeExpBarTiles += two;

        for (i = two; i < numWholeExpBarTiles; i++)
            StartSpriteAnim(sExpBar->sprites[i], 8);

        if (numWholeExpBarTiles >= 10)
        {
            if (totalExpToNextLevel == curExpToNextLevel)
                return;
            else
                StartSpriteAnim(sExpBar->sprites[9], 7);
        }

        animNum = (v1 * 8) / v0;
        StartSpriteAnim(sExpBar->sprites[numWholeExpBarTiles], animNum);

        for (i = numWholeExpBarTiles + 1; i < 10; i++)
            StartSpriteAnim(sExpBar->sprites[i], 0);
    }
    else
        for (i = two; i < 10; i++)
            StartSpriteAnim(sExpBar->sprites[i], 0);

    StartSpriteAnim(sExpBar->sprites[0], 9);
    StartSpriteAnim(sExpBar->sprites[1], 10);
    StartSpriteAnim(sExpBar->sprites[10], 11);
}

static void DestroyExpBarSprites(void)
{
    u8 i;

    for (i = 0; i < EXP_BAR_SPRITES_COUNT; i++)
        if (sExpBar->sprites[i] != NULL)
            DestroySpriteAndFreeResources(sExpBar->sprites[i]);

    FREE_AND_SET_NULL(sExpBar);
}

static void SetExpBarSprites(u8 invisible)
{
    u8 i;

    for (i = 0; i < EXP_BAR_SPRITES_COUNT; i++)
        sExpBar->sprites[i]->invisible = invisible;
}

static void LimitEggSummaryPageDisplay(void) // If the pokemon is an egg, limit the number of pages displayed to 1
{
    if (sMonSummaryScreen->currPageIndex == PSS_PAGE_INFO) 
    {
        ScheduleBgCopyTilemapToVram(1);
        ScheduleBgCopyTilemapToVram(2);
        if (sMonSummaryScreen->summary.isEgg)
        {
            SetBgTilemapBuffer(1, sMonSummaryScreen->bgTilemapBuffers[0][SUMMARY_BG_EGG_INFO_PAGE]);
            SetBgTilemapBuffer(2, sMonSummaryScreen->bgTilemapBuffers[1][SUMMARY_BG_EGG_INFO_PAGE]);
        }
        else
        {
            SetBgTilemapBuffer(1, sMonSummaryScreen->bgTilemapBuffers[0][SUMMARY_BG_INFO_PAGE]);
            SetBgTilemapBuffer(2, sMonSummaryScreen->bgTilemapBuffers[1][SUMMARY_BG_INFO_PAGE]);
        }
        ShowBg(2);
    }
}

static void ResetWindows(void)
{
    u8 i;

    InitWindows(sPssHeaderWindowTemplates);
    DeactivateAllTextPrinters();
    FillWindowPixelBuffer(0, PIXEL_FILL(0));
    for (i = 0; i < ARRAY_COUNT(sMonSummaryScreen->windowIds); i++)
        sMonSummaryScreen->windowIds[i] = WINDOW_NONE;
}

static void PrintTextOnWindow(u8 windowId, const u8 *string, u8 x, u8 y, u8 lineSpacing, u8 colorId)
{
    AddTextPrinterParameterized4(windowId, 2, x, y, 0, lineSpacing, sTextColors[colorId], 0, string);
}

static void PrintPokemonName(bool32 movesPage)
{
    struct PokeSummary *summary = &sMonSummaryScreen->summary;
    FillWindowPixelBuffer(WIN_NAME, PIXEL_FILL(0));
    
    if (!sMonSummaryScreen->summary.isEgg)
    {
        if (!movesPage)
            PrintMonLevel(summary);
        PrintMonNickname(movesPage);
        PrintGenderSymbol(&sMonSummaryScreen->currentMon, summary->species2);
    }
    CopyWindowToVram(WIN_NAME, 2);
}

static const u8 sText_PokemonInfo[] = _("{POKEMON_CAPS} INFO");
static const u8 sText_PokemonSkills[] = _("{POKEMON_CAPS} SKILLS");
static const u8 sText_PokemonMoves[] = _("KNOWN MOVES");
static const u8 sText_DpadPageACancel[] = _("{DPAD_RIGHT}PAGE{CLEAR 0x08}{A_BUTTON}CANCEL");
static void PrintPSSHeader(u8 page)
{
    const u8 *str;
    s32 x;
    
    FillWindowPixelBuffer(WIN_HEADER, PIXEL_FILL(0));
    
    PrintPokemonName(page == PSS_PAGE_BATTLE_MOVES);
    switch (page)
    {
    case PSS_PAGE_INFO:
        str = sText_PokemonInfo;
        break;
    case PSS_PAGE_SKILLS:
        str = sText_PokemonSkills;
        break;
    case PSS_PAGE_BATTLE_MOVES:
        str = sText_PokemonMoves;
        break;
    }
    
    PrintTextOnWindow(WIN_HEADER, str, 0, 1, 0, PSS_TEXT_COLOR_WHITE);
    
    x = GetStringRightAlignXOffset(2, sText_DpadPageACancel, 28 * 8);
    PrintTextOnWindow(WIN_HEADER, sText_DpadPageACancel, x, 1, 0, PSS_TEXT_COLOR_WHITE);
    
    CopyWindowToVram(WIN_HEADER, 2);
}

static void PrintMonLevel(struct PokeSummary *summary)
{
    StringCopy(gStringVar1, gText_LevelSymbol);
    ConvertIntToDecimalStringN(gStringVar2, summary->level, STR_CONV_MODE_LEFT_ALIGN, 3);
    StringAppend(gStringVar1, gStringVar2);
    PrintTextOnWindow(WIN_NAME, gStringVar1, 4, 3, 0, PSS_TEXT_COLOR_DARK_GRAY);
}

static void PrintMonNickname(bool32 movesPage)
{
    GetMonNickname(&sMonSummaryScreen->currentMon, gStringVar1);
    PrintTextOnWindow(WIN_NAME, gStringVar1, 40, 3, 0, (movesPage) ? PSS_TEXT_COLOR_WHITE : PSS_TEXT_COLOR_DARK_GRAY);
}

static void PrintGenderSymbol(struct Pokemon *mon, u16 species)
{
    switch (GetMonGender(mon))
    {
    case MON_MALE:
        PrintTextOnWindow(WIN_NAME, gText_MaleSymbol, 104, 3, 0, PSS_TEXT_COLOR_BLUE);
        break;
    case MON_FEMALE:
        PrintTextOnWindow(WIN_NAME, gText_FemaleSymbol, 104, 3, 0, PSS_TEXT_COLOR_RED);
        break;
    }
}

static void PutPageWindowTilemaps(u8 page)
{
    u8 i;

    ClearWindowTilemap(WIN_HEADER);
    PutWindowTilemap(WIN_HEADER);
    ClearWindowTilemap(WIN_NAME);
    PutWindowTilemap(WIN_NAME);

    for (i = 0; i < ARRAY_COUNT(sMonSummaryScreen->windowIds); i++)
        PutWindowTilemap(sMonSummaryScreen->windowIds[i]);

    PrintPSSHeader(page);
    ScheduleBgCopyTilemapToVram(0);
}

static void ClearPageWindowTilemaps(u8 page)
{
    u8 i;
    
    switch (page)
    {
    case PSS_PAGE_INFO:
    case PSS_PAGE_SKILLS:
        break;
    case PSS_PAGE_BATTLE_MOVES:
        if (sMonSummaryScreen->mode == PSS_MODE_SELECT_MOVE)
        {
            if (sMonSummaryScreen->newMove != MOVE_NONE || sMonSummaryScreen->firstMoveIndex != MAX_MON_MOVES)
            {
                gSprites[sMonSummaryScreen->splitIconSpriteId].invisible = TRUE;
            }
        }
        break;
    }

    for (i = 0; i < ARRAY_COUNT(sMonSummaryScreen->windowIds); i++)
        RemoveWindowByIndex(i);

    ScheduleBgCopyTilemapToVram(0);
}

static u8 AddWindowFromTemplateList(const struct WindowTemplate *template, u8 templateId)
{
    u8 *windowIdPtr = &sMonSummaryScreen->windowIds[templateId];
    if (*windowIdPtr == WINDOW_NONE)
    {
        *windowIdPtr = AddWindow(&template[templateId]);
        FillWindowPixelBuffer(*windowIdPtr, PIXEL_FILL(0));
    }
    return *windowIdPtr;
}

static void RemoveWindowByIndex(u8 windowIndex)
{
    u8 *windowIdPtr = &sMonSummaryScreen->windowIds[windowIndex];
    if (*windowIdPtr != WINDOW_NONE)
    {
        ClearWindowTilemap(*windowIdPtr);
        RemoveWindow(*windowIdPtr);
        *windowIdPtr = WINDOW_NONE;
    }
}

static void PrintChangeMonPageText(u8 pageIndex)
{
    u16 i;
    for (i = 0; i < ARRAY_COUNT(sMonSummaryScreen->windowIds); i++)
    {
        if (sMonSummaryScreen->windowIds[i] != WINDOW_NONE)
            FillWindowPixelBuffer(sMonSummaryScreen->windowIds[i], PIXEL_FILL(0));
    }
    sChangedMonTextPrintFunctions[pageIndex]();
}

static void CreateChangePageTask(u8 pageIndex)
{
    CreateTask(sChangePageTextPrinterTasks[pageIndex], 16);
}

static void PrintInfoPageText(void)
{
    PrintPSSHeader(PSS_PAGE_INFO);
    if (sMonSummaryScreen->summary.isEgg)
    {
        PrintEggName();
        ClearHeldItemText();
        PrintEggMemo();
    }
    else
    {
        PrintMonDexNum();
        PrintMonSpecies();
        PrintMonOTName();
        PrintMonOTID();
        PrintHeldItemName();
        BufferMonTrainerMemo();
        PrintMonTrainerMemo();
    }
}

static void Task_PrintInfoPage(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    switch (data[0])
    {
    case 1:
        PrintMonDexNum();
        break;
    case 2:
        PrintMonSpecies();
        break;
    case 3:
        PrintMonOTName();
        break;
    case 4:
        PrintMonOTID();
        break;
    case 5:
        PrintHeldItemName();
        break;
    case 6:
        BufferMonTrainerMemo();
        break;
    case 7:
        PrintMonTrainerMemo();
        break;
    case 8:
        DestroyTask(taskId);
        return;
    }
    data[0]++;
}

#define INFO_X          8
#define INFO_Y_0        5
#define INFO_Y_DELTA    15
#define INFO_Y(line)    (INFO_Y_0 + (line * INFO_Y_DELTA))
static void PrintMonDexNum(void)
{
    struct PokeSummary *summary = &sMonSummaryScreen->summary;
    struct Pokemon *mon = &sMonSummaryScreen->currentMon;
    u8 windowId = AddWindowFromTemplateList(sPageInfoTemplate, WIN_INFO);
    u16 dexNum = SpeciesToPokedexNum(summary->species);

    if (dexNum != 0xFFFF)
    {
        ConvertIntToDecimalStringN(gStringVar1, dexNum, STR_CONV_MODE_LEADING_ZEROS, 3);
        PrintTextOnWindow(windowId, gStringVar1, INFO_X, INFO_Y_0, 0, PSS_TEXT_COLOR_DARK_GRAY);
    }
}

static void PrintMonSpecies(void)
{
    struct PokeSummary *summary = &sMonSummaryScreen->summary;
    PrintTextOnWindow(AddWindowFromTemplateList(sPageInfoTemplate, WIN_INFO), &gSpeciesNames[summary->species2][0], INFO_X, INFO_Y(1), 0, PSS_TEXT_COLOR_DARK_GRAY);
}

static void PrintMonOTName(void)
{
    int x, windowId;
    if (InBattleFactory() != TRUE && InSlateportBattleTent() != TRUE)
    {
        windowId = AddWindowFromTemplateList(sPageInfoTemplate, WIN_INFO);
        PrintTextOnWindow(windowId, sMonSummaryScreen->summary.OTName, INFO_X, INFO_Y(3), 0, PSS_TEXT_COLOR_DARK_GRAY);
    }
}

static void PrintMonOTID(void)
{
    if (InBattleFactory() != TRUE && InSlateportBattleTent() != TRUE)
    {
        ConvertIntToDecimalStringN(gStringVar1, (u16)sMonSummaryScreen->summary.OTID, STR_CONV_MODE_LEADING_ZEROS, 5);
        PrintTextOnWindow(AddWindowFromTemplateList(sPageInfoTemplate, WIN_INFO), gStringVar1, INFO_X, INFO_Y(4), 0, PSS_TEXT_COLOR_DARK_GRAY);
    }
}

static void PrintHeldItemName(void)
{
    const u8 *text;
    int x;

    if (sMonSummaryScreen->summary.item == ITEM_ENIGMA_BERRY
        && IsMultiBattle() == TRUE
        && (sMonSummaryScreen->curMonIndex == 1 || sMonSummaryScreen->curMonIndex == 4 || sMonSummaryScreen->curMonIndex == 5))
    {
        text = ItemId_GetName(ITEM_ENIGMA_BERRY);
    }
    else if (sMonSummaryScreen->summary.item == ITEM_NONE)
    {
        text = gText_None;
    }
    else
    {
        CopyItemName(sMonSummaryScreen->summary.item, gStringVar1);
        text = gStringVar1;
    }

    PrintTextOnWindow(AddWindowFromTemplateList(sPageInfoTemplate, WIN_INFO), text, INFO_X, INFO_Y(5), 0, PSS_TEXT_COLOR_DARK_GRAY);
}

static void ClearHeldItemText(void)
{
    u8 windowId = AddWindowFromTemplateList(sPageInfoTemplate, WIN_INFO);
    FillWindowPixelRect(windowId, PIXEL_FILL(0), INFO_X, INFO_Y(5), 10 * 8, 16);
    CopyWindowToVram(windowId, 2);
}

static const u8 sText_ColorRed[] = _("{COLOR}{13}");
static const u8 sText_ColorNormal[] = _("{COLOR DARK_GRAY}");
static void BufferMonTrainerMemo(void)
{
    struct PokeSummary *sum = &sMonSummaryScreen->summary;
    const u8 *text;

    DynamicPlaceholderTextUtil_Reset();
    BufferNatureString();

    if (InBattleFactory() == TRUE || InSlateportBattleTent() == TRUE || IsInGamePartnerMon() == TRUE)
    {
        DynamicPlaceholderTextUtil_ExpandPlaceholders(gStringVar4, gText_XNature);
    }
    else
    {
        u8 *metLevelString = Alloc(32);
        u8 *metLocationString = Alloc(32);
        GetMetLevelString(metLevelString);
        
        DynamicPlaceholderTextUtil_SetPlaceholderPtr(0, sText_ColorRed);
        DynamicPlaceholderTextUtil_SetPlaceholderPtr(1, sText_ColorNormal);
        

        if (sum->metLocation < MAPSEC_NONE)
        {
            GetMapNameHandleAquaHideout(metLocationString, sum->metLocation);
            DynamicPlaceholderTextUtil_SetPlaceholderPtr(4, metLocationString);
        }

        if (DoesMonOTMatchOwner() == TRUE)
        {
            if (sum->metLevel == 0)
                text = (sum->metLocation >= MAPSEC_NONE) ? gText_XNatureHatchedSomewhereAt : gText_XNatureHatchedAtYZ;
            else
                text = (sum->metLocation >= MAPSEC_NONE) ? gText_XNatureMetSomewhereAt : gText_XNatureMetAtYZ;
        }
        else if (sum->metLocation == METLOC_FATEFUL_ENCOUNTER)
        {
            text = gText_XNatureFatefulEncounter;
        }
        else if (sum->metLocation != METLOC_IN_GAME_TRADE && DidMonComeFromGBAGames())
        {
            text = (sum->metLocation >= MAPSEC_NONE) ? gText_XNatureObtainedInTrade : gText_XNatureProbablyMetAt;
        }
        else
        {
            text = gText_XNatureObtainedInTrade;
        }

        DynamicPlaceholderTextUtil_ExpandPlaceholders(gStringVar4, text);
        Free(metLevelString);
        Free(metLocationString);
    }
}

static void PrintMonTrainerMemo(void)
{
    PrintTextOnWindow(AddWindowFromTemplateList(sPageInfoTemplate, WIN_MEMO), gStringVar4, 8, 3, 0, PSS_TEXT_COLOR_DARK_GRAY);
}

static void BufferNatureString(void)
{
    struct PokemonSummaryScreenData *sumStruct = sMonSummaryScreen;
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(2, gNatureNamePointers[sumStruct->summary.nature]);
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(5, gText_EmptyString5);
}

static void GetMetLevelString(u8 *output)
{
    u8 level = sMonSummaryScreen->summary.metLevel;
    if (level == 0)
        level = EGG_HATCH_LEVEL;
    ConvertIntToDecimalStringN(output, level, STR_CONV_MODE_LEFT_ALIGN, 3);
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(3, output);
}

static bool8 DoesMonOTMatchOwner(void)
{
    struct PokeSummary *sum = &sMonSummaryScreen->summary;
    u32 trainerId;
    u8 gender;

    if (sMonSummaryScreen->monList.mons == gEnemyParty)
    {
        u8 multiID = GetMultiplayerId() ^ 1;
        trainerId = gLinkPlayers[multiID].trainerId & 0xFFFF;
        gender = gLinkPlayers[multiID].gender;
        StringCopy(gStringVar1, gLinkPlayers[multiID].name);
    }
    else
    {
        trainerId = GetPlayerIDAsU32() & 0xFFFF;
        gender = gSaveBlock2Ptr->playerGender;
        StringCopy(gStringVar1, gSaveBlock2Ptr->playerName);
    }

    if (gender != sum->OTGender || trainerId != (sum->OTID & 0xFFFF) || StringCompareWithoutExtCtrlCodes(gStringVar1, sum->OTName))
        return FALSE;
    else
        return TRUE;
}

static bool8 DidMonComeFromGBAGames(void)
{
    struct PokeSummary *sum = &sMonSummaryScreen->summary;
    if (sum->metGame > 0 && sum->metGame <= VERSION_LEAF_GREEN)
        return TRUE;
    return FALSE;
}

bool8 DidMonComeFromRSE(void)
{
    struct PokeSummary *sum = &sMonSummaryScreen->summary;
    if (sum->metGame > 0 && sum->metGame <= VERSION_EMERALD)
        return TRUE;
    return FALSE;
}

static bool8 IsInGamePartnerMon(void)
{
    if ((gBattleTypeFlags & BATTLE_TYPE_INGAME_PARTNER) && gMain.inBattle)
    {
        if (sMonSummaryScreen->curMonIndex == 1 || sMonSummaryScreen->curMonIndex == 4 || sMonSummaryScreen->curMonIndex == 5)
            return TRUE;
    }
    return FALSE;
}

static void PrintEggName(void)
{
    GetMonNickname(&sMonSummaryScreen->currentMon, gStringVar1);
    PrintTextOnWindow(AddWindowFromTemplateList(sPageInfoTemplate, WIN_INFO), gStringVar1, INFO_X, INFO_Y(1), 0, PSS_TEXT_COLOR_DARK_GRAY);
}

static void PrintEggMemo(void)
{
    const u8 *text;
    struct PokeSummary *sum = &sMonSummaryScreen->summary;

    if (sMonSummaryScreen->summary.sanity != 1)
    {
        if (sum->metLocation == METLOC_FATEFUL_ENCOUNTER)
            text = gText_PeculiarEggNicePlace;
        else if (DidMonComeFromGBAGames() == FALSE || DoesMonOTMatchOwner() == FALSE)
            text = gText_PeculiarEggTrade;
        else if (sum->metLocation == METLOC_SPECIAL_EGG)
            text = (DidMonComeFromRSE() == TRUE) ? gText_EggFromHotSprings : gText_EggFromTraveler;
        else
            text = gText_OddEggFoundByCouple;
    }
    else
    {
        text = gText_OddEggFoundByCouple;
    }
    
    
    StringCopy(gStringVar4, text);
    StringAppend(gStringVar4, gText_Space);
    
    // append the state
    if (sMonSummaryScreen->summary.sanity == TRUE)
        StringAppend(gStringVar4, gText_EggWillTakeALongTime);
    else if (sum->friendship <= 5)
        StringAppend(gStringVar4, gText_EggAboutToHatch);
    else if (sum->friendship <= 10)
        StringAppend(gStringVar4, gText_EggWillHatchSoon);
    else if (sum->friendship <= 40)
        StringAppend(gStringVar4, gText_EggWillTakeSomeTime);
    else
        StringAppend(gStringVar4, gText_EggWillTakeALongTime);
    

    PrintTextOnWindow(AddWindowFromTemplateList(sPageInfoTemplate, WIN_MEMO), gStringVar4, 8, 4, 0, PSS_TEXT_COLOR_DARK_GRAY);
}

static void PrintSkillsPageText(void)
{
    PrintMonStats();
    PrintHP();
    PrintMonAbilityName();
    PrintMonAbilityDescription();
    PrintExp();
}

static void Task_PrintSkillsPage(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    switch (data[0])
    {
    case 1:
        PrintMonStats();
        break;
    case 2:
        PrintHP();
        break;
    case 3:
        PrintMonAbilityName();
        break;
    case 4:
        PrintMonAbilityDescription();
        break;
    case 5:
        PrintExp();
        break;
    case 6:
        DestroyTask(taskId);
        return;
    }
    data[0]++;
}

static const u8 sText_NeutralNature[] = _("{COLOR}{02}{SHADOW}{03}{STR_VAR_1}");
static const u8 sTextNatureDown[] = _("{COLOR}{15}{SHADOW}{03}{STR_VAR_1}");
static const u8 sTextNatureUp[] = _("{COLOR}{13}{SHADOW}{03}{STR_VAR_1}");
static u8 *BufferStat(u8 *dst, u32 stat, s8 natureMod)
{
    u8 *txtPtr;
    
    if (natureMod == 0)
        txtPtr = StringCopy(dst, sText_NeutralNature);
    else if (natureMod > 0)
        txtPtr = StringCopy(dst, sTextNatureUp);
    else
        txtPtr = StringCopy(dst, sTextNatureDown);
    
    return ConvertIntToDecimalStringN(txtPtr, stat, STR_CONV_MODE_RIGHT_ALIGN, 3);
}

static const u8 sStatIdToTablePos[] = {
    [0] = 0,    //atk
    [1] = 1,    //def
    [2] = 3,    //spatk. 3rd in summary screen, 4th in nature table
    [3] = 4,    //spdef
    [4] = 2,    //speed. last in summary screen, 3rd in nature table
};
#define STAT_Y(i)   (6 + (13 * i))
static void PrintMonStats(void)
{
    const s8 *natureMod = gNatureStatTable[sMonSummaryScreen->summary.nature];
    u8 windowId = AddWindowFromTemplateList(sPageSkillsTemplate, WIN_STATS);
    u32 i;
    
    for (i = 0; i < 5; i++)
    {
        BufferStat(gStringVar2, sMonSummaryScreen->summary.stat[i], natureMod[sStatIdToTablePos[i]]);
        PrintTextOnWindow(windowId, gStringVar2, 6, STAT_Y(i), 0, PSS_TEXT_COLOR_DARK_GRAY);
    }
}

static void PrintHP(void)
{
    u8 *currentHPString = Alloc(8);
    u8 *maxHPString = Alloc(8);
    u32 x;
    
    ConvertIntToDecimalStringN(currentHPString, sMonSummaryScreen->summary.currentHP, STR_CONV_MODE_RIGHT_ALIGN, 3);
    ConvertIntToDecimalStringN(maxHPString, sMonSummaryScreen->summary.maxHP, STR_CONV_MODE_RIGHT_ALIGN, 3);

    DynamicPlaceholderTextUtil_Reset();
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(0, currentHPString);
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(1, maxHPString);
    DynamicPlaceholderTextUtil_ExpandPlaceholders(gStringVar1, sHPLayout);
    Free(currentHPString);
    Free(maxHPString);
    
    x = GetStringRightAlignXOffset(2, gStringVar1, 76);
    PrintTextOnWindow(AddWindowFromTemplateList(sPageSkillsTemplate, WIN_HP), gStringVar1, x, 5, 0, PSS_TEXT_COLOR_DARK_GRAY);
}

static void PrintMonAbilityName(void)
{
    u8 ability = GetAbilityBySpecies(sMonSummaryScreen->summary.species, sMonSummaryScreen->summary.abilityNum);
    PrintTextOnWindow(AddWindowFromTemplateList(sPageSkillsTemplate, WIN_ABILITY_NAME), gAbilityNames[ability], 1, 8, 0, PSS_TEXT_COLOR_DARK_GRAY);
}

static void PrintMonAbilityDescription(void)
{
    u8 ability = GetAbilityBySpecies(sMonSummaryScreen->summary.species, sMonSummaryScreen->summary.abilityNum);
    PrintTextOnWindow(AddWindowFromTemplateList(sPageSkillsTemplate, WIN_ABILITY_DESC), gAbilityDescriptionPointers[ability], 2, 5, 0, PSS_TEXT_COLOR_DARK_GRAY);
}

static void PrintExp(void)
{
    struct PokeSummary *sum = &sMonSummaryScreen->summary;
    u32 expToNextLevel;
    int x;
    u8 windowId = AddWindowFromTemplateList(sPageSkillsTemplate, WIN_EXP);
    
    // Print Exp. Points & Next Lv. headers
    PrintTextOnWindow(windowId, gText_ExpPoints, 9, 0, 0, PSS_TEXT_COLOR_DARK_GRAY);
    PrintTextOnWindow(windowId, gText_NextLv, 9, 12, 0, PSS_TEXT_COLOR_DARK_GRAY);
    
    // Print Exp. Points
    ConvertIntToDecimalStringN(gStringVar2, sMonSummaryScreen->summary.exp, STR_CONV_MODE_RIGHT_ALIGN, 7);
    x = GetStringRightAlignXOffset(2, gStringVar2, 21 * 8);
    PrintTextOnWindow(windowId, gStringVar2, x, 0, 0, PSS_TEXT_COLOR_DARK_GRAY);
    
    // Print Next Lv.
    if (sum->level < MAX_LEVEL)
        expToNextLevel = gExperienceTables[gBaseStats[sum->species].growthRate][sum->level + 1] - sum->exp;
    else
        expToNextLevel = 0;
    ConvertIntToDecimalStringN(gStringVar3, expToNextLevel, STR_CONV_MODE_RIGHT_ALIGN, 6);
    x = GetStringRightAlignXOffset(2, gStringVar3, 21 * 8);
    PrintTextOnWindow(windowId, gStringVar3, x, 12, 0, PSS_TEXT_COLOR_DARK_GRAY);
}

static void PrintBattleMoves(void)
{
    PrintPSSHeader(PSS_PAGE_BATTLE_MOVES);
    PrintMoveNameAndPP(0);
    PrintMoveNameAndPP(1);
    PrintMoveNameAndPP(2);
    PrintMoveNameAndPP(3);

    if (sMonSummaryScreen->mode == PSS_MODE_SELECT_MOVE)
    {
        PrintNewMoveDetailsOrCancelText();
        if (sMonSummaryScreen->firstMoveIndex == MAX_MON_MOVES)
        {
            if (sMonSummaryScreen->newMove != MOVE_NONE)
                PrintMoveDetails(sMonSummaryScreen->newMove);
        }
        else
        {
            PrintMoveDetails(sMonSummaryScreen->summary.moves[sMonSummaryScreen->firstMoveIndex]);
        }
    }
}

static void Task_PrintBattleMoves(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    switch (data[0])
    {
    case 1:
        PrintMoveNameAndPP(0);
        break;
    case 2:
        PrintMoveNameAndPP(1);
        break;
    case 3:
        PrintMoveNameAndPP(2);
        break;
    case 4:
        PrintMoveNameAndPP(3);
        break;
    case 5:
        if (sMonSummaryScreen->mode == PSS_MODE_SELECT_MOVE)
            PrintNewMoveDetailsOrCancelText();
        break;
    case 6:
        if (sMonSummaryScreen->mode == PSS_MODE_SELECT_MOVE)
        {
            if (sMonSummaryScreen->firstMoveIndex == MAX_MON_MOVES)
                data[1] = sMonSummaryScreen->newMove;
            else
                data[1] = sMonSummaryScreen->summary.moves[sMonSummaryScreen->firstMoveIndex];
        }
        break;
    case 7:
        if (sMonSummaryScreen->mode == PSS_MODE_SELECT_MOVE)
        {
            if (sMonSummaryScreen->newMove != MOVE_NONE || sMonSummaryScreen->firstMoveIndex != MAX_MON_MOVES)
                PrintMoveDetails(data[1]);
        }
        break;
    case 8:
        DestroyTask(taskId);
        return;
    }
    data[0]++;
}

static int PpStateToTextColor(int ppState)
{
    switch(ppState)
    {
        case PP_STATE_LESS_THAN_HALF: // Less than 1/2 PP
            return PSS_TEXT_COLOR_PP_YELLOW;
        case PP_STATE_LESS_THAN_QUARTER: // Less than 1/4 PP
            return PSS_TEXT_COLOR_PP_ORANGE;
        case PP_STATE_ZERO: // 0 PP
            return PSS_TEXT_COLOR_PP_RED;
        case PP_STATE_MORE_THAN_HALF: // Max PP
        default:
            return PSS_TEXT_COLOR_DARK_GRAY;
    }
}

#define MOVE_NAME_Y(index) ((index) * 28 + 6)
#define MOVE_PP_Y(index) ((index) * 28 + 17)
static void PrintMoveNameAndPP(u8 moveIndex)
{
    u8 pp;
    int textColor, x;
    const u8 *text;
    struct PokeSummary *summary = &sMonSummaryScreen->summary;
    u8 windowId = AddWindowFromTemplateList(sPageMovesTemplate, WIN_MOVE_NAMES);
    u16 move = summary->moves[moveIndex];

    if (move != MOVE_NONE)
    {
        pp = CalculatePPWithBonus(move, summary->ppBonuses, moveIndex);
        PrintTextOnWindow(windowId, gMoveNames[move], 0, MOVE_NAME_Y(moveIndex), 0, PSS_TEXT_COLOR_DARK_GRAY);
        ConvertIntToDecimalStringN(gStringVar1, summary->pp[moveIndex], STR_CONV_MODE_RIGHT_ALIGN, 2);
        ConvertIntToDecimalStringN(gStringVar2, pp, STR_CONV_MODE_RIGHT_ALIGN, 2);
        DynamicPlaceholderTextUtil_Reset();
        DynamicPlaceholderTextUtil_SetPlaceholderPtr(0, gStringVar1);
        DynamicPlaceholderTextUtil_SetPlaceholderPtr(1, gStringVar2);
        DynamicPlaceholderTextUtil_ExpandPlaceholders(gStringVar4, sMovesPPLayout);
        text = gStringVar4;
        textColor = PpStateToTextColor(GetCurrentPpToMaxPpState(summary->pp[moveIndex], pp));
    }
    else
    {
        PrintTextOnWindow(windowId, gText_OneDash, 0, MOVE_NAME_Y(moveIndex), 0, PSS_TEXT_COLOR_DARK_GRAY);
        text = gText_TwoDashes;
        textColor = PpStateToTextColor(PP_STATE_MORE_THAN_HALF);
    }

    // print PP
    x = GetStringRightAlignXOffset(2, text, 10 * 8 - 2);
    PrintTextOnWindow(windowId, text, x, MOVE_PP_Y(moveIndex) + 1, 0, textColor);
}

static void PrintMovePowerAndAccuracy(u8 windowId, u16 moveIndex)
{
    const u8 *text;

    if (gBattleMoves[moveIndex].power < 2)
    {
        text = gText_ThreeDashes;
    }
    else
    {
        ConvertIntToDecimalStringN(gStringVar1, gBattleMoves[moveIndex].power, STR_CONV_MODE_RIGHT_ALIGN, 3);
        text = gStringVar1;
    }

    PrintTextOnWindow(windowId, text, 7, 1, 0, PSS_TEXT_COLOR_DARK_GRAY);
    if (gBattleMoves[moveIndex].accuracy == 0)
    {
        text = gText_ThreeDashes;
    }
    else
    {
        ConvertIntToDecimalStringN(gStringVar1, gBattleMoves[moveIndex].accuracy, STR_CONV_MODE_RIGHT_ALIGN, 3);
        text = gStringVar1;
    }

    PrintTextOnWindow(windowId, text, 7, 15, 0, PSS_TEXT_COLOR_DARK_GRAY);
}

static void PrintMoveDetails(u16 move)
{
    u8 windowId = AddWindowFromTemplateList(sPageMovesTemplate, WIN_MOVE_DETAILS);
    FillWindowPixelBuffer(windowId, PIXEL_FILL(0));
    if (move != MOVE_NONE)
    {
        ShowSplitIcon(gBattleMoves[move].split);
        PrintMovePowerAndAccuracy(windowId, move);
        PutWindowTilemap(windowId);
        
        windowId = AddWindowFromTemplateList(sPageMovesTemplate, WIN_MOVE_DESCRIPTION);
        FillWindowPixelBuffer(windowId, PIXEL_FILL(0));
        PrintTextOnWindow(windowId, gMoveDescriptionPointers[move - 1], 6, 2, 0, PSS_TEXT_COLOR_DARK_GRAY);
        PutWindowTilemap(windowId);
    }
    else
    {
        ClearWindowTilemap(windowId);
        //ClearContestMoveHearts();
        windowId = AddWindowFromTemplateList(sPageMovesTemplate, WIN_MOVE_DESCRIPTION);
        ClearWindowTilemap(windowId);
    }

    ScheduleBgCopyTilemapToVram(0);
}

static void PrintNewMoveDetailsOrCancelText(void)
{
    u8 windowId = AddWindowFromTemplateList(sPageMovesTemplate, WIN_MOVE_NAMES);

    if (sMonSummaryScreen->newMove == MOVE_NONE)
    {
        PrintTextOnWindow(windowId, gText_Cancel, 0, 4 * MOVE_SLOT_DELTA + 5, 0, PSS_TEXT_COLOR_DARK_GRAY);
    }
    else
    {
        u16 move = sMonSummaryScreen->newMove;

        PrintTextOnWindow(windowId, gMoveNames[move], 0, 4 * MOVE_SLOT_DELTA + 5, 0, PSS_TEXT_COLOR_DARK_GRAY);
        ConvertIntToDecimalStringN(gStringVar1, gBattleMoves[move].pp, STR_CONV_MODE_RIGHT_ALIGN, 2);
        DynamicPlaceholderTextUtil_Reset();
        DynamicPlaceholderTextUtil_SetPlaceholderPtr(0, gStringVar1);
        DynamicPlaceholderTextUtil_SetPlaceholderPtr(1, gStringVar1);
        DynamicPlaceholderTextUtil_ExpandPlaceholders(gStringVar4, sMovesPPLayout);
        PrintTextOnWindow(windowId, gStringVar4, GetStringRightAlignXOffset(1, gStringVar4, 158), 4 * MOVE_SLOT_DELTA + 6, 0, PpStateToTextColor(PP_STATE_MORE_THAN_HALF));
    }
}

static void SwapMovesNamesPP(u8 moveIndex1, u8 moveIndex2)
{
    u8 windowId = AddWindowFromTemplateList(sPageMovesTemplate, WIN_MOVE_NAMES);

    FillWindowPixelRect(windowId, PIXEL_FILL(0), 0, moveIndex1 * MOVE_SLOT_DELTA + 5, 170, 16);
    FillWindowPixelRect(windowId, PIXEL_FILL(0), 0, moveIndex2 * MOVE_SLOT_DELTA + 5, 170, 16);

    PrintMoveNameAndPP(moveIndex1);
    PrintMoveNameAndPP(moveIndex2);
}

static void PrintHMMovesCantBeForgotten(void)
{
    u8 windowId = AddWindowFromTemplateList(sPageMovesTemplate, WIN_MOVE_DESCRIPTION);
    FillWindowPixelBuffer(windowId, PIXEL_FILL(0));
    PrintTextOnWindow(windowId, gText_HMMovesCantBeForgotten2, 6, 18, 0, PSS_TEXT_COLOR_DARK_GRAY);
}

static void ResetSpriteIds(void)
{
    u8 i;

    for (i = 0; i < ARRAY_COUNT(sMonSummaryScreen->spriteIds); i++)
        sMonSummaryScreen->spriteIds[i] = SPRITE_NONE;
}

static void DestroySpriteInArray(u8 spriteArrayId)
{
    if (sMonSummaryScreen->spriteIds[spriteArrayId] != SPRITE_NONE)
    {
        DestroySprite(&gSprites[sMonSummaryScreen->spriteIds[spriteArrayId]]);
        sMonSummaryScreen->spriteIds[spriteArrayId] = SPRITE_NONE;
    }
}

static void SetSpriteInvisibility(u8 spriteArrayId, bool8 invisible)
{
    gSprites[sMonSummaryScreen->spriteIds[spriteArrayId]].invisible = invisible;
}

static void HideTypeSprites(void)
{
    u8 i;

    for (i = SPRITE_ARR_ID_TYPE; i < SPRITE_ARR_ID_MOVE_SELECTOR1; i++)
    {
        if (sMonSummaryScreen->spriteIds[i] != SPRITE_NONE)
            SetSpriteInvisibility(i, TRUE);
    }
}

static void HideMoveSelectorSprites(void)
{
    u8 i;

    for (i = SPRITE_ARR_ID_MOVE_SELECTOR1; i < ARRAY_COUNT(sMonSummaryScreen->spriteIds); i++)
    {
        if (sMonSummaryScreen->spriteIds[i] != SPRITE_NONE)
            SetSpriteInvisibility(i, TRUE);
    }
}

static void SwapMonSpriteIconSpriteVisibility(bool8 invisible)
{
    // Hide the Pokémon sprite on the Moves details page, otherwise show
    if (sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MON] != SPRITE_NONE)
        SetSpriteInvisibility(SPRITE_ARR_ID_MON, invisible);
    
    // Show the Pokémon's icon sprite on the Moves details page, otherwise hide
    if (sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MON_ICON] != SPRITE_NONE)
        SetSpriteInvisibility(SPRITE_ARR_ID_MON_ICON, !invisible);
}

static void ToggleSpritesForMovesPage(void)
{
    bool8 invisible = TRUE;
    
    if (sMonSummaryScreen->currPageIndex == PSS_PAGE_BATTLE_MOVES)
    {
        // When on the Moves pages, hide the Shiny star
        if (sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_SHINY_STAR] != SPRITE_NONE)
            SetSpriteInvisibility(SPRITE_ARR_ID_SHINY_STAR, invisible);
        
        SwapMonSpriteIconSpriteVisibility(TRUE);
        
        // moves page -> hide status icon
        if (sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_STATUS] != SPRITE_NONE)
            SetSpriteInvisibility(SPRITE_ARR_ID_STATUS, TRUE);
    }
    else
    {
        invisible = FALSE;
        
        // When not on the Moves pages, try and show the Shiny star
        CreateSetShinyStar(&sMonSummaryScreen->currentMon);
        SwapMonSpriteIconSpriteVisibility(FALSE);
        CreateSetStatusSprite();
    }
    
    // Hide the Ball sprite on the Moves pages, otherwise show
    if (sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_BALL] != SPRITE_NONE)
        SetSpriteInvisibility(SPRITE_ARR_ID_BALL, invisible);    
}

static void SetTypeIcons(void)
{
    switch (sMonSummaryScreen->currPageIndex)
    {
    case PSS_PAGE_INFO:
        SetMonTypeIcons(PSS_PAGE_INFO);
        break;
    case PSS_PAGE_BATTLE_MOVES:
        SetMoveTypeIcons();
        SetNewMoveTypeIcon();
        SetMonTypeIcons(PSS_PAGE_BATTLE_MOVES);
        break;
    }
}

static void CreateMoveTypeIcons(void)
{
    u8 i;

    for (i = SPRITE_ARR_ID_TYPE; i < SPRITE_ARR_ID_MOVE_SELECTOR1; i++)
    {
        if (sMonSummaryScreen->spriteIds[i] == SPRITE_NONE)
            sMonSummaryScreen->spriteIds[i] = CreateSprite(&gSpriteTemplate_MoveTypes, 0, 0, 2);

        SetSpriteInvisibility(i, TRUE);
    }
}

static void SetTypeSpritePosAndPal(u8 typeId, u8 x, u8 y, u8 spriteArrayId)
{
    struct Sprite *sprite = &gSprites[sMonSummaryScreen->spriteIds[spriteArrayId]];
    StartSpriteAnim(sprite, typeId);
    sprite->oam.paletteNum = gMoveTypeToOamPaletteNum[typeId];
    sprite->pos1.x = x + 16;
    sprite->pos1.y = y + 8;
    SetSpriteInvisibility(spriteArrayId, FALSE);
}

static void SetMonTypeIcons(u8 page)
{
    struct PokeSummary *summary = &sMonSummaryScreen->summary;
    s32 x, y;
    
    x = (page == PSS_PAGE_BATTLE_MOVES) ? 40 : 168;
    y = (page == PSS_PAGE_BATTLE_MOVES) ? 34 : 50;
    
    if (summary->isEgg)
    {
        SetSpriteInvisibility(SPRITE_ARR_ID_TYPE, TRUE);
        SetSpriteInvisibility(SPRITE_ARR_ID_TYPE_2, TRUE);
    }
    else
    {
        SetTypeSpritePosAndPal(gBaseStats[summary->species].type1, x, y, SPRITE_ARR_ID_TYPE);
        if (gBaseStats[summary->species].type1 != gBaseStats[summary->species].type2)
        {
            SetTypeSpritePosAndPal(gBaseStats[summary->species].type2, x + 34, y, SPRITE_ARR_ID_TYPE_2);
            SetSpriteInvisibility(SPRITE_ARR_ID_TYPE_2, FALSE);
        }
        else
        {
            SetSpriteInvisibility(SPRITE_ARR_ID_TYPE_2, TRUE);
        }
    }
}

static void SetMoveTypeIcons(void)
{
    u8 i;
    struct PokeSummary *summary = &sMonSummaryScreen->summary;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (summary->moves[i] != MOVE_NONE)
            SetTypeSpritePosAndPal(gBattleMoves[summary->moves[i]].type, 124, 20 + (i * MOVE_SLOT_DELTA), i + SPRITE_ARR_ID_MOVE_1_TYPE);
        else
            SetSpriteInvisibility(i + SPRITE_ARR_ID_MOVE_1_TYPE, TRUE);
    }
}

static void SetNewMoveTypeIcon(void)
{
    if (sMonSummaryScreen->newMove == MOVE_NONE)
        SetSpriteInvisibility(SPRITE_ARR_ID_MOVE_5_TYPE, TRUE);
    else
        SetTypeSpritePosAndPal(gBattleMoves[sMonSummaryScreen->newMove].type, 124, 20 + (4 * MOVE_SLOT_DELTA), SPRITE_ARR_ID_MOVE_5_TYPE);
}

static void SwapMovesTypeSprites(u8 moveIndex1, u8 moveIndex2)
{
    struct Sprite *sprite1 = &gSprites[sMonSummaryScreen->spriteIds[moveIndex1 + SPRITE_ARR_ID_MOVE_1_TYPE]];
    struct Sprite *sprite2 = &gSprites[sMonSummaryScreen->spriteIds[moveIndex2 + SPRITE_ARR_ID_MOVE_1_TYPE]];

    u8 temp = sprite1->animNum;
    sprite1->animNum = sprite2->animNum;
    sprite2->animNum = temp;

    temp = sprite1->oam.paletteNum;
    sprite1->oam.paletteNum = sprite2->oam.paletteNum;
    sprite2->oam.paletteNum = temp;

    sprite1->animBeginning = TRUE;
    sprite1->animEnded = FALSE;
    sprite2->animBeginning = TRUE;
    sprite2->animEnded = FALSE;
}

static u8 LoadMonGfxAndSprite(struct Pokemon *mon, s16 *state)
{
    const struct CompressedSpritePalette *pal;
    struct PokeSummary *summary = &sMonSummaryScreen->summary;

    switch (*state)
    {
    default:
        return CreateMonSprite(mon);
    case 0:
        if (gMain.inBattle)
        {
            /*if (ShouldIgnoreDeoxysForm(3, sMonSummaryScreen->curMonIndex))
                HandleLoadSpecialPokePic(&gMonFrontPicTable[summary->species2], gMonSpritesGfxPtr->sprites[1], summary->species2, summary->pid);
            else
                HandleLoadSpecialPokePic(&gMonFrontPicTable[summary->species2], gMonSpritesGfxPtr->sprites[1], summary->species2, summary->pid);*/
            HandleLoadSpecialPokePic(&gMonFrontPicTable[summary->species2], gMonSpritesGfxPtr->sprites.ptr[1], summary->species2, summary->pid);
        }
        else
        {
            if (gMonSpritesGfxPtr != NULL)
            {
                if (sMonSummaryScreen->monList.mons == gPlayerParty || sMonSummaryScreen->mode == PSS_MODE_BOX || sMonSummaryScreen->unk40EF == TRUE)
                    HandleLoadSpecialPokePic(&gMonFrontPicTable[summary->species2], gMonSpritesGfxPtr->sprites.ptr[1], summary->species2, summary->pid);
                else
                    HandleLoadSpecialPokePic(&gMonFrontPicTable[summary->species2], gMonSpritesGfxPtr->sprites.ptr[1], summary->species2, summary->pid);
            }
            else
            {
                if (sMonSummaryScreen->monList.mons == gPlayerParty || sMonSummaryScreen->mode == PSS_MODE_BOX || sMonSummaryScreen->unk40EF == TRUE)
                    HandleLoadSpecialPokePic(&gMonFrontPicTable[summary->species2], sub_806F4F8(0, 1), summary->species2, summary->pid);
                else
                    HandleLoadSpecialPokePic(&gMonFrontPicTable[summary->species2], sub_806F4F8(0, 1), summary->species2, summary->pid);
            }
        }
        (*state)++;
        return SPRITE_NONE;
    case 1:
        pal = GetMonSpritePalStructFromOtIdPersonality(summary->species2, summary->OTID, summary->pid);
        LoadCompressedSpritePalette(pal);
        SetMultiuseSpriteTemplateToPokemon(pal->tag, 1);
        (*state)++;
        return SPRITE_NONE;
    }
}

static void PlayMonCry(void)
{
    struct PokeSummary *summary = &sMonSummaryScreen->summary;
    if (!summary->isEgg)
    {
        if (ShouldPlayNormalMonCry(&sMonSummaryScreen->currentMon) == TRUE)
            PlayCry3(summary->species2, 0, 0);
        else
            PlayCry3(summary->species2, 0, 11);
    }
}

static u8 CreateMonSprite(struct Pokemon *unused)
{
    struct PokeSummary *summary = &sMonSummaryScreen->summary;
    u8 spriteId = CreateSprite(&gMultiuseSpriteTemplate, 60, 65, 5);

    FreeSpriteOamMatrix(&gSprites[spriteId]);
    gSprites[spriteId].data[0] = summary->species2;
    gSprites[spriteId].data[2] = 0;
    gSprites[spriteId].callback = SpriteCB_Pokemon;
    gSprites[spriteId].oam.priority = 0;
    gSprites[spriteId].hFlip = IsMonSpriteNotFlipped(summary->species2);

    return spriteId;
}

static void SpriteCB_Pokemon(struct Sprite *sprite)
{
    struct PokeSummary *summary = &sMonSummaryScreen->summary;

    if (!gPaletteFade.active && sprite->data[2] != 1)
    {
        sprite->data[1] = !IsMonSpriteNotFlipped(sprite->data[0]);
        PlayMonCry();
        PokemonSummaryDoMonAnimation(sprite, sprite->data[0], summary->isEgg);
    }
}

void SummaryScreen_SetAnimDelayTaskId(u8 taskId)
{
    sAnimDelayTaskId = taskId;
}

void SummaryScreen_DestroyAnimDelayTask(void)
{
    if (sAnimDelayTaskId != TASK_NONE)
    {
        DestroyTask(sAnimDelayTaskId);
        sAnimDelayTaskId = TASK_NONE;
    }
}

// unused
static bool32 IsMonAnimationFinished(void)
{
    if (gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MON]].callback == SpriteCallbackDummy)
        return FALSE;
    else
        return TRUE;
}

static void StopPokemonAnimations(void)  // A subtle effect, this function stops pokemon animations when leaving the PSS
{
    u16 i;
    u16 paletteIndex;

    gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MON]].animPaused = TRUE;
    gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MON]].callback = SpriteCallbackDummy;
    StopPokemonAnimationDelayTask();

    paletteIndex = (gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MON]].oam.paletteNum * 16) | 0x100;

    for (i = 0; i < 16; i++)
    {
        u16 id = i + paletteIndex;
        gPlttBufferUnfaded[id] = gPlttBufferFaded[id];
    }
}

static void CreateCaughtBallSprite(struct Pokemon *mon)
{
    u8 ball = ItemIdToBallId(GetMonData(mon, MON_DATA_POKEBALL));

    LoadBallGfx(ball);
    sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_BALL] = CreateSprite(&gBallSpriteTemplates[ball], 99, 91, 0);
    gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_BALL]].callback = SpriteCallbackDummy;
    gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_BALL]].oam.priority = 1;
    gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_BALL]].invisible = (sMonSummaryScreen->currPageIndex >= PSS_PAGE_BATTLE_MOVES);
}

#define RGB2GBA(r, g, b) (((r >> 3) & 31) | (((g >> 3) & 31) << 5) | (((b >> 3) & 31) << 10))
#define RGB_SHINY_PURPLE_1  RGB2GBA(153, 125, 155)
#define RGB_SHINY_PURPLE_2  RGB2GBA(100, 88, 136)
#define RGB_REG_1  RGB2GBA(128, 120, 96)
#define RGB_REG_2  RGB2GBA(96, 88, 64)

static const u16 sShinySpriteBgColors[2] = 
{
    RGB_SHINY_PURPLE_1,
    RGB_SHINY_PURPLE_2,
};

static void CreateSetShinyStar(struct Pokemon *mon)
{
    u8 *spriteId = &sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_SHINY_STAR];
    bool8 isInvisible = TRUE;
    
    if (*spriteId == SPRITE_NONE)
        *spriteId = CreateSprite(&sSpriteTemplate_ShinyStar, 100, 43, 0);
    
    if (IsMonShiny(mon))
    {
        // update palettes for shiny box        
        gPlttBufferUnfaded[10] = RGB_SHINY_PURPLE_1;
        gPlttBufferUnfaded[11] = RGB_SHINY_PURPLE_2;
        gPlttBufferUnfaded[16 + 10] = RGB_SHINY_PURPLE_1;
        gPlttBufferUnfaded[16 + 11] = RGB_SHINY_PURPLE_2;
        gPlttBufferUnfaded[32 + 10] = RGB_SHINY_PURPLE_1;
        gPlttBufferUnfaded[32 + 11] = RGB_SHINY_PURPLE_2;     
        
        gPlttBufferFaded[10] = RGB_SHINY_PURPLE_1;
        gPlttBufferFaded[11] = RGB_SHINY_PURPLE_2;
        gPlttBufferFaded[16 + 10] = RGB_SHINY_PURPLE_1;
        gPlttBufferFaded[16 + 11] = RGB_SHINY_PURPLE_2;
        gPlttBufferFaded[32 + 10] = RGB_SHINY_PURPLE_1;
        gPlttBufferFaded[32 + 11] = RGB_SHINY_PURPLE_2;
        
        if (sMonSummaryScreen->currPageIndex != PSS_PAGE_BATTLE_MOVES)
            isInvisible = FALSE;
    }
    else
    {
        gPlttBufferUnfaded[10] = RGB_REG_1;
        gPlttBufferUnfaded[11] = RGB_REG_2;
        gPlttBufferUnfaded[16 + 10] = RGB_REG_1;
        gPlttBufferUnfaded[16 + 11] = RGB_REG_2;
        gPlttBufferUnfaded[32 + 10] = RGB_REG_1;
        gPlttBufferUnfaded[32 + 11] = RGB_REG_2;
        
        gPlttBufferFaded[10] = RGB_REG_1;
        gPlttBufferFaded[11] = RGB_REG_2;
        gPlttBufferFaded[16 + 10] = RGB_REG_1;
        gPlttBufferFaded[16 + 11] = RGB_REG_2;
        gPlttBufferFaded[32 + 10] = RGB_REG_1;
        gPlttBufferFaded[32 + 11] = RGB_REG_2;
    }
    
    SetSpriteInvisibility(SPRITE_ARR_ID_SHINY_STAR, isInvisible);
}

static void CreateMonIconSprite(void)
{
    struct PokeSummary *summary = &sMonSummaryScreen->summary;
    sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MON_ICON] = CreateMonIcon(summary->species2, SpriteCallbackDummy, 24, 32, 0, summary->pid);
    SetSpriteInvisibility(SPRITE_ARR_ID_MON_ICON, TRUE);
}

static void CreateSetStatusSprite(void)
{
    u8 *spriteId = &sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_STATUS];
    u8 statusAnim;

    if (*spriteId == SPRITE_NONE)
        *spriteId = CreateSprite(&sSpriteTemplate_StatusCondition, 25, 38, 0);

    statusAnim = sMonSummaryScreen->summary.ailment;
    if (statusAnim != 0)
    {
        StartSpriteAnim(&gSprites[*spriteId], statusAnim - 1);
        SetSpriteInvisibility(SPRITE_ARR_ID_STATUS, FALSE);
    }
    else
    {
        SetSpriteInvisibility(SPRITE_ARR_ID_STATUS, TRUE);
    }
}

static void CreateMoveSelectorSprites(u8 idArrayStart)
{
    u8 i;
    u8 *spriteIds = &sMonSummaryScreen->spriteIds[idArrayStart];

    if (sMonSummaryScreen->currPageIndex == PSS_PAGE_BATTLE_MOVES)
    {
        u8 subpriority = 0;
        if (idArrayStart == SPRITE_ARR_ID_MOVE_SELECTOR1)
            subpriority = 1;

        for (i = 0; i < MOVE_SELECTOR_SPRITES_COUNT; i++)
        {
            spriteIds[i] = CreateSprite(&sMoveSelectorSpriteTemplate, i * 64 + 148 + ((i == 0) ? 6 : 0), 35, subpriority);
            if (i == 0)
                StartSpriteAnim(&gSprites[spriteIds[i]], 0); // left
            else
                StartSpriteAnim(&gSprites[spriteIds[i]], 1); // right

            gSprites[spriteIds[i]].callback = SpriteCb_MoveSelector;
            gSprites[spriteIds[i]].data[0] = idArrayStart;
            gSprites[spriteIds[i]].data[1] = 0;
        }
    }
}

static void SpriteCb_MoveSelector(struct Sprite *sprite)
{
    if (sprite->data[0] == SPRITE_ARR_ID_MOVE_SELECTOR1)
    {
        sprite->pos2.y = sMonSummaryScreen->firstMoveIndex * MOVE_SLOT_DELTA;
    }
    else
    {
        sprite->pos2.y = sMonSummaryScreen->secondMoveIndex * MOVE_SLOT_DELTA;
        
        sprite->data[1] = (sprite->data[1] + 1) & 0x1F;
        if (sprite->data[1] > 24)
            sprite->invisible = TRUE;
        else
            sprite->invisible = FALSE;
    }
}

static void DestroyMoveSelectorSprites(u8 firstArrayId)
{
    u8 i;
    for (i = 0; i < MOVE_SELECTOR_SPRITES_COUNT; i++)
        DestroySpriteInArray(firstArrayId + i);
}

static void SetMainMoveSelectorColor(u8 which)
{
    u8 i;
    u8 *spriteIds = &sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MOVE_SELECTOR1];

    for (i = 0; i < MOVE_SELECTOR_SPRITES_COUNT; i++)
    {
        StartSpriteAnim(&gSprites[spriteIds[i]], which*2 + i);
    }
}

static void KeepMoveSelectorVisible(u8 firstSpriteId)
{
    u8 i;
    u8 *spriteIds = &sMonSummaryScreen->spriteIds[firstSpriteId];

    for (i = 0; i < MOVE_SELECTOR_SPRITES_COUNT; i++)
    {
        gSprites[spriteIds[i]].data[1] = 0;
        gSprites[spriteIds[i]].invisible = FALSE;
    }
}
