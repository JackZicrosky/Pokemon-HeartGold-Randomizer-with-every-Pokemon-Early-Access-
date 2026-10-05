// Romhack: pret/pokeheartgold scr_seq_0753_T03PC0101.s with the trade's Pokemon buffered for the text (tools/rh/gen_rh_trade_scripts.py).
#include "constants/species.h"
#include "rh/pret_compat.h"
#include "constants/item.h"
#include "constants/moves.h"
#include "rh/pret_constants.h"
.include "asm/include/rh_pret_script.inc"
.include "asm/include/flags.inc"
.include "asm/include/vars.inc"
.include "asm/include/soundeffects.inc"
#include "tools/rh/pret/events/event_T03PC0101.h"
	

	.data

	ScrDef scr_seq_T03PC0101_000
	ScrDef scr_seq_T03PC0101_001
	ScrDef scr_seq_T03PC0101_002
	ScrDef scr_seq_T03PC0101_003
	ScrDef scr_seq_T03PC0101_004
	ScrDefEnd

scr_seq_T03PC0101_000:
	SetVar VAR_SPECIAL_x8007, 0
	CallStd std_nurse_joy
	End

scr_seq_T03PC0101_001:
	PlaySE SEQ_SE_DP_SELECT
	LockAll
	FacePlayer
	@ romhack: the trade's (randomized) Pokemon, for the text
	LoadNPCTrade 9
	NPCTradeGetReqSpecies VAR_SPECIAL_x8008
	GetOfferedSpecies VAR_SPECIAL_x8009
	NPCTradeEnd
	BufferSpeciesName 6, VAR_SPECIAL_x8008, 0, 0
	BufferSpeciesName 7, VAR_SPECIAL_x8009, 0, 0
	GoToIfSet FLAG_TRADE_PEWTER_CITY_HAUNTER_XATU, _00C9
	NPCMsg 3
	TouchscreenMenuHide
	GetMenuChoice VAR_SPECIAL_RESULT
	TouchscreenMenuShow
	Compare VAR_SPECIAL_RESULT, 1
	GoToIfEq _00BE
	CloseMsg
	FadeScreen 6, 1, 0, RGB_BLACK
	WaitFade
	ScrCmd_566
	GetPartySelection VAR_SPECIAL_RESULT
	RestoreOverworld
	FadeScreen 6, 1, 1, RGB_BLACK
	WaitFade
	Compare VAR_SPECIAL_RESULT, 255
	GoToIfEq _00BE
	LoadNPCTrade 9
	CopyVar VAR_SPECIAL_x8004, VAR_SPECIAL_RESULT
	GetPartyMonSpecies VAR_SPECIAL_x8004, VAR_SPECIAL_x8005
	NPCTradeGetReqSpecies VAR_SPECIAL_RESULT
	Compare VAR_SPECIAL_x8005, VAR_SPECIAL_RESULT
	GoToIfNe _00B1
	NPCTradeExec VAR_SPECIAL_x8004
	NPCTradeEnd
	SetFlag FLAG_TRADE_PEWTER_CITY_HAUNTER_XATU
	NPCMsg 4
	WaitButton
	CloseMsg
	ReleaseAll
	End

_00B1:
	NPCTradeEnd
	NPCMsg 5
	WaitButton
	CloseMsg
	ReleaseAll
	End

_00BE:
	NPCMsg 6
	WaitButton
	CloseMsg
	ReleaseAll
	End

_00C9:
	NPCMsg 7
	WaitButton
	CloseMsg
	ReleaseAll
	End

scr_seq_T03PC0101_002:
	SimpleNPCMsg 0
	End

scr_seq_T03PC0101_003:
	PlaySE SEQ_SE_DP_SELECT
	LockAll
	FacePlayer
	PlayCry SPECIES_JIGGLYPUFF, 0
	NPCMsg 1
	WaitCry
	WaitButton
	CloseMsg
	ReleaseAll
	End

scr_seq_T03PC0101_004:
	SimpleNPCMsg 2
	End
	.balign 4, 0
