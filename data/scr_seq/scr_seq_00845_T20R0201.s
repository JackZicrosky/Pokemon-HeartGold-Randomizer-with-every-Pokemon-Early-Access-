// Romhack: pret/pokeheartgold scr_seq_0845_T20R0201.s, Mom gives the Mega Ring (tools/rh/gen_rh_mom_script.py).
#include "constants/species.h"
#include "rh/pret_compat.h"
#include "constants/item.h"
#include "constants/moves.h"
#include "rh/pret_constants.h"
.include "asm/include/rh_pret_script.inc"
.include "asm/include/flags.inc"
.include "asm/include/vars.inc"
.include "asm/include/soundeffects.inc"
#include "tools/rh/pret/events/event_T20R0201.h"
	

	.data

	ScrDef scr_seq_T20R0201_000
	ScrDef scr_seq_T20R0201_001
	ScrDef scr_seq_T20R0201_002
	ScrDef scr_seq_T20R0201_003
	ScrDef scr_seq_T20R0201_004
	ScrDef scr_seq_T20R0201_005
	ScrDef scr_seq_T20R0201_006
	ScrDefEnd

scr_seq_T20R0201_000:
	ScrCmd_609
	LockAll
	ApplyMovement obj_player, _00A4
	ApplyMovement obj_T20R0201_gsmama, _00B0
	WaitMovement
	CallStd std_play_mom_music
	Wait 30, VAR_SPECIAL_RESULT
	ApplyMovement obj_T20R0201_gsmama, _00B8
	WaitMovement
	BufferPlayersName 0
	GenderMsgBox 0, 1
	SetFlag FLAG_GOT_BAG
	PlayFanfare SEQ_SE_PL_KIRAKIRA
	WaitFanfare
	NPCMsg 2
	SetFlag FLAG_GOT_TRAINER_CARD
	PlayFanfare SEQ_SE_PL_KIRAKIRA
	WaitFanfare
	NPCMsg 3
	SetFlag FLAG_GOT_SAVE_BUTTON
	PlayFanfare SEQ_SE_PL_KIRAKIRA
	WaitFanfare
	NPCMsg 4
	SetFlag FLAG_GOT_OPTIONS_BUTTON
	PlayFanfare SEQ_SE_PL_KIRAKIRA
	WaitFanfare
	NPCMsg 5
	CloseMsg
	Wait 15, VAR_SPECIAL_RESULT
	ApplyMovement obj_T20R0201_gsmama, _00C8
	WaitMovement
	CallStd std_fade_end_mom_music
	SetVar VAR_SCENE_PLAYERS_HOUSE_1F, 1
	ReleaseAll
	End

	.balign 4, 0
_00A4:
	Delay4
	WalkOnSpotNormalSouth
	EndMovement

	.balign 4, 0
_00B0:
	WalkOnSpotNormalNorth
	EndMovement

	.balign 4, 0
_00B8:
	WalkNormalNorth 2
	WalkNormalWest 3
	WalkNormalNorth
	EndMovement

	.balign 4, 0
_00C8:
	WalkOnSpotNormalSouth
	WalkNormalSouth 3
	WalkNormalEast 3
	WalkOnSpotNormalNorth
	EndMovement

scr_seq_T20R0201_006:
	ScrCmd_609
	LockAll
	ApplyMovement obj_player, _00A4
	ApplyMovement obj_T20R0201_gsmama, _00B0
	WaitMovement
	CallStd std_play_mom_music
	Wait 30, VAR_SPECIAL_RESULT
	ApplyMovement obj_T20R0201_gsmama, _00B8
	WaitMovement
	BufferPlayersName 0
	NPCMsg 33
	CloseMsg
	ApplyMovement obj_T20R0201_gsmama, _00C8
	WaitMovement
	CallStd std_fade_end_mom_music
	SetVar VAR_SCENE_PLAYERS_HOUSE_1F, 4
	ReleaseAll
	End

scr_seq_T20R0201_001:
	GoToIfSet FLAG_GAME_CLEAR, _015C
	Compare VAR_SCENE_ELMS_LAB, 4
	GoToIfGe _0205
	GoToIfSet FLAG_GOT_STARTER, _0179
	SimpleNPCMsg 6
	End

_015C:
	HasItem ITEM_S_S__TICKET, 1, VAR_SPECIAL_RESULT
	Compare VAR_SPECIAL_RESULT, 0
	GoToIfEq _01F2
	GoTo _0205
	End

_0179:
	PlaySE SEQ_SE_DP_SELECT
	LockAll
	FacePlayer
	GoToIfSet FLAG_GOT_POKEGEAR, _01D4
	BufferPlayersName 0
	NPCMsg 7
	BufferPlayersName 0
	NPCMsg 8
	SetFlag FLAG_GOT_POKEGEAR
	PlayFanfare SEQ_ME_ITEM
	WaitFanfare
	NPCMsg 9
	NPCMsg 10
	TouchscreenMenuHide
	GetMenuChoice VAR_SPECIAL_RESULT
	TouchscreenMenuShow
	Compare VAR_SPECIAL_RESULT, 0
	GoToIfNe _01C6
	NPCMsg 11
	GoTo _01C9

_01C6:
	NPCMsg 12
_01C9:
	NPCMsg 13
	WaitButton
	@ romhack: the Mega Ring
	NPCMsg 39
	GiveItemNoCheck ITEM_MEGA_RING, 1
	SetFlag 2518
	NPCMsg 40
	WaitButtonOrDpad
	CloseMsg
	ReleaseAll
	End

_01D4:
	HasItem ITEM_MEGA_RING, 1, VAR_SPECIAL_RESULT
	Compare VAR_SPECIAL_RESULT, 1
	GoToIfEq _rh_has_ring
	@ romhack: the Mega Ring
	NPCMsg 39
	GiveItemNoCheck ITEM_MEGA_RING, 1
	SetFlag 2518
	NPCMsg 40
	WaitButton
_rh_has_ring:
	NPCMsg 14
	WaitButton
	CloseMsg
	ReleaseAll
	End

_01DF:
	SimpleNPCMsg 7
	End

_01F2:
	SimpleNPCMsg 34
	End

_0205:
	PlaySE SEQ_SE_DP_SELECT
	LockAll
	FacePlayer
	GoToIfSet FLAG_TALKED_TO_MOM_AFTER_NAMING_RIVAL, _0275
	CheckBadge BADGE_ZEPHYR, VAR_TEMP_x4000
	Compare VAR_TEMP_x4000, 0
	GoToIfNe _0234
	NPCMsg 15
	GoTo _023A

_0234:
	BufferPlayersName 0
	NPCMsg 16
_023A:
	SetFlag FLAG_TALKED_TO_MOM_AFTER_NAMING_RIVAL
	SetVar VAR_SCENE_ROUTE_30_PHONE_CALL, 0
	TouchscreenMenuHide
	GetMenuChoice VAR_SPECIAL_RESULT
	TouchscreenMenuShow
	Compare VAR_SPECIAL_RESULT, 0
	GoToIfNe _0266
	NPCMsg 17
	SetFlag FLAG_SYS_MOMS_SAVINGS
	GoTo _026D

_0266:
	NPCMsg 18
	ClearFlag FLAG_SYS_MOMS_SAVINGS
_026D:
	WaitButton
	CloseMsg
	ReleaseAll
	End

_0275:
	NPCMsg 20
	ScrCmd_795 1, 1
	TouchscreenMenuHide
	MenuInit 1, 1, 0, 1, VAR_SPECIAL_RESULT
	MenuItemAdd 29, 255, 0
	MenuItemAdd 30, 255, 1
	MenuItemAdd 31, 255, 2
	MenuItemAdd 32, 255, 3
	MenuExec
	Switch VAR_SPECIAL_RESULT
	Case 0, _02DF
	Case 1, _0335
	Case 2, _0398
	GoTo _03D1
	End

_02DF:
	BankOrWalletIsFull 1, VAR_SPECIAL_RESULT
	Compare VAR_SPECIAL_RESULT, 1
	GoToIfEq _043A
	CheckBankBalance VAR_SPECIAL_RESULT, 1
	Compare VAR_SPECIAL_RESULT, 0
	GoToIfEq _041C
	BankTransaction 1, VAR_SPECIAL_RESULT
	ScrCmd_796
	TouchscreenMenuShow
	Switch VAR_SPECIAL_RESULT
	Case 0, _03E0
	Case 1, _0411
	ReleaseAll
	End

_0335:
	BankOrWalletIsFull 0, VAR_SPECIAL_RESULT
	Compare VAR_SPECIAL_RESULT, 1
	GoToIfEq _042B
	HasEnoughMoneyVar VAR_SPECIAL_RESULT, 1
	Compare VAR_SPECIAL_RESULT, 0
	GoToIfEq _0389
	BankTransaction 0, VAR_SPECIAL_RESULT
	ScrCmd_796
	TouchscreenMenuShow
	Switch VAR_SPECIAL_RESULT
	Case 0, _03FA
	Case 1, _0411
	ReleaseAll
	End

_0389:
	TouchscreenMenuShow
	ScrCmd_796
	NPCMsg 26
	WaitButton
	CloseMsg
	ReleaseAll
	End

_0398:
	NPCMsg 25
	TouchscreenMenuHide
	GetMenuChoice VAR_SPECIAL_RESULT
	TouchscreenMenuShow
	ScrCmd_796
	Compare VAR_SPECIAL_RESULT, 0
	GoToIfNe _03C2
	BufferPlayersName 0
	NPCMsg 17
	SetFlag FLAG_SYS_MOMS_SAVINGS
	GoTo _03C9

_03C2:
	NPCMsg 18
	ClearFlag FLAG_SYS_MOMS_SAVINGS
_03C9:
	WaitButton
	CloseMsg
	ReleaseAll
	End

_03D1:
	ScrCmd_796
	TouchscreenMenuShow
	NPCMsg 21
	WaitButton
	CloseMsg
	ReleaseAll
	End

_03E0:
	CloseMsg
	Wait 8, VAR_SPECIAL_RESULT
	PlaySE SEQ_SE_GS_OKOZUKAI
	BufferPlayersName 0
	NPCMsg 24
	WaitButton
	CloseMsg
	ReleaseAll
	End

_03FA:
	CloseMsg
	Wait 8, VAR_SPECIAL_RESULT
	PlaySE SEQ_SE_GS_OKOZUKAI
	NPCMsg 23
	WaitButton
	CloseMsg
	ReleaseAll
	End

_0411:
	NPCMsg 21
	WaitButton
	CloseMsg
	ReleaseAll
	End

_041C:
	TouchscreenMenuShow
	ScrCmd_796
	NPCMsg 22
	WaitButton
	CloseMsg
	ReleaseAll
	End

_042B:
	TouchscreenMenuShow
	ScrCmd_796
	NPCMsg 27
	WaitButton
	CloseMsg
	ReleaseAll
	End

_043A:
	TouchscreenMenuShow
	ScrCmd_796
	NPCMsg 28
	WaitButton
	CloseMsg
	ReleaseAll
	End

scr_seq_T20R0201_002:
	PlaySE SEQ_SE_DP_SELECT
	LockAll
	NPCMsg 35
	WaitButton
	CloseMsg
	ReleaseAll
	End

scr_seq_T20R0201_003:
	PlaySE SEQ_SE_DP_SELECT
	LockAll
	NPCMsg 36
	WaitButton
	CloseMsg
	ReleaseAll
	End

scr_seq_T20R0201_004:
	PlaySE SEQ_SE_DP_SELECT
	LockAll
	NPCMsg 37
	WaitButton
	CloseMsg
	ReleaseAll
	End

scr_seq_T20R0201_005:
	PlaySE SEQ_SE_DP_SELECT
	LockAll
	NPCMsg 38
	WaitButton
	CloseMsg
	ReleaseAll
	End
	.balign 4, 0
