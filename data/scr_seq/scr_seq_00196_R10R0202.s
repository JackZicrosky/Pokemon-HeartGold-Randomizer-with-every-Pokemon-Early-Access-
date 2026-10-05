// Romhack: pret/pokeheartgold scr_seq_0196_R10R0202.s with the trade's Pokemon buffered for the text (tools/rh/gen_rh_trade_scripts.py).
#include "constants/species.h"
#include "rh/pret_compat.h"
#include "constants/item.h"
#include "constants/moves.h"
#include "rh/pret_constants.h"
.include "asm/include/rh_pret_script.inc"
.include "asm/include/flags.inc"
.include "asm/include/vars.inc"
.include "asm/include/soundeffects.inc"
#include "tools/rh/pret/events/event_R10R0202.h"
	

	.data

	ScrDef scr_seq_R10R0202_000
	ScrDef scr_seq_R10R0202_001
	ScrDef scr_seq_R10R0202_002
	ScrDef scr_seq_R10R0202_003
	ScrDef scr_seq_R10R0202_004
	ScrDef scr_seq_R10R0202_005
	ScrDef scr_seq_R10R0202_006
	ScrDef scr_seq_R10R0202_007
	ScrDef scr_seq_R10R0202_008
	ScrDef scr_seq_R10R0202_009
	ScrDefEnd

scr_seq_R10R0202_006:
	PlaySE SEQ_SE_DP_SELECT
	LockAll
	FacePlayer
	HasItem ITEM_MACHINE_PART, 1, VAR_SPECIAL_RESULT
	Compare VAR_SPECIAL_RESULT, 1
	GoToIfGe _0071
	GoToIfSet FLAG_GOT_POWER_PLANT_MANAGERS_STORY, _005C
	SetFlag FLAG_GOT_POWER_PLANT_MANAGERS_STORY
	SetVar VAR_SCENE_ROUTE_10_POWER_PLANT, 1
_005C:
	NPCMsg 13
	WaitButton
	CloseMsg
	ApplyMovement obj_R10R0202_policeman, _0100
	WaitMovement
	ReleaseAll
	End

_0071:
	NPCMsg 14
	CloseMsg
	TakeItem ITEM_MACHINE_PART, 1, VAR_SPECIAL_RESULT
	ApplyMovement obj_R10R0202_gsbigman, _0108
	WaitMovement
	WaitSE SEQ_SE_GS_W012
	FadeScreen 6, 1, 0, RGB_BLACK
	WaitFade
	Warp MAP_ROUTE_10_POWER_PLANT_REPAIRED, 0, 13, 7, DIR_SOUTH
	FadeScreen 6, 1, 1, RGB_BLACK
	WaitFade
	NPCMsg 15
	ApplyMovement obj_R10R0202_gsbigman, _0120
	WaitMovement
	SetFlag FLAG_RESTORED_POWER
	NPCMsg 16
	GiveItemNoCheck ITEM_TM57, 1
	NPCMsg 17
	WaitButton
	CloseMsg
	SetFlag FLAG_UNK_258
	ClearFlag FLAG_HIDE_LAVENDER_RADIO_TOWER_DIRECTOR
	SetFlag FLAG_HIDE_SAFFRON_CITY_COPYCAT_HOUSE_CLEFAIRY_DOLL
	ClearFlag FLAG_HIDE_VERMILION_FAN_CLUB_LOST_ITEM
	SetVar VAR_UNK_40FF, 1
	ReleaseAll
	End

_00F5:
	CallStd std_bag_is_full
	CloseMsg
	ReleaseAll
	End

	.balign 4, 0
_0100:
	FaceSouth
	EndMovement

	.balign 4, 0
_0108:
	WalkOnSpotNormalWest
	Delay2
	WalkOnSpotNormalWest
	Delay2
	WalkOnSpotNormalWest
	EndMovement

	.balign 4, 0
_0120:
	FaceNorth
	EndMovement

scr_seq_R10R0202_007:
	ScrCmd_609
	LockAll
	PlaySE SEQ_SE_GS_PHONE0
	ToggleFollowingPokemonMovement 0
	WaitFollowingPokemonMovement
	FollowingPokemonMovement 56
	ApplyMovement obj_R10R0202_policeman, _0198
	ApplyMovement obj_player, _01B4
	ApplyMovement obj_R10R0202_gsassistantm, _01CC
	WaitMovement
	WaitFollowingPokemonMovement
	ToggleFollowingPokemonMovement 1
	FollowingPokemonMovement 48
	NPCMsg 1
	CloseMsg
	ApplyMovement obj_R10R0202_policeman, _01D8
	ApplyMovement obj_player, _01FC
	WaitMovement
	NPCMsg 2
	CloseMsg
	ApplyMovement obj_R10R0202_policeman, _01E8
	WaitMovement
	ClearFlag FLAG_HIDE_CERULEAN_GYM_ROCKET
	SetVar VAR_SCENE_ROUTE_10_POWER_PLANT, 0
	SetVar VAR_UNK_411C, 1
	ReleaseAll
	End

	.balign 4, 0
_0198:
	EmoteExclamationMark
	WalkNormalSouth
	WalkNormalEast 2
	WalkNormalNorth 3
	WalkNormalEast 5
	WalkNormalNorth 2
	EndMovement

	.balign 4, 0
_01B4:
	Delay32 2
	WalkNormalNorth
	FaceSouth
	Delay16
	FaceEast
	EndMovement

	.balign 4, 0
_01CC:
	Delay32 4
	FaceSouth
	EndMovement

	.balign 4, 0
_01D8:
	WalkNormalSouth 2
	WalkNormalWest 5
	FaceNorth
	EndMovement

	.balign 4, 0
_01E8:
	WalkNormalSouth 3
	WalkNormalWest 2
	WalkNormalNorth
	WalkOnSpotNormalSouth
	EndMovement

	.balign 4, 0
_01FC:
	Delay8 7
	FaceSouth
	EndMovement

scr_seq_R10R0202_000:
	PlaySE SEQ_SE_DP_SELECT
	LockAll
	FacePlayer
	GoToIfSet FLAG_GOT_POWER_PLANT_MANAGERS_STORY, _0226
	NPCMsg 0
	WaitButton
	CloseMsg
	ReleaseAll
	End

_0226:
	NPCMsg 3
	WaitButton
	CloseMsg
	ReleaseAll
	End

scr_seq_R10R0202_005:
	PlaySE SEQ_SE_DP_SELECT
	LockAll
	FacePlayer
	@ romhack: the trade's (randomized) Pokemon, for the text
	LoadNPCTrade 8
	NPCTradeGetReqSpecies VAR_SPECIAL_x8008
	GetOfferedSpecies VAR_SPECIAL_x8009
	NPCTradeEnd
	BufferSpeciesName 6, VAR_SPECIAL_x8008, 0, 0
	BufferSpeciesName 7, VAR_SPECIAL_x8009, 0, 0
	GoToIfSet FLAG_TRADE_POWER_PLANT_DUGTRIO_MAGNETON, _02D8
	NPCMsg 8
	TouchscreenMenuHide
	GetMenuChoice VAR_SPECIAL_RESULT
	TouchscreenMenuShow
	Compare VAR_SPECIAL_RESULT, 1
	GoToIfEq _02CD
	CloseMsg
	FadeScreen 6, 1, 0, RGB_BLACK
	WaitFade
	ScrCmd_566
	GetPartySelection VAR_SPECIAL_RESULT
	RestoreOverworld
	FadeScreen 6, 1, 1, RGB_BLACK
	WaitFade
	Compare VAR_SPECIAL_RESULT, 255
	GoToIfEq _02CD
	LoadNPCTrade 8
	CopyVar VAR_SPECIAL_x8004, VAR_SPECIAL_RESULT
	GetPartyMonSpecies VAR_SPECIAL_x8004, VAR_SPECIAL_x8005
	NPCTradeGetReqSpecies VAR_SPECIAL_RESULT
	Compare VAR_SPECIAL_x8005, VAR_SPECIAL_RESULT
	GoToIfNe _02C0
	NPCTradeExec VAR_SPECIAL_x8004
	NPCTradeEnd
	SetFlag FLAG_TRADE_POWER_PLANT_DUGTRIO_MAGNETON
	NPCMsg 9
	WaitButton
	CloseMsg
	ReleaseAll
	End

_02C0:
	NPCTradeEnd
	NPCMsg 10
	WaitButton
	CloseMsg
	ReleaseAll
	End

_02CD:
	NPCMsg 11
	WaitButton
	CloseMsg
	ReleaseAll
	End

_02D8:
	NPCMsg 12
	WaitButton
	CloseMsg
	ReleaseAll
	End

scr_seq_R10R0202_003:
	SimpleNPCMsg 6
	End

scr_seq_R10R0202_001:
	SimpleNPCMsg 4
	End

scr_seq_R10R0202_002:
	SimpleNPCMsg 5
	End

scr_seq_R10R0202_004:
	SimpleNPCMsg 7
	End

scr_seq_R10R0202_008:
	SimpleNPCMsg 18
	End

scr_seq_R10R0202_009:
	SimpleNPCMsg 19
	End
	.balign 4, 0
