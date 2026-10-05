// Romhack: pret/pokeheartgold scr_seq_0195_R10R0201.s with the trade's Pokemon buffered for the text (tools/rh/gen_rh_trade_scripts.py).
#include "constants/species.h"
#include "rh/pret_compat.h"
#include "constants/item.h"
#include "constants/moves.h"
#include "rh/pret_constants.h"
.include "asm/include/rh_pret_script.inc"
.include "asm/include/flags.inc"
.include "asm/include/vars.inc"
.include "asm/include/soundeffects.inc"
#include "tools/rh/pret/events/event_R10R0201.h"
	

	.data

	ScrDef scr_seq_R10R0201_000
	ScrDef scr_seq_R10R0201_001
	ScrDef scr_seq_R10R0201_002
	ScrDef scr_seq_R10R0201_003
	ScrDef scr_seq_R10R0201_004
	ScrDef scr_seq_R10R0201_005
	ScrDef scr_seq_R10R0201_006
	ScrDef scr_seq_R10R0201_007
	ScrDef scr_seq_R10R0201_008
	ScrDefEnd

scr_seq_R10R0201_000:
	SimpleNPCMsg 3
	End

scr_seq_R10R0201_001:
	SimpleNPCMsg 4
	End

scr_seq_R10R0201_002:
	SimpleNPCMsg 5
	End

scr_seq_R10R0201_003:
	SimpleNPCMsg 6
	End

scr_seq_R10R0201_004:
	SimpleNPCMsg 7
	End

scr_seq_R10R0201_005:
	SimpleNPCMsg 8
	End

scr_seq_R10R0201_006:
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
	GoToIfSet FLAG_TRADE_POWER_PLANT_DUGTRIO_MAGNETON, _013F
	NPCMsg 9
	TouchscreenMenuHide
	GetMenuChoice VAR_SPECIAL_RESULT
	TouchscreenMenuShow
	Compare VAR_SPECIAL_RESULT, 1
	GoToIfEq _0134
	CloseMsg
	FadeScreen 6, 1, 0, RGB_BLACK
	WaitFade
	ScrCmd_566
	GetPartySelection VAR_SPECIAL_RESULT
	RestoreOverworld
	FadeScreen 6, 1, 1, RGB_BLACK
	WaitFade
	Compare VAR_SPECIAL_RESULT, 255
	GoToIfEq _0134
	LoadNPCTrade 8
	CopyVar VAR_SPECIAL_x8004, VAR_SPECIAL_RESULT
	GetPartyMonSpecies VAR_SPECIAL_x8004, VAR_SPECIAL_x8005
	NPCTradeGetReqSpecies VAR_SPECIAL_RESULT
	Compare VAR_SPECIAL_x8005, VAR_SPECIAL_RESULT
	GoToIfNe _0127
	NPCTradeExec VAR_SPECIAL_x8004
	NPCTradeEnd
	SetFlag FLAG_TRADE_POWER_PLANT_DUGTRIO_MAGNETON
	NPCMsg 10
	WaitButton
	CloseMsg
	ReleaseAll
	End

_0127:
	NPCTradeEnd
	NPCMsg 11
	WaitButton
	CloseMsg
	ReleaseAll
	End

_0134:
	NPCMsg 12
	WaitButton
	CloseMsg
	ReleaseAll
	End

_013F:
	NPCMsg 13
	WaitButton
	CloseMsg
	ReleaseAll
	End

scr_seq_R10R0201_007:
	PlaySE SEQ_SE_DP_SELECT
	LockAll
	NPCMsg 14
	WaitButton
	CloseMsg
	ReleaseAll
	End

scr_seq_R10R0201_008:
	PlaySE SEQ_SE_DP_SELECT
	LockAll
	NPCMsg 15
	WaitButton
	CloseMsg
	ReleaseAll
	End
	.balign 4, 0
