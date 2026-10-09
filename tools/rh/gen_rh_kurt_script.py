#!/usr/bin/env python3
# Romhack: Kurt sells Poke Balls (owner's choice: he has made Balls for a long time and sells his surplus; Apricorns
# only make certain kinds, which is why the player can't get any Ball from them). He offers his shop (special shop
# RH_SPECIAL_BALLS: every kind of Ball) when he has no Apricorn Ball to make, and after taking Apricorns. When the
# shops aren't randomized his one Master Ball is a one-time ¥100,000 offer (in the shop otherwise, as in FireRed).
# From pret's Kurt's house script (tools/rh/pret/scr_kurt.s = scr_seq_0874_T23R0501) and text bank 571.
# Writes data/scr_seq/scr_seq_00874_T23R0501.s and data/text/571.txt.
# Usage: python3 tools/rh/gen_rh_kurt_script.py   (from the repository root; needs tools/msgenc)
import sys

sys.path.insert(0, 'tools/rh')
from gen_rh_trade_scripts import text_bank  # noqa: E402
from import_pret_script import convert  # noqa: E402

BANK = 571
FLAG_RH_BOUGHT_MASTER_BALL = 1031   # formerly unused (pret FLAG_UNK_407, a spare hidden item flag)
FLAG_RH_KURT_SHOP_TOLD = 1032       # pret FLAG_UNK_408
RH_SPECIAL_MART_BASE, RH_SPECIAL_BALLS = 30, 8          # include/rh/rh_data.h
RH_SPECIAL_MASTER_BALL_OFFER = 2                        # include/rh/rh_items.h

TEXTS = [
    # first time
    r'Kurt: I’ve been making Poké Balls\nfor a long, long time.\r'
    r'I’ve made more than I’ll ever use,\nso I sell the extras.\r'
    r'Mind you, an Apricorn can only\nbecome one certain kind of Ball.\f'
    r'That’s why I can’t make you any\nBall you like from your Apricorns.\r'
    r'But my own stock has every kind\nof Ball I’ve ever made!\r',
    r'Kurt: Want to buy some of my\nPoké Balls? {YESNO 0}',
    r'Kurt: I also made a Master Ball.\nJust the one, mind you.\r'
    r'It’s yours for ¥100,000.\nWill you buy it? {YESNO 0}',
    r'Kurt: You don’t have enough money!\r',
    r'Kurt: Come back any time you\nneed Poké Balls.',
    r'Kurt: Treat it with care!\r',
]

SHOP = '''
_rh_kurt_shop:
	GoToIfSet {told}, _rh_kurt_ask
	SetFlag {told}
	NPCMsg {t0}
_rh_kurt_ask:
	NPCMsg {t1}
	TouchscreenMenuHide
	GetMenuChoice VAR_SPECIAL_RESULT
	TouchscreenMenuShow
	Compare VAR_SPECIAL_RESULT, 0
	GoToIfNe _rh_kurt_bye
	SetVar VAR_TEMP_x4005, {offer}
	NopVar490 VAR_TEMP_x4005
	Compare VAR_SPECIAL_RESULT, 0
	GoToIfEq _rh_kurt_mart
	GoToIfSet {bought}, _rh_kurt_mart
	ShowMoneyBox 20, 2
	NPCMsg {t2}
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
	SetFlag {bought}
	SetVar VAR_SPECIAL_x8004, ITEM_MASTER_BALL
	SetVar VAR_SPECIAL_x8005, 1
	CallStd std_obtain_item_verbose
	HideMoneyBox
	NPCMsg {t5}
	GoTo _rh_kurt_mart

_rh_kurt_poor:
	NPCMsg {t3}
_rh_kurt_no_master:
	HideMoneyBox
_rh_kurt_mart:
	CloseMsg
	SpecialMartBuy {mart}
_rh_kurt_bye:
	NPCMsg {t4}
	WaitButton
	Return
'''


def main():
    src = open('tools/rh/pret/scr_kurt.s', encoding='utf-8').read()
    lines = text_bank(BANK)
    t = list(range(len(lines), len(lines) + len(TEXTS)))
    # no Apricorns: after "I make Poke Balls from Apricorns..."
    old = '_020F:\n\tNPCMsg msg_0571_T23R0501_00003\n'
    assert old in src
    src = src.replace(old, old + '\tWaitButton\n\tCall _rh_kurt_shop\n', 1)
    # after taking Apricorns (or the player gave none)
    old = '_0190:\n\tWaitButton\n'
    assert old in src
    src = src.replace(old, old + '\tCall _rh_kurt_shop\n', 1)
    old = '\t.balign 4, 0\n_02B0:'
    assert old in src
    src = src.replace(old, SHOP.format(told=FLAG_RH_KURT_SHOP_TOLD, bought=FLAG_RH_BOUGHT_MASTER_BALL,
                                       offer=RH_SPECIAL_MASTER_BALL_OFFER, mart=RH_SPECIAL_MART_BASE + RH_SPECIAL_BALLS,
                                       t0=t[0], t1=t[1], t2=t[2], t3=t[3], t4=t[4], t5=t[5]) + '\n' + old, 1)
    head = '// Romhack: pret/pokeheartgold scr_seq_0874_T23R0501.s, Kurt sells Poke Balls (tools/rh/gen_rh_kurt_script.py).\n'
    open('data/scr_seq/scr_seq_00874_T23R0501.s', 'w', newline='\n').write(head + convert(src))
    lines = [line if line != '' else ' ' for line in lines] + TEXTS
    open('data/text/%03d.txt' % BANK, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n')
    print('Kurt: messages', t[0], '-', t[-1])


if __name__ == '__main__':
    main()
