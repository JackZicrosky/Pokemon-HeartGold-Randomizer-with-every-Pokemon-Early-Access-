// Romhack: pret/pokeheartgold scr_seq_0947_T30R0301.s with the trade's Pokemon buffered for the text (tools/rh/gen_rh_trade_scripts.py).
#include "constants/species.h"
#include "rh/pret_compat.h"
#include "constants/item.h"
#include "constants/moves.h"
#include "rh/pret_constants.h"
.include "asm/include/rh_pret_script.inc"
.include "asm/include/flags.inc"
.include "asm/include/vars.inc"
.include "asm/include/soundeffects.inc"
#include "tools/rh/pret/events/event_T30R0301.h"
	

	.data

	ScrDef scr_seq_T30R0301_000
	ScrDef scr_seq_T30R0301_001
	ScrDefEnd

scr_seq_T30R0301_000:
	PlaySE SEQ_SE_DP_SELECT
	LockAll
	FacePlayer
	@ romhack: the trade's (randomized) Pokemon, for the text
	LoadNPCTrade 3
	NPCTradeGetReqSpecies VAR_SPECIAL_x8008
	GetOfferedSpecies VAR_SPECIAL_x8009
	NPCTradeEnd
	BufferSpeciesName 6, VAR_SPECIAL_x8008, 0, 0
	BufferSpeciesName 7, VAR_SPECIAL_x8009, 0, 0
	GoToIfSet FLAG_TRADE_BLACKTHORN_CITY_DRAGONAIR_DODRIO, _00CA
	NPCMsg 0
	TouchscreenMenuHide
	GetMenuChoice VAR_SPECIAL_RESULT
	TouchscreenMenuShow
	Compare VAR_SPECIAL_RESULT, 0
	GoToIfEq _003B
	GoTo _00BF

_003B:
	CloseMsg
	FadeScreen 6, 1, 0, RGB_BLACK
	WaitFade
	ScrCmd_566
	GetPartySelection VAR_SPECIAL_RESULT
	RestoreOverworld
	FadeScreen 6, 1, 1, RGB_BLACK
	WaitFade
	Compare VAR_SPECIAL_RESULT, 255
	GoToIfEq _00BF
	LoadNPCTrade 3
	CopyVar VAR_SPECIAL_x8004, VAR_SPECIAL_RESULT
	GetPartyMonSpecies VAR_SPECIAL_x8004, VAR_SPECIAL_x8005
	Compare VAR_SPECIAL_x8008, SPECIES_DRAGONAIR
	GoToIfNe _rh_skip_gender
	MonGetGender VAR_SPECIAL_x8004, VAR_SPECIAL_x8006
	Compare VAR_SPECIAL_x8006, 0
	GoToIfEq _00B2
_rh_skip_gender:
	NPCTradeGetReqSpecies VAR_SPECIAL_RESULT
	Compare VAR_SPECIAL_x8005, VAR_SPECIAL_RESULT
	GoToIfNe _00B2
	NPCTradeExec VAR_SPECIAL_x8004
	NPCTradeEnd
	SetFlag FLAG_TRADE_BLACKTHORN_CITY_DRAGONAIR_DODRIO
	NPCMsg 1
	WaitButton
	CloseMsg
	ReleaseAll
	End

_00B2:
	NPCTradeEnd
	NPCMsg 2
	WaitButton
	CloseMsg
	ReleaseAll
	End

_00BF:
	NPCMsg 3
	WaitButton
	CloseMsg
	ReleaseAll
	End

_00CA:
	NPCMsg 4
	WaitButton
	CloseMsg
	ReleaseAll
	End

scr_seq_T30R0301_001:
	PlaySE SEQ_SE_DP_SELECT
	LockAll
	FacePlayer
	GetPartyLeadAlive VAR_SPECIAL_x8000
	MonHasRibbon VAR_SPECIAL_RESULT, VAR_SPECIAL_x8000, RIBBON_HOENN_EFFORT
	Compare VAR_SPECIAL_RESULT, 1
	GoToIfEq _0199
	BufferMonSpeciesName 0, VAR_SPECIAL_x8000
	NPCMsg 5
	GetEVTotal VAR_SPECIAL_RESULT, VAR_SPECIAL_x8000
	Compare VAR_SPECIAL_RESULT, 510
	GoToIfLt _0136
	NPCMsg 6
	GiveRibbon VAR_SPECIAL_x8000, RIBBON_HOENN_EFFORT
	PlayFanfare SEQ_ME_ITEM
	BufferPlayersName 1
	NPCMsg 7
	WaitFanfare
	BufferMonSpeciesName 0, VAR_SPECIAL_x8000
	NPCMsg 8
	WaitButton
	CloseMsg
	ReleaseAll
	End

_0136:
	Compare VAR_SPECIAL_RESULT, 506
	GoToIfNe _014C
	NPCMsg 10
	GoTo _0191

_014C:
	Compare VAR_SPECIAL_RESULT, 507
	GoToIfNe _0162
	NPCMsg 11
	GoTo _0191

_0162:
	Compare VAR_SPECIAL_RESULT, 508
	GoToIfNe _0178
	NPCMsg 12
	GoTo _0191

_0178:
	Compare VAR_SPECIAL_RESULT, 509
	GoToIfNe _018E
	NPCMsg 13
	GoTo _0191

_018E:
	NPCMsg 9
_0191:
	WaitButton
	CloseMsg
	ReleaseAll
	End

_0199:
	BufferMonSpeciesName 0, VAR_SPECIAL_x8000
	NPCMsg 14
	WaitButton
	CloseMsg
	ReleaseAll
	End
	.balign 4, 0
