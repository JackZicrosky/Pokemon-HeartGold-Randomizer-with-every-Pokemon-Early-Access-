// Romhack: pret/pokeheartgold scr_seq_0916_T26R0301.s with the trade's Pokemon buffered for the text (tools/rh/gen_rh_trade_scripts.py).
#include "constants/species.h"
#include "rh/pret_compat.h"
#include "constants/item.h"
#include "constants/moves.h"
#include "rh/pret_constants.h"
.include "asm/include/rh_pret_script.inc"
.include "asm/include/flags.inc"
.include "asm/include/vars.inc"
.include "asm/include/soundeffects.inc"
#include "tools/rh/pret/events/event_T26R0301.h"
	

	.data

	ScrDef scr_seq_T26R0301_000
	ScrDefEnd

scr_seq_T26R0301_000:
	PlaySE SEQ_SE_DP_SELECT
	LockAll
	FacePlayer
	@ romhack: the trade's (randomized) Pokemon, for the text
	LoadNPCTrade 2
	NPCTradeGetReqSpecies VAR_SPECIAL_x8008
	GetOfferedSpecies VAR_SPECIAL_x8009
	NPCTradeEnd
	BufferSpeciesName 6, VAR_SPECIAL_x8008, 0, 0
	BufferSpeciesName 7, VAR_SPECIAL_x8009, 0, 0
	GoToIfSet FLAG_TRADE_OLIVINE_CITY_KRABBY_VOLTORB, _00B3
	NPCMsg 0
	TouchscreenMenuHide
	GetMenuChoice VAR_SPECIAL_RESULT
	TouchscreenMenuShow
	Compare VAR_SPECIAL_RESULT, 0
	GoToIfEq _0037
	GoTo _00A8

_0037:
	CloseMsg
	FadeScreen 6, 1, 0, RGB_BLACK
	WaitFade
	ScrCmd_566
	GetPartySelection VAR_SPECIAL_RESULT
	RestoreOverworld
	FadeScreen 6, 1, 1, RGB_BLACK
	WaitFade
	Compare VAR_SPECIAL_RESULT, 255
	GoToIfEq _00A8
	LoadNPCTrade 2
	CopyVar VAR_SPECIAL_x8004, VAR_SPECIAL_RESULT
	GetPartyMonSpecies VAR_SPECIAL_x8004, VAR_SPECIAL_x8005
	NPCTradeGetReqSpecies VAR_SPECIAL_RESULT
	Compare VAR_SPECIAL_x8005, VAR_SPECIAL_RESULT
	GoToIfNe _009B
	NPCTradeExec VAR_SPECIAL_x8004
	NPCTradeEnd
	SetFlag FLAG_TRADE_OLIVINE_CITY_KRABBY_VOLTORB
	NPCMsg 1
	WaitButton
	CloseMsg
	ReleaseAll
	End

_009B:
	NPCTradeEnd
	NPCMsg 2
	WaitButton
	CloseMsg
	ReleaseAll
	End

_00A8:
	NPCMsg 3
	WaitButton
	CloseMsg
	ReleaseAll
	End

_00B3:
	NPCMsg 4
	WaitButton
	CloseMsg
	ReleaseAll
	End
	.balign 4, 0
