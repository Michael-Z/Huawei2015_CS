#include <stdio.h>
#include <stdlib.h>  //
#include <string.h>  //strlen
#include <unistd.h>  //usleep/close
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "Player.h"

/* ������Ϣ��ȡ����� */

typedef int (* Msg_LineReader)(char * pstrLine, int LineLen, void * pData);

typedef struct MSG_READ_INFO_
{
    const char * pMsg;
    int MaxLen;
    int Index;
    void * pData;
} MSG_READ_INFO;

typedef struct MSG_NAME_TYPE_ENTRY_
{
    const char * pStartName;
    const char * pEndtName;
    SER_MSG_TYPES MsgType;
} MSG_NAME_TYPE_ENTRY;

typedef struct PLAYER_SEAT_INFO_
{
    char PlayerID[32];
    PLAYER_SEAT_TYPES Type;
    unsigned int Jetton;
    unsigned int Money;
} PLAYER_SEAT_INFO;

typedef struct MSG_SEAT_INFO_
{
    int PlayerNum;
    PLAYER_SEAT_INFO Players[8];
} MSG_SEAT_INFO;

typedef struct MSG_PUBLIC_CARD_INFO_
{
    int CardNum;
    CARD PublicCards[5];    /* ǰ3�Ź��ƣ���4��ת�ƣ���5�ź��� */
} MSG_PUBLIC_CARD_INFO;

MSG_NAME_TYPE_ENTRY AllMsgTypes[] =
{
    /***************************************************************************
    seat/ eol
    button: pid jetton money eol
    small blind: pid jetton money eol
    (big blind: pid jetton money eol)0-1
    (pid jetton money eol)0-5
    /seat eol
    ****************************************************************************/
    {"seat/ \n" , "/seat \n", SER_MSG_TYPE_seat_info},

    /****************************************************************************
    hold/ eol
    color point eol
    color point eol
    /hold eol
    ****************************************************************************/
    {"blind/ \n" , "/blind \n", SER_MSG_TYPE_blind},

    /****************************************************************************
    hold/ eol
    color point eol
    color point eol
    /hold eol
    ****************************************************************************/
    {"hold/ \n" , "/hold \n", SER_MSG_TYPE_hold_cards},

    /****************************************************************************
    inquire/ eol
    (pid jetton money bet blind | check | call | raise | all_in | fold eol)1-8
    total pot: num eol
    /inquire eol
    ****************************************************************************/
    {"inquire/ \n" , "/inquire \n", SER_MSG_TYPE_inquire},

    /****************************************************************************
    flop/ eol
    color point eol
    color point eol
    color point eol
    /flop eol
    ****************************************************************************/
    {"flop/ \n" , "/flop \n", SER_MSG_TYPE_flop},

    /****************************************************************************
    turn/ eol
    color point eol
    /turn eol
    ****************************************************************************/
    {"turn/ \n" , "/turn \n", SER_MSG_TYPE_turn},

    /****************************************************************************
    river/ eol
    color point eol
    /river eol
    ****************************************************************************/
    {"river/ \n" , "/river \n", SER_MSG_TYPE_river},

    /*****************************************************************************
    showdown/ eol
    common/ eol
    (color point eol)5
    /common eol
    (rank: pid color point color point nut_hand eol)2-8
    /showdown eol
    *****************************************************************************/
    {"showdown/ \n" , "/showdown \n", SER_MSG_TYPE_showdown},

    /****************************************************************************
    pot-win/ eol
    (pid: num eol)0-8
    /pot-win eol
    *****************************************************************************/
    {"pot-win/ \n" , "/pot-win \n", SER_MSG_TYPE_pot_win},

    /*****************************************************************************
    notify/ eol
    (pid jetton money bet blind | check | call | raise | all_in | fold eol)1-8
    total pot: num eol
    /notify eol
    *****************************************************************************/
    {"notify/ \n" , "/notify \n", SER_MSG_TYPE_notify},

    /*****************************************************************************
    game-over eol
    *****************************************************************************/
    {"game-over \n" , NULL, SER_MSG_TYPE_game_over},
};

SER_MSG_TYPES Msg_GetMsgTypeByMsgName(const char * pMsgName)
{
    int Count = sizeof(AllMsgTypes)/sizeof(MSG_NAME_TYPE_ENTRY);
    MSG_NAME_TYPE_ENTRY * pEntry = NULL;
    int index = 0;
    for (index = 0; index < Count; index ++)
    {
        pEntry = &AllMsgTypes[index];
        if (memcmp((void *)pMsgName, (void *)pEntry->pStartName, strlen(pEntry->pStartName)) == 0)
        {
            return pEntry->MsgType;
        }
    }
    return SER_MSG_TYPE_none;
}

const char * Msg_GetMsgNameByType(SER_MSG_TYPES Type)
{
    int Count = sizeof(AllMsgTypes)/sizeof(MSG_NAME_TYPE_ENTRY);
    MSG_NAME_TYPE_ENTRY * pEntry = NULL;
    int index = 0;
    for (index = 0; index < Count; index ++)
    {
        pEntry = &AllMsgTypes[index];
        if (Type == pEntry->MsgType)
        {
            return pEntry->pStartName;
        }
    }
    return "";
}

/*
    ����Ϣ�ж�ȡһ�У����û�ж�ȡ�����ͷ���-1�����򷵻���һ�е���ʼλ�á�
*/
int Msg_ReadLine(MSG_READ_INFO * pInfo, char OutLine[256])
{
    int index = pInfo->Index;
    int ReadNum = 0;

    /* �ҵ�һ�еĽ���\n */
    while (index++ < pInfo->MaxLen)
    {
        if (pInfo->pMsg[index] == '\n')
        {
            /* �������� */
            ReadNum = index - pInfo->Index + 1;
            memcpy((void *)OutLine, (void *)(pInfo->pMsg + pInfo->Index), ReadNum);
            break;
        }
    }
    return ReadNum;
}

/* ���ö�ȡƫ�� */
void Msg_SetOffset(MSG_READ_INFO * pInfo, int Index)
{
    Index = (Index < 0) ? 0 : Index;
    Index = (Index > pInfo->MaxLen) ? pInfo->MaxLen : Index;
    pInfo->Index = Index;
    return;
}

/* ��ȡ��Ϣ��һ�У�ȡ����Ϣ���� */
SER_MSG_TYPES Msg_GetMsgType(const char * pMsg, int MaxLen)
{
    char LineBuffer[256] = {0};
    int index = 0;
    MSG_READ_INFO MsgInfo = {0};

    MsgInfo.pMsg = pMsg;
    MsgInfo.MaxLen = MaxLen;
    index = Msg_ReadLine(&MsgInfo, LineBuffer);
    //TRACE("Msg_ReadLine:%d:[%s];\n", index, LineBuffer);
    if (index < 0)
    {
        return SER_MSG_TYPE_none;
    }
    return Msg_GetMsgTypeByMsgName(LineBuffer);
}

/* button: 1002 2000 8000 \nsmall blind: 1003 2000 8000 \nbig blind: 1001 2000 8000 \n1004 2000 8000\n */
void Msg_ReadSeatInfo_PlayerSeat(MSG_SEAT_INFO * pSetInfo, char LineBuffer[256])
{
    #define BUTTON      "button: "
    #define BUTTON_FMT  "button: %s %d %d"
    #define SMALL       "small blind: "
    #define SMALL_FMT   "small blind: %s %d %d"
    #define BIG         "big blind: "
    #define BIG_FMT     "big blind: %s %d %d"
    #define PLAYER_FMT  "%s %d %d"
    const char * pFmt = NULL;
    PLAYER_SEAT_INFO * pPlayerSeat = NULL;
    PLAYER_SEAT_INFO PlayerSeat = {0};
    int readnum = 0;

    if (memcmp((void *)LineBuffer, (void *)BUTTON, sizeof(BUTTON) - 1) == 0)
    {
        pFmt = BUTTON_FMT;
        pPlayerSeat = &pSetInfo->Players[0];
        pPlayerSeat->Type = PLAYER_SEAT_TYPES_button;
        goto SCAN_PLAYER;
    }

    if (memcmp((void *)LineBuffer, (void *)SMALL, sizeof(SMALL) - 1) == 0)
    {
        pFmt = SMALL_FMT;
        pPlayerSeat = &pSetInfo->Players[1];
        pPlayerSeat->Type = PLAYER_SEAT_TYPES_small_blind;
        goto SCAN_PLAYER;
    }

    if (memcmp((void *)LineBuffer, (void *)BIG, sizeof(BIG) - 1) == 0)
    {
        pFmt = BIG_FMT;
        pPlayerSeat = &pSetInfo->Players[2];
        pPlayerSeat->Type = PLAYER_SEAT_TYPES_big_blind;
        goto SCAN_PLAYER;
    }

    pFmt = PLAYER_FMT;
    pPlayerSeat = &pSetInfo->Players[pSetInfo->PlayerNum];
    pPlayerSeat->Type = PLAYER_SEAT_TYPES_none;

SCAN_PLAYER:
    readnum = sscanf(LineBuffer, pFmt, PlayerSeat.PlayerID, &PlayerSeat.Jetton, &PlayerSeat.Money);
    if (readnum == 3)
    {
        memcpy(pPlayerSeat->PlayerID, PlayerSeat.PlayerID, sizeof(PlayerSeat.PlayerID));
        pPlayerSeat->Jetton = PlayerSeat.Jetton;
        pPlayerSeat->Money = PlayerSeat.Money;
        pSetInfo->PlayerNum ++;
    }
    return;
}

/* ��ȡseat������Ϣ */
int Msg_ReadSeatInfo(MSG_READ_INFO *pMsgInfo, MSG_SEAT_INFO * pSetInfo)
{
    char LineBuffer[256] = {0};
    int ReadNum = 0;
    int Line = 0;

    /* �ȶ�ȡ��Ϣ���ͣ���һ�� */
    ReadNum = Msg_ReadLine(pMsgInfo, LineBuffer);
    Line ++;
    //printf("%s\r\n", LineBuffer);
    while (ReadNum > 0)
    {
        memset(LineBuffer, 0, sizeof(LineBuffer));
        Msg_SetOffset(pMsgInfo, pMsgInfo->Index + ReadNum);
        ReadNum = Msg_ReadLine(pMsgInfo, LineBuffer);
        //printf("%s\r\n", LineBuffer);
        Line ++;
        //
        Msg_ReadSeatInfo_PlayerSeat(pSetInfo, LineBuffer);
    }
    return 0;
}

void Msg_ReadPublicCardsInfo_Card(MSG_PUBLIC_CARD_INFO * pCards, char LineBuffer[256])
{
    #define SPADES      "SPADES "
    #define HEARTS      "HEARTS "
    #define CLUBS       "CLUBS "
    #define DIAMONDS    "DIAMONDS "
    CARD * pCard = NULL;

    if (memcmp((void *)LineBuffer, (void *)SPADES, sizeof(SPADES) - 1) == 0)
    {
        pCard = &pCards->PublicCards[pCards->CardNum];
        pCard->Color = CARD_COLOR_SPADES;
        sscanf(LineBuffer + sizeof(SPADES) - 1, "%d", (int *)&pCard->Point);
        pCards->CardNum ++;
        return;
    }
    if (memcmp((void *)LineBuffer, (void *)HEARTS, sizeof(HEARTS) - 1) == 0)
    {
        pCard = &pCards->PublicCards[pCards->CardNum];
        pCard->Color = CARD_COLOR_HEARTS;
        sscanf(LineBuffer + sizeof(HEARTS) - 1, "%d", (int *)&pCard->Point);
        pCards->CardNum ++;
        return;
    }
    if (memcmp((void *)LineBuffer, (void *)CLUBS, sizeof(CLUBS) - 1) == 0)
    {
        pCard = &pCards->PublicCards[pCards->CardNum];
        pCard->Color = CARD_COLOR_CLUBS;
        sscanf(LineBuffer + sizeof(CLUBS) - 1, "%d", (int *)&pCard->Point);
        pCards->CardNum ++;
        return;
    }
    if (memcmp((void *)LineBuffer, (void *)DIAMONDS, sizeof(DIAMONDS) - 1) == 0)
    {
        pCard = &pCards->PublicCards[pCards->CardNum];
        pCard->Color = CARD_COLOR_DIAMONDS;
        sscanf(LineBuffer + sizeof(DIAMONDS) - 1, "%d", (int *)&pCard->Point);
        pCards->CardNum ++;
        return;
    }
    return;
}

/* ��ȡ�����Ƶ���Ϣ */
int Msg_ReadPublicCardsInfo(MSG_READ_INFO *pMsgInfo, MSG_PUBLIC_CARD_INFO * pCards)
{
    char LineBuffer[256] = {0};
    int ReadNum = 0;
    int Line = 0;

    /* �ȶ�ȡ��Ϣ���ͣ���һ�� */
    ReadNum = Msg_ReadLine(pMsgInfo, LineBuffer);
    Line ++;
    printf("%s\r\n", LineBuffer);
    while (ReadNum > 0)
    {
        memset(LineBuffer, 0, sizeof(LineBuffer));
        Msg_SetOffset(pMsgInfo, pMsgInfo->Index + ReadNum);
        ReadNum = Msg_ReadLine(pMsgInfo, LineBuffer);
        printf("%s\r\n", LineBuffer);
        Line ++;
        //
        Msg_ReadPublicCardsInfo_Card(pCards, LineBuffer);
    }
    return 0;
}

SER_MSG_TYPES Msg_Read(char * pMsg, int MaxLen, void * pData, Msg_LineReader LineReader)
{
    return SER_MSG_TYPE_none;
}
/* ������λ���ͷ�����λ��Ϣ */
const char * Msg_GetPlayType(PLAYER_SEAT_INFO *pPlayerInfo)
{
    switch (pPlayerInfo->Type)
    {
        default:
        case PLAYER_SEAT_TYPES_none: return "common";
        case PLAYER_SEAT_TYPES_small_blind: return "small blind";
        case PLAYER_SEAT_TYPES_big_blind: return "big blind";
        case PLAYER_SEAT_TYPES_button: return "Button";
    }
}

const char * Msg_GetCardColor(CARD * pCard)
{
    switch (pCard->Color)
    {
        default:
        case CARD_COLOR_CLUBS: return "CLUBS";
        case CARD_COLOR_DIAMONDS: return "DIAMONDS";
        case CARD_COLOR_HEARTS: return "HEARTS";
        case CARD_COLOR_SPADES: return "SPADES";
    }
}

const char  * TestMessages[] =
{
    "seat/ \nbutton: 1002 2000 8000 \nsmall blind: 1003 2000 8000 \nbig blind: 1001 2000 8000\n1004 2000 8000 /seat \n",
    "blind/ \n1003: 50 \n1001: 100 \n/blind \n",
    "hold/ \nCLUBS 10 \nDIAMONDS 8 \n/hold \n",
    "inquire/ \n1001 1900 8000 100 blind \n1003 1950 8000 50 blind \ntotal pot: 150 \n/inquire \n",
    "flop/ \nSPADES 2 \nSPADES 3 \nDIAMONDS 4 \n/flop \n",
    "turn/ \nHEARTS 10 \n/turn \n",
    "river/ \nSPADES 4 \n/river \n",
    "showdown/ \ncommon/ SPADES 2 \nSPADES 3 \nDIAMONDS 4 \nHEARTS 10 \nSPADES 4 \n/common \n1: 1001 SPADES 10 DIAMONDS 10 FULL_HOUSE \n2: 1002 SPADES 7 DIAMONDS Q ONE_PAIR \n/showdown \n",
    "pot-win/ \n1001: 250 \n/pot-win \n",
    "game-over \n"
};

void Debug_PrintSeatInfo(MSG_SEAT_INFO * pSeatInfo)
{
    int index = 0;
    PLAYER_SEAT_INFO * pPlayer = NULL;
    for (index = 0; index < pSeatInfo ->PlayerNum; index ++)
    {
        pPlayer = &pSeatInfo->Players[index];
        printf("%s ID:%s Jetton:%d Money:%d\n", Msg_GetPlayType(pPlayer), pPlayer->PlayerID, pPlayer->Jetton, pPlayer->Money);
    }
    return;
}

void Debug_PrintPublicChardInfo(MSG_PUBLIC_CARD_INFO * pPublicCard)
{
    int index = 0;
    CARD * pCard = NULL;
    for (index = 0; index < pPublicCard->CardNum; index ++)
    {
        pCard = &pPublicCard->PublicCards[index];
        printf("card_%d %s %d\n", index, Msg_GetCardColor(pCard), pCard->Point);
    }
    return;
}

int main(int argc, char * argv[])
{
    int count = sizeof(TestMessages)/sizeof(const char  *);
    SER_MSG_TYPES type = SER_MSG_TYPE_none;
    int index = 0;
    MSG_READ_INFO MsgInfo = {0};
    MSG_SEAT_INFO SeatInfo = {0};
    MSG_PUBLIC_CARD_INFO PublicCards = {0};

    for (index = 0; index < count; index++)
    {
        type = Msg_GetMsgType(TestMessages[index], strlen(TestMessages[index]) + 1);
        printf("Msg %d:%s", type, Msg_GetMsgNameByType(type));
    }

    /* ����seat��Ϣ���� */
    printf("test seat info:\r\n");
    MsgInfo.pMsg = TestMessages[0];
    MsgInfo.MaxLen = strlen(TestMessages[0]);
    if(Msg_ReadSeatInfo(&MsgInfo, &SeatInfo) == 0)
    {
        Debug_PrintSeatInfo(&SeatInfo);
    }

    /* ����flop��turn���Լ�rirver�Ƶ���Ϣ */
    printf("test card info:\r\n");
    memset(&MsgInfo, 0, sizeof(MsgInfo));
    MsgInfo.pMsg = TestMessages[4];
    MsgInfo.MaxLen = strlen(TestMessages[4]);
    Msg_ReadPublicCardsInfo(&MsgInfo, &PublicCards);

    memset(&MsgInfo, 0, sizeof(MsgInfo));
    MsgInfo.pMsg = TestMessages[5];
    MsgInfo.MaxLen = strlen(TestMessages[5]);
    Msg_ReadPublicCardsInfo(&MsgInfo, &PublicCards);

    memset(&MsgInfo, 0, sizeof(MsgInfo));
    MsgInfo.pMsg = TestMessages[6];
    MsgInfo.MaxLen = strlen(TestMessages[6]);
    Msg_ReadPublicCardsInfo(&MsgInfo, &PublicCards);

    Debug_PrintPublicChardInfo(&PublicCards);

    return 0;
}


