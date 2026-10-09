// Romhack: pret/pokeheartgold scr_seq_0874_T23R0501.s, Kurt sells Poke Balls (tools/rh/gen_rh_kurt_script.py).
#include "constants/species.h"
#include "rh/pret_compat.h"
#include "constants/item.h"
#include "constants/moves.h"
#include "rh/pret_constants.h"
.include "asm/include/rh_pret_script.inc"
.include "asm/include/flags.inc"
.include "asm/include/vars.inc"
.include "asm/include/soundeffects.inc"
#include "tools/rh/pret/events/event_T23R0501.h"
	

	.data

	ScrDef scr_seq_T23R0501_000
	ScrDef scr_seq_T23R0501_001
	ScrDef scr_seq_T23R0501_002
	ScrDef scr_seq_T23R0501_003
	ScrDef scr_seq_T23R0501_004
	ScrDef scr_seq_T23R0501_005
	ScrDef scr_seq_T23R0501_006
	ScrDefEnd

scr_seq_T23R0501_000:
	GoToIfSet FLAG_BEAT_AZALEA_ROCKETS, _002F
	SetFlag FLAG_HIDE_AZALEA_SLOWPOKES
	End

_002F:
	ClearFlag FLAG_HIDE_AZALEA_SLOWPOKES
	End

scr_seq_T23R0501_005:
	ScrCmd_609
	LockAll
	SetVar VAR_UNK_4080, 3
	ApplyMovement obj_T23R0501_gantetsu, _02B0
	WaitMovement
	BufferPlayersName 0
	NPCMsg 1
	SetVar VAR_SPECIAL_x8004, 492
	SetVar VAR_SPECIAL_x8005, 1
	CallStd std_obtain_item_verbose
	SetFlag FLAG_UNK_07C
	ReleaseAll
	GoTo _011A
	End

scr_seq_T23R0501_001:
	GoToIfSet FLAG_UNK_07C, _011A
	PlaySE SEQ_SE_DP_SELECT
	LockAll
	FacePlayer
	BufferPlayersName 0
	NPCMsg 0
	WaitABPress
	CloseMsg
	GetPlayerFacing VAR_TEMP_x4001
	Compare VAR_TEMP_x4001, 0
	GoToIfNe _00A7
	GoTo _00C7

_00A1:
	GoTo _00AF

_00A7:
	ApplyMovement obj_T23R0501_gantetsu, _02E0
_00AF:
	WaitMovement
_00B1:
	ReleaseAll
	HidePerson obj_T23R0501_gantetsu
	WaitFanfare
	SetFlag FLAG_UNK_077
	SetFlag FLAG_UNK_19E
	SetFlag FLAG_UNK_19F
	End

_00C7:
	ToggleFollowingPokemonMovement 0
	WaitFollowingPokemonMovement
	FollowingPokemonMovement 56
	GetPersonCoords 253, VAR_TEMP_x4002, VAR_TEMP_x4003
	Compare VAR_TEMP_x4002, 5
	GoToIfNe _00F4
	ApplyMovement obj_T23R0501_gantetsu, _02E8
	GoTo _0108

_00F4:
	PlaySE SEQ_SE_DP_WALL_HIT
	ApplyMovement obj_T23R0501_gantetsu, _02E0
	ApplyMovement obj_player, _02C8
_0108:
	WaitMovement
	WaitFollowingPokemonMovement
	ToggleFollowingPokemonMovement 1
	FollowingPokemonMovement 48
	GoTo _00B1

_011A:
	PlaySE SEQ_SE_DP_SELECT
	LockAll
	FacePlayer
	ScrCmd_735 VAR_SPECIAL_x8000
	Compare VAR_SPECIAL_x8000, 0
	GoToIfNe _01A5
	GetTotalApricornCount VAR_SPECIAL_RESULT
	Compare VAR_SPECIAL_RESULT, 0
	GoToIfEq _020F
	ApplyMovement obj_T23R0501_gantetsu, _02BC
	WaitMovement
	NPCMsg 4
	CloseMsg
	FadeScreen 6, 1, 0, RGB_BLACK
	WaitFade
	ScrCmd_739
	RestoreOverworld
	FadeScreen 6, 1, 1, RGB_BLACK
	WaitFade
	ScrCmd_735 VAR_SPECIAL_RESULT
	Compare VAR_SPECIAL_RESULT, 0
	GoToIfNe _0189
	NPCMsg 6
	GoTo _0190

_0189:
	SetFlag FLAG_DAILY_KURT_MAKING_BALLS
	NPCMsg 5
_0190:
	WaitButton
	Call _rh_kurt_shop
	CloseMsg
	Compare VAR_UNK_4080, 3
	GoToIfEq _022D
	ReleaseAll
	End

_01A5:
	GoToIfSet FLAG_DAILY_KURT_MAKING_BALLS, _0204
	BufferPlayersName 0
	NPCMsg 8
	ScrCmd_737 VAR_SPECIAL_x8004
	HasSpaceForItem VAR_SPECIAL_x8004, VAR_SPECIAL_x8000, VAR_SPECIAL_RESULT
	CopyVar VAR_SPECIAL_x8005, VAR_SPECIAL_x8000
	CallStd std_give_item_verbose
	ClearKurtApricorn
	Compare VAR_UNK_413B, 10
	GoToIfGe _01EE
	AddVar VAR_UNK_413B, 1
	Compare VAR_UNK_413B, 10
	CallIfGe _0227
_01EE:
	NPCMsg 10
	GoTo _021F
	End

_01F9:
	NPCMsg 9
	GoTo _021F
	End

_0204:
	NPCMsg 7
	GoTo _021F
	End

_020F:
	NPCMsg 3
	WaitButton
	Call _rh_kurt_shop
	Compare VAR_UNK_4080, 3
	GoToIfEq _022D
_021F:
	WaitButton
	CloseMsg
	ReleaseAll
	End

_0227:
	SetFlag FLAG_UNK_127
	Return

_022D:
	SetVar VAR_UNK_4080, 4
	ApplyMovement obj_T23R0501_gsbabygirl1, _02FC
	WaitMovement
	ApplyMovement obj_player, _02D4
	WaitMovement
	NPCMsg 15
_024A:
	TouchscreenMenuHide
	GetMenuChoice VAR_SPECIAL_RESULT
	TouchscreenMenuShow
	Compare VAR_SPECIAL_RESULT, 0
	GoToIfEq _026E
	Compare VAR_SPECIAL_RESULT, 1
	GoToIfGe _0289
	End

_026E:
	BufferPlayersName 0
	NPCMsg 16
	PlayFanfare SEQ_ME_POKEGEAR_REGIST
	WaitFanfare
	RegisterGearNumber PHONE_CONTACT_KURT
	NPCMsg 17
	WaitButton
	CloseMsg
	ReleaseAll
	End

_0289:
	NPCMsg 18
	WaitButton
	CloseMsg
	ReleaseAll
	End

_0294:
	ApplyMovement 1, _0304
	WaitMovement
	ReleaseAll
	End

_02A2:
	NPCMsg 19
	GoTo _024A
	End


_rh_kurt_shop:
	GoToIfSet 1032, _rh_kurt_ask
	SetFlag 1032
	NPCMsg 25
_rh_kurt_ask:
	NPCMsg 26
	TouchscreenMenuHide
	GetMenuChoice VAR_SPECIAL_RESULT
	TouchscreenMenuShow
	Compare VAR_SPECIAL_RESULT, 0
	GoToIfNe _rh_kurt_bye
	SetVar VAR_TEMP_x4005, 2
	NopVar490 VAR_TEMP_x4005
	Compare VAR_SPECIAL_RESULT, 0
	GoToIfEq _rh_kurt_mart
	GoToIfSet 1031, _rh_kurt_mart
	ShowMoneyBox 20, 2
	NPCMsg 27
	TouchscreenMenuHide
	GetMenuChoice VAR_SPECIAL_RESULT
	TouchscreenMenuShow
	Compare VAR_SPECIAL_RESULT, 0
	GoToIfNe _rh_kurt_no_master
	HasEnoughMoneyImmediate VAR_SPECIAL_RESULT, 100000
	Compare VAR_SPECIAL_RESULT, 0
	GoToIfEq _rh_kurt_poor
	SubMoneyImmediate 100000
	UpdateMoneyBox
	SetFlag 1031
	SetVar VAR_SPECIAL_x8004, ITEM_MASTER_BALL
	SetVar VAR_SPECIAL_x8005, 1
	CallStd std_obtain_item_verbose
	HideMoneyBox
	NPCMsg 30
	GoTo _rh_kurt_mart

_rh_kurt_poor:
	NPCMsg 28
_rh_kurt_no_master:
	HideMoneyBox
_rh_kurt_mart:
	CloseMsg
	SpecialMartBuy 38
_rh_kurt_bye:
	NPCMsg 29
	WaitButton
	Return

	.balign 4, 0
_02B0:
	Delay16
	WalkOnSpotFastSouth
	EndMovement

	.balign 4, 0
_02BC:
	EmoteExclamationMark
	Delay8
	EndMovement

	.balign 4, 0
_02C8:
	WalkFastWest
	Delay16
	EndMovement

	.balign 4, 0
_02D4:
	WalkOnSpotFastEast
	FaceEast
	EndMovement

	.balign 4, 0
_02E0:
	WalkFastSouth 5
	EndMovement

	.balign 4, 0
_02E8:
	WalkFastWest
	WalkFastSouth 2
	WalkFastEast
	WalkFastSouth 3
	EndMovement

	.balign 4, 0
_02FC:
	WalkNormalWest
	EndMovement

	.balign 4, 0
_0304:
	WalkOnSpotFastEast
	WalkNormalEast
	WalkOnSpotFastWest
	EndMovement

scr_seq_T23R0501_002:
	PlaySE SEQ_SE_DP_SELECT
	LockAll
	FacePlayer
	GoToIfSet FLAG_UNK_077, _0332
	NPCMsg 11
	WaitButton
	CloseMsg
	ReleaseAll
	End

_0332:
	GoToIfSet FLAG_BEAT_AZALEA_ROCKETS, _0348
	NPCMsg 12
	WaitButton
	CloseMsg
	ReleaseAll
	End

_0348:
	CheckRegisteredPhoneNumber PHONE_CONTACT_KURT, VAR_TEMP_x4001
	Compare VAR_TEMP_x4001, 0
	GoToIfEq _02A2
	GoToIfSet FLAG_GAME_CLEAR, _0371
	NPCMsg 13
	WaitButton
	CloseMsg
	ReleaseAll
	End

_0371:
	NPCMsg 14
	WaitButton
	CloseMsg
	ReleaseAll
	End

scr_seq_T23R0501_003:
	PlaySE SEQ_SE_DP_SELECT
	LockAll
	FacePlayer
	PlayCry SPECIES_SLOWPOKE, 0
	NPCMsg 20
	WaitCry
	WaitButton
	CloseMsg
	ReleaseAll
	End

scr_seq_T23R0501_004:
	SimpleNPCMsg 21
	End

scr_seq_T23R0501_006:
	PlaySE SEQ_SE_DP_SELECT
	LockAll
	FacePlayer
	Compare VAR_SCENE_ROCKET_TAKEOVER, 2
	GoToIfNe _03C8
	NPCMsg 24
	GoTo _03F7

_03C8:
	Compare VAR_SCENE_ROCKET_TAKEOVER, 3
	GoToIfNe _03DE
	NPCMsg 24
	GoTo _03F7

_03DE:
	Compare VAR_SCENE_ROCKET_TAKEOVER, 4
	GoToIfNe _03F4
	NPCMsg 24
	GoTo _03F7

_03F4:
	NPCMsg 23
_03F7:
	WaitButton
	CloseMsg
	ReleaseAll
	End
	.balign 4, 0
