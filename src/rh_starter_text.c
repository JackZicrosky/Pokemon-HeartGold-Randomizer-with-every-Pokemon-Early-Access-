// Romhack: Prof. Elm's starter screen names the Pokemon really in each ball. Replaces the starter app's message
// printer (overlay 61, printMsgOnWinEx at 021E6D78; pret choose_starter_app.c). Text bank 190 (data/text/190.txt)
// has '#' where the species name goes and '~' for its type.
#include "../include/types.h"
#include "../include/message.h"
#include "../include/pokemon.h"
#include "../include/sprite.h"
#include "../include/window.h"
#include "../include/rh/rh_coretable.h"

#define CHAR_SPECIES 0x01C0      // '#'
#define CHAR_TYPE    0x01C3      // '~'
#define CHAR_SLASH   0x01B1      // '/'
#define CHAR_EOS     0xFFFF
#define NARC_MSG     27
#define BANK_STARTER 190
#define BANK_TYPES   735

static u32 Append(u16 *dst, u32 n, u32 max, const u16 *src)
{
    while (*src != CHAR_EOS && n < max)
        dst[n++] = *src++;
    return n;
}

static u32 AppendType(u16 *dst, u32 n, u32 max, u32 type, u32 heapID)
{
    MsgData *types = NewMsgDataFromNarc(MSGDATA_LOAD_DIRECT, NARC_MSG, BANK_TYPES, heapID);
    String *s;
    if (types == NULL)
        return n;
    s = NewString_ReadMsgData(types, type);
    if (s != NULL)
    {
        u32 i;
        for (i = 0; i < s->size && n < max; i++)
            dst[n++] = s->data[i];
        String_Delete(s);
    }
    DestroyMsgData(types);
    return n;
}

// The starter in ball "slot": its name for '#', its type(s) for '~'.
static String *FillStarterText(String *in, u32 slot, u32 heapID)
{
    u16 name[16], buf[160], species, form;
    u32 i, n = 0, sp, t1, t2;
    String *out;
    sp = RHCT_StarterForSlot(slot);
    RHCT_SpeciesKey(sp, &species, &form);
    GetSpeciesNameIntoArray(species, heapID, name);
    name[NELEMS(name) - 1] = CHAR_EOS;
    t1 = PokePersonalParaGet(sp, PERSONAL_TYPE_1);
    t2 = PokePersonalParaGet(sp, PERSONAL_TYPE_2);
    for (i = 0; i < in->size && n < NELEMS(buf) - 1; i++)
    {
        if (in->data[i] == CHAR_SPECIES)
            n = Append(buf, n, NELEMS(buf) - 1, name);
        else if (in->data[i] == CHAR_TYPE)
        {
            n = AppendType(buf, n, NELEMS(buf) - 1, t1, heapID);
            if (t2 != t1 && n < NELEMS(buf) - 1)
            {
                buf[n++] = CHAR_SLASH;
                n = AppendType(buf, n, NELEMS(buf) - 1, t2, heapID);
            }
        }
        else
            buf[n++] = in->data[i];
    }
    buf[n] = CHAR_EOS;
    out = String_New(n + 1, heapID);
    if (out == NULL)
        return in;
    for (i = 0; i <= n; i++)
        out->data[i] = buf[i];
    out->size = n;
    String_Delete(in);
    return out;
}

u8 RH_StarterPrintMsg(void *window, u32 heapID, BOOL makeFrame, s32 msgBank, int msgno, u32 color, u32 speed, String **out)
{
    MsgData *msgData = NewMsgDataFromNarc(MSGDATA_LOAD_DIRECT, NARC_MSG, msgBank, heapID);
    u8 ret;
    *out = NewString_ReadMsgData(msgData, msgno);
    if (msgBank == BANK_STARTER && msgno >= 1 && msgno <= 6 && *out != NULL)
        *out = FillStarterText(*out, (msgno - 1) % 3, heapID);
    FillWindowPixelBuffer(window, color);
    ret = AddTextPrinterParameterizedWithColor(window, 1, *out, 0, 0, speed, color, NULL);
    if (makeFrame)
        DrawFrameAndWindow2(window, FALSE, 0x200, 0);
    else
        CopyWindowToVram(window);
    DestroyMsgData(msgData);
    return ret;
}
