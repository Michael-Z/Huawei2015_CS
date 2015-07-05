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

void Debug_PrintChardInfo(CARD * pCard, int CardNum);
void Msg_LinerReader_Seat(char Buffer[256], RoundInfo * pArg);
void Msg_LinerReader_Blind(char Buffer[256], RoundInfo * pArg);
void Msg_LinerReader_Hold(char Buffer[256], RoundInfo * pArg);
void Msg_LinerReader_Inquire(char Buffer[256], RoundInfo * pArg);
void Msg_LinerReader_Flop(char Buffer[256], RoundInfo * pArg);
void Msg_LinerReader_Turn(char Buffer[256], RoundInfo * pArg);
void Msg_LinerReader_River(char Buffer[256], RoundInfo * pArg);
void Msg_LinerReader_ShowDown(char Buffer[256], RoundInfo * pArg);
void Msg_LinerReader_Notify(char Buffer[256], RoundInfo * pArg);
void Msg_LinerReader_GameOver(char Buffer[256], RoundInfo * pArg);
void Msg_LinerReader_PotWin(char Buffer[256], RoundInfo * pArg);
void Msg_ReadSeatInfo_EndAction(RoundInfo *pRound);
void Msg_Inquire_Action(RoundInfo * pRound);
void STG_Inquire_Action(RoundInfo * pRound);
void STG_SaveRoundData(RoundInfo * pRound);
void ExitGame(RoundInfo * pRound);

/*
typedef enum SER_MSG_TYPES_
{
    SER_MSG_TYPE_none = 0,
    // ��������player����Ϣ
    SER_MSG_TYPE_seat_info,
    SER_MSG_TYPE_blind,
    SER_MSG_TYPE_hold_cards,
    SER_MSG_TYPE_inquire,
    //
    SER_MSG_TYPE_flop,
    SER_MSG_TYPE_turn,
    SER_MSG_TYPE_river,
    SER_MSG_TYPE_showdown,
    SER_MSG_TYPE_pot_win,
    SER_MSG_TYPE_notify,
    SER_MSG_TYPE_game_over,
    //
} SER_MSG_TYPES;
*/
MSG_NAME_TYPE_ENTRY AllMsgTypes[] =
{
    {"" , "", 0, SER_MSG_TYPE_none, NULL},

    /***************************************************************************
    seat/ eol
    button: pid jetton money eol
    small blind: pid jetton money eol
    (big blind: pid jetton money eol)0-1
    (pid jetton money eol)0-5
    /seat eol
    ****************************************************************************/
    {"seat/ \n" , "/seat \n", sizeof("seat/ \n") - 1,
    SER_MSG_TYPE_seat_info, Msg_LinerReader_Seat, Msg_ReadSeatInfo_EndAction},

    /****************************************************************************
    blind/ eol
    (pid: bet eol)1-2
    /blind eol
    ****************************************************************************/
    {"blind/ \n" , "/blind \n", sizeof("blind/ \n") - 1,
    SER_MSG_TYPE_blind, Msg_LinerReader_Blind, NULL},

    /****************************************************************************
    hold/ eol
    color point eol
    color point eol
    /hold eol
    ****************************************************************************/
    {"hold/ \n" , "/hold \n", sizeof("hold/ \n") - 1,
    SER_MSG_TYPE_hold_cards, Msg_LinerReader_Hold, NULL},

    /****************************************************************************
    inquire/ eol
    (pid jetton money bet blind | check | call | raise | all_in | fold eol)1-8
    total pot: num eol
    /inquire eol
    ****************************************************************************/
    {"inquire/ \n" , "/inquire \n", sizeof("inquire/ \n") - 1,
    SER_MSG_TYPE_inquire, Msg_LinerReader_Inquire, Msg_Inquire_Action},

    /****************************************************************************
    flop/ eol
    color point eol
    color point eol
    color point eol
    /flop eol
    ****************************************************************************/
    {"flop/ \n" , "/flop \n", sizeof("flop/ \n") - 1,
    SER_MSG_TYPE_flop, Msg_LinerReader_Flop, NULL},

    /****************************************************************************
    turn/ eol
    color point eol
    /turn eol
    ****************************************************************************/
    {"turn/ \n" , "/turn \n", sizeof("turn/ \n") - 1,
    SER_MSG_TYPE_turn, Msg_LinerReader_Turn, NULL},

    /****************************************************************************
    river/ eol
    color point eol
    /river eol
    ****************************************************************************/
    {"river/ \n" , "/river \n", sizeof("river/ \n") - 1,
    SER_MSG_TYPE_river, Msg_LinerReader_River, NULL},

    /*****************************************************************************
    showdown/ eol
    common/ eol
    (color point eol)5
    /common eol
    (rank: pid color point color point nut_hand eol)2-8
    /showdown eol
    *****************************************************************************/
    {"showdown/ \n" , "/showdown \n", sizeof("showdown/ \n") - 1,
    SER_MSG_TYPE_showdown, Msg_LinerReader_ShowDown, NULL},

    /****************************************************************************
    pot-win/ eol
    (pid: num eol)0-8
    /pot-win eol
    *****************************************************************************/
    {"pot-win/ \n" , "/pot-win \n", sizeof("seat/ \n") - 1,
    SER_MSG_TYPE_pot_win, Msg_LinerReader_PotWin, STG_SaveRoundData},

    /*****************************************************************************
    notify/ eol
    (pid jetton money bet blind | check | call | raise | all_in | fold eol)1-8
    total pot: num eol
    /notify eol
    *****************************************************************************/
    {"notify/ \n" , "/notify \n", sizeof("notify/ \n") - 1,
    SER_MSG_TYPE_notify, Msg_LinerReader_Notify, NULL},

    /*****************************************************************************
    game-over eol
    *****************************************************************************/
    {"game-over \n" , NULL, 0, SER_MSG_TYPE_game_over,
    Msg_LinerReader_GameOver, ExitGame},
};

SER_MSG_TYPES Msg_GetMsgTypeByMsgName(const char * pMsgName)
{
    int Count = sizeof(AllMsgTypes)/sizeof(MSG_NAME_TYPE_ENTRY);
    MSG_NAME_TYPE_ENTRY * pEntry = NULL;
    int index = 0;
    for (index = 1; index < Count; index ++)
    {
        pEntry = &AllMsgTypes[index];
        if (memcmp((void *)pMsgName, (void *)pEntry->pStartName, pEntry->NameLen) == 0)
        {
            //TRACE("[%s %s]\r\n", pMsgName, pEntry->pStartName);
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

    memset(OutLine, 0, 256);

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

/* button: 1002 2000 8000 \nsmall blind: 1003 2000 8000 \nbig blind: 1001 2000 8000 \n1004 2000 8000\n */
void Msg_ReadSeatInfo_PlayerSeat(void * pObj, char LineBuffer[256])
{
    #define BUTTON      "button: "
    #define BUTTON_FMT  "button: %d %d %d"
    #define SMALL       "small blind: "
    #define SMALL_FMT   "small blind: %d %d %d"
    #define BIG         "big blind: "
    #define BIG_FMT     "big blind: %d %d %d"
    #define PLAYER_FMT  "%d %d %d"
    MSG_SEAT_INFO * pSetInfo = (MSG_SEAT_INFO *)pObj;
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
    readnum = sscanf(LineBuffer, pFmt, &PlayerSeat.PlayerID, &PlayerSeat.Jetton, &PlayerSeat.Money);
    if (readnum != 3)
    {
        TRACE("sscanf error [%s];\r\n", LineBuffer);
        return ;
    }
    //memcpy(pPlayerSeat->PlayerID, PlayerSeat.PlayerID, sizeof(PlayerSeat.PlayerID));
    //pPlayerSeat->Jetton = PlayerSeat.Jetton;
    //pPlayerSeat->Money = PlayerSeat.Money;
    *pPlayerSeat = PlayerSeat;
    pSetInfo->PlayerNum ++;
    return;
}

void Msg_ReadSeatInfo_EndAction(RoundInfo *pRound)
{
    //pRound->SeatInfo.PlayerNum --;
    //printf("Read seat num:%d\r\n", pRound->SeatInfo.PlayerNum);
}

CARD_POINT GetCardPoint(char CardPoint[4])
{
    switch (CardPoint[0])
    {
        case '2' : return CARD_POINTT_2;
        case '3' : return CARD_POINTT_3;
        case '4' : return CARD_POINTT_4;
        case '5' : return CARD_POINTT_5;
        case '6' : return CARD_POINTT_6;
        case '7' : return CARD_POINTT_7;
        case '8' : return CARD_POINTT_8;
        case '9' : return CARD_POINTT_9;
        case '1' : return CARD_POINTT_10;  /* �����1�Ļ�����Ȼ��10 */
        case 'J' : return CARD_POINTT_J;
        case 'Q' : return CARD_POINTT_Q;
        case 'K' : return CARD_POINTT_K;
        case 'A' : return CARD_POINTT_A;
    }
}

const char * GetCardPointName(CARD_POINT point)
{
    switch (point)
    {
         case CARD_POINTT_2: return "2" ;
         case CARD_POINTT_3: return "3" ;
         case CARD_POINTT_4: return "4" ;
         case CARD_POINTT_5: return "5" ;
         case CARD_POINTT_6: return "6" ;
         case CARD_POINTT_7: return "7" ;
         case CARD_POINTT_8: return "8" ;
         case CARD_POINTT_9: return "9" ;
         case CARD_POINTT_10:return "10";
         case CARD_POINTT_J: return "J" ;
         case CARD_POINTT_Q: return "Q" ;
         case CARD_POINTT_K: return "K" ;
         case CARD_POINTT_A: return "A" ;
         default:return "err";
    }
}

bool Msg_ReadCardsInfo_OneCard(const char LineBuffer[256], CARD * pCard)
{
    #define SPADES      "SPADES "
    #define HEARTS      "HEARTS "
    #define CLUBS       "CLUBS "
    #define DIAMONDS    "DIAMONDS "
    char Point[4] = {0};
    int scan_num = 0;
    CARD_COLOR Color = CARD_COLOR_Unknow;

    //TRACE("%s",LineBuffer);

    if (memcmp((void *)LineBuffer, (void *)SPADES, sizeof(SPADES) - 1) == 0)
    {
        Color = CARD_COLOR_SPADES;
        scan_num = sscanf(LineBuffer + sizeof(SPADES) - 1, "%s", Point);
        goto OUT;
    }
    if (memcmp((void *)LineBuffer, (void *)HEARTS, sizeof(HEARTS) - 1) == 0)
    {
        Color = CARD_COLOR_HEARTS;
        scan_num = sscanf(LineBuffer + sizeof(HEARTS) - 1, "%s", Point);
        goto OUT;
    }
    if (memcmp((void *)LineBuffer, (void *)CLUBS, sizeof(CLUBS) - 1) == 0)
    {
        Color = CARD_COLOR_CLUBS;
        scan_num = sscanf(LineBuffer + sizeof(CLUBS) - 1, "%s", Point);
        goto OUT;
    }
    if (memcmp((void *)LineBuffer, (void *)DIAMONDS, sizeof(DIAMONDS) - 1) == 0)
    {
        Color = CARD_COLOR_DIAMONDS;
        scan_num = sscanf(LineBuffer + sizeof(DIAMONDS) - 1, "%s", Point);
    }

OUT:
    if (scan_num != 1)
    {
        TRACE("sscanf error [%s];\r\n", LineBuffer);
        return false;
    }
    pCard->Color = Color;
    pCard->Point = GetCardPoint(Point);
    return true;
}

void Msg_ReadCardsInfo_Card(void * pObj, char LineBuffer[256])
{
    MSG_CARD_INFO * pCards = (MSG_CARD_INFO * )pObj;

    if(Msg_ReadCardsInfo_OneCard(LineBuffer, &pCards->Cards[pCards->CardNum]))
    {
        pCards->CardNum ++;
    }
    return;
}

/* ��ȡPot win����blind��Ϣ��������Ϣ�ṹһ����������һ��������ȡ */
/* "pot-win/ \n1001: 250 \n/pot-win \n", */
void Msg_ReadPlayJettonInfo(PLAYER_JETTION_INFO * pPlayerJetton, char LineBuffer[256])
{
    int ReadNum = 0;
    int scan_num = 0;
    //char PID[32] = {0};
    int PID =0;
    int Jetton = 0;

    scan_num = sscanf(LineBuffer, "%d: %d", &PID, &Jetton);
    if (scan_num != 2)
    {
        TRACE("sscanf error [%s];\r\n", LineBuffer);
        return;
    }
    //memcpy(pPotWinInfo->PlayerID, PID, 32);
    pPlayerJetton->PlayerID = PID;
    pPlayerJetton->Jetton = Jetton;
    //printf("Player %d %d.\r\n", PID, Jetton);
    return;
}

void Msg_ReadBlindInfo_Ex(char LineBuffer[256], MSG_BLIND_INFO * pBlindInfo)
{
    Msg_ReadPlayJettonInfo(&pBlindInfo->BlindPlayers[pBlindInfo->BlindNum], LineBuffer);
    pBlindInfo->BlindNum = (pBlindInfo->BlindNum + 1) % 2;
    return;
}

const char * GetCardColorName(CARD * pCard)
{
    switch (pCard->Color)
    {
        default: return "err";
        case CARD_COLOR_CLUBS:      return "CLUBS";
        case CARD_COLOR_DIAMONDS:   return "DIAMONDS";
        case CARD_COLOR_HEARTS:     return "HEARTS";
        case CARD_COLOR_SPADES:     return "SPADES";
    }
}

CARD_COLOR GetCardColor(const char *pColor)
{
    if (strcmp("CLUBS", pColor) == 0) return CARD_COLOR_CLUBS;
    if (strcmp("DIAMONDS", pColor) == 0) return CARD_COLOR_DIAMONDS;
    if (strcmp("HEARTS", pColor) == 0) return CARD_COLOR_HEARTS;
    if (strcmp("SPADES", pColor) == 0) return CARD_COLOR_SPADES;
    return CARD_COLOR_Unknow;
}

bool Msg_ReadPlayerCardInfo(MSG_SHOWDWON_PLAYER_CARD * pPlayerCard, char LineBuffer[256])
{
    char CardPoint1[4] = {0};
    char CardColor1[32] = {0};
    char CardPoint2[4] = {0};
    char CardColor2[32] = {0};
    char CardType[32] = {0};
    int index = 0;

    int scan_num = 0;

    //printf("%s", LineBuffer);
    /* "1: 1001 SPADES 10 DIAMONDS 10 FULL_HOUSE", */
    scan_num = sscanf(LineBuffer, "%d: %d %s %s %s %s %s",
               &index,
               &pPlayerCard->PlayerID,
               CardColor1,CardPoint1,
               CardColor2,CardPoint2,
               CardType);
    if (scan_num != 7)
    {
        TRACE("sscanf error [%s];\r\n", LineBuffer);
        return false;
    }
    pPlayerCard->Index = index ;
    memcpy(pPlayerCard->CardType, CardType, 32);
    pPlayerCard->HoldCards[0].Color = GetCardColor(CardColor1);
    pPlayerCard->HoldCards[0].Point = GetCardPoint(CardPoint1);
    pPlayerCard->HoldCards[1].Color = GetCardColor(CardColor2);
    pPlayerCard->HoldCards[1].Point = GetCardPoint(CardPoint2);
    return true;
}

PLAYER_Action_EN GetAction(const char * ActionNAme)
{
    if (strcmp(ActionNAme, "call") == 0) return ACTION_call;
    if (strcmp(ActionNAme, "check") == 0) return ACTION_check;
    if (strcmp(ActionNAme, "raise") == 0) return ACTION_raise;
    if (strcmp(ActionNAme, "fold") == 0) return ACTION_fold;
    if (strcmp(ActionNAme, "allin") == 0) return ACTION_allin;
    return ACTION_fold;
}

const char * GetActionName(PLAYER_Action_EN act)
{
    const char * Actions[ACTION_BUTTON] = {"fold", "check", "call", "raise", "all_in", "fold"};
    return Actions[act];
}

/* һ��inquire msg�����8����ҵ���Ϣ */
void Msg_ReadInquireInfoEx(const char LineBuffer[256], MSG_INQUIRE_INFO * pInquire)
{
    char Actions[32] = {0};
    unsigned int PID = 0;
    int Jetton = 0;
    int Money = 0;
    int Bet = 0;
    int scan_num = 0;
    MSG_INQUIRE_PLAYER_ACTION * pPlayerAction = NULL;

    DOT(pInquire->PlayerNum);    

    printf("Msg_ReadInquireInfoEx:%s;\r\n", LineBuffer);
    
    pPlayerAction = &pInquire->PlayerActions[pInquire->PlayerNum];
    scan_num = sscanf(LineBuffer, "%d %d %d %d %s",
                      &PID, &Jetton, &Money, &Bet, Actions);
    if (scan_num != 5)
    {
        TRACE("sscanf error [%s];\r\n", LineBuffer);
        return;
    }

    //memcpy(pPlayerAction->PlayerID, PID, 32),
    pPlayerAction->PlayerID = PID;
    pPlayerAction->Jetton = Jetton;
    pPlayerAction->Money = Money;
    pPlayerAction->Bet = Bet;
    pPlayerAction->Action = GetAction(Actions);

    /* ���ֻ��8�����������8�����������ͬһID�Ĳ�ѯ�����棬�����ٷ������� */
    pInquire->PlayerNum = (pInquire->PlayerNum + 1) % 8;
    DOT(pInquire->PlayerNum);
    return;
}

/* ������λ���ͷ�����λ��Ϣ */
const char * Msg_GetPlayType(PLAYER_SEAT_INFO *pPlayerInfo)
{
    switch (pPlayerInfo->Type)
    {
        default:
        case PLAYER_SEAT_TYPES_none:        return "common";
        case PLAYER_SEAT_TYPES_small_blind: return "small blind";
        case PLAYER_SEAT_TYPES_big_blind:   return "big blind";
        case PLAYER_SEAT_TYPES_button:      return "Button";
    }
}

void Msg_LinerReader_Seat(char Buffer[256], RoundInfo * pRound)
{
    Msg_ReadSeatInfo_PlayerSeat(&pRound->SeatInfo, Buffer);
    pRound->RoundStatus = SER_MSG_TYPE_seat_info;
    return;
}

void Msg_LinerReader_Blind(char Buffer[256], RoundInfo * pRound)
{
    Msg_ReadBlindInfo_Ex(Buffer, &pRound->Blind);
    pRound->RoundStatus = SER_MSG_TYPE_blind;
    return;
}

void Msg_LinerReader_Hold(char Buffer[256], RoundInfo * pRound)
{
    Msg_ReadCardsInfo_Card(&pRound->HoldCards, Buffer);
    pRound->RoundStatus = SER_MSG_TYPE_hold_cards;
//    Debug_PrintChardInfo(__FILE__, __LINE__, pRound->HoldCards.Cards, pRound->HoldCards.CardNum);
    return;
}

MSG_INQUIRE_INFO * Msg_GetCurrentInquireInfo(RoundInfo * pRound)
{
    DOT(pRound->PublicCards.CardNum);
    switch (pRound->PublicCards.CardNum)
    {
        default:
        case 0:
            return &pRound->Inquires[0];
        case 3:
            return &pRound->Inquires[1];
        case 4:
            return &pRound->Inquires[2];
        case 5:
            return &pRound->Inquires[3];
    }
}

void Msg_LinerReader_Inquire(char Buffer[256], RoundInfo * pRound)
{
    MSG_INQUIRE_INFO * pInquireInfo = NULL;

    pInquireInfo = Msg_GetCurrentInquireInfo(pRound);
    if (strstr(Buffer, "total pot") != NULL)
    {
        int Jetton = 0;
        sscanf(Buffer, "total pot:%d", &Jetton);
        pInquireInfo->TotalPot = Jetton;
        return;
    }
    Msg_ReadInquireInfoEx(Buffer, pInquireInfo);
    //Debug_PrintChardInfo(__FILE__, __LINE__, pRound->HoldCards.Cards, pRound->HoldCards.CardNum);
    return;
}

void Msg_Inquire_Action(RoundInfo * pRound)
{
    #if 0
    char ActionBufer[128] = "check";
    ResponseAction(ActionBufer, strlen(ActionBufer));
    return;
    #else // CHECK
    MSG_INQUIRE_INFO * pInquire = Msg_GetCurrentInquireInfo(pRound);
    Debug_PrintInquireInfo(pInquire);
    return STG_Inquire_Action(pRound);
    #endif
}

void Msg_LinerReader_Flop(char Buffer[256], RoundInfo * pRound)
{
    Msg_ReadCardsInfo_Card(&pRound->PublicCards, Buffer);
    pRound->RoundStatus = SER_MSG_TYPE_flop;
    //Debug_PrintChardInfo(__FILE__, __LINE__, pRound->HoldCards.Cards, pRound->HoldCards.CardNum);
    return;
}

void Msg_LinerReader_Turn(char Buffer[256], RoundInfo * pRound)
{
    Msg_ReadCardsInfo_Card(&pRound->PublicCards, Buffer);
    pRound->RoundStatus = SER_MSG_TYPE_turn;
    //Debug_PrintChardInfo(__FILE__, __LINE__, pRound->HoldCards.Cards, pRound->HoldCards.CardNum);
    return;
}

const char *Msg_GetCardTypeName(CARD_TYPES Type)
{
    const char * TypeNames[] =
    {
        /* CT for CardType */
        "CT_None",
        "CT_HIGH_CARD",
        "CT_ONE_PAIR",
        "CT_TWO_PAIR",
        "CT_THREE_OF_A_KIND",
        "CT_STRAIGHT",
        "CT_FLUSH",
        "CT_FULL_HOUSE",
        "CT_FOUR_OF_A_KIND",
        "CT_STRAIGHT_FLUSH",
        "CT_ROYAL_FLUSH",
    };
    return TypeNames[Type];
}

void Msg_LinerReader_River(char Buffer[256], RoundInfo * pRound)
{
    Msg_ReadCardsInfo_Card(&pRound->PublicCards, Buffer);
    pRound->RoundStatus = SER_MSG_TYPE_river;
    
    //Debug_PrintChardInfo(__FILE__, __LINE__, pRound->HoldCards.Cards, pRound->HoldCards.CardNum);

    return;
}

void Msg_LinerReader_ShowDown(char Buffer[256], RoundInfo * pRound)
{
    pRound->RoundStatus = SER_MSG_TYPE_showdown;

    //printf("[%s:%d]%s;\r\n", __FILE__, __LINE__, Buffer);

#if 0
    /* �ȶ�ȡ5�Ź��� */
    if ((pRound->ShowDown.CardNum >= 1) && (pRound->ShowDown.CardNum < 5))
    {
        CARD *pCard = NULL;
        pCard = &pRound->ShowDown.PublicCards[pRound->ShowDown.CardNum - 1];
        if (Msg_ReadCardsInfo_OneCard(Buffer, pCard))
        {
            pRound->ShowDown.CardNum ++;
        }
        return;
    }
#endif

    /* ��ȡÿ���˵����ƣ��������ᳬ��seatʱ�ĸ��� */
    if (   (pRound->ShowDown.PlayerNum >= 1))
    //    && (pRound->ShowDown.PlayerNum < pRound->SeatInfo.PlayerNum))
    {
        MSG_SHOWDWON_PLAYER_CARD *pPlayerCard = NULL;
        pPlayerCard = &pRound->ShowDown.Players[pRound->ShowDown.PlayerNum - 1];
        if (Msg_ReadPlayerCardInfo(pPlayerCard, Buffer))
        {
            pRound->ShowDown.PlayerNum ++;
        }
        Debug_PrintShowDown(&pRound->ShowDown);
        return;
    }

    /* ��ʼ��ȡ���� */
    if (strstr(Buffer, "common/") != NULL)
    {
        pRound->ShowDown.CardNum = 1;
        return;
    }

    /* ������ȡ���ƣ���ʼ������ */
    if (strstr(Buffer, "/common ") != NULL)
    {
        pRound->ShowDown.PlayerNum = 1;
        return;
    }

    return;
}

void Msg_LinerReader_Notify(char Buffer[256], RoundInfo * pRound)
{
    return;
}

void Msg_LinerReader_GameOver(char Buffer[256], RoundInfo * pRound)
{
    /* ���Խ����ݱ��浽���� */

    return;
}

/* ��������˵�����һ���ģ�pot win���������߶������ݣ���˲����ڽ���һ�������Ժ�ͱ���round���� */
void Msg_LinerReader_PotWin(char Buffer[256], RoundInfo * pRound)
{
    static int RoundIndex = 0;
    /* ��ȡ�ʳ���Ϣ */
    Msg_ReadPlayJettonInfo(&pRound->PotWin.PotWin[pRound->PotWin.PotWinCount], Buffer);
//    printf("Player %d win %d;\r\n",
//           pRound->PotWin.PotWin[pRound->PotWin.PotWinCount].PlayerID,
//           pRound->PotWin.PotWin[pRound->PotWin.PotWinCount].Jetton);
    pRound->PotWin.PotWinCount = (pRound->PotWin.PotWinCount + 1) % 2;
    //
    return;
}

void Msg_Read_Ex(const char * pMsg, int MaxLen, RoundInfo * pRound)
{
    int ReadNum;
    char Buffer[256] ;//= {0};
    MSG_READ_INFO MsgInfo = {0};
    SER_MSG_TYPES Type;//= Msg_GetMsgType(pMsg, MaxLen);
    MSG_NAME_TYPE_ENTRY * pMsgEntry;// = AllMsgTypes[Type];
    //
    MsgInfo.pMsg = pMsg;
    MsgInfo.MaxLen = MaxLen;

    while ((ReadNum = Msg_ReadLine(&MsgInfo, Buffer)) > 0)
    {
        Type = Msg_GetMsgTypeByMsgName(Buffer);
        pRound->CurrentMsgType = Type;

        //DOT(pRound->CurrentMsgType);

        pMsgEntry = &AllMsgTypes[Type];
        
        Msg_SetOffset(&MsgInfo, MsgInfo.Index + ReadNum);
        //printf("[%d][%s]\r\n", ReadNum, Buffer);
        if (pRound->CurrentMsgType == SER_MSG_TYPE_game_over)
        {
            pMsgEntry->Action(pRound);
            break;
        }
        
        while ((ReadNum = Msg_ReadLine(&MsgInfo, Buffer)) > 0)
        {
            /* ��ȡ��Ϣ�����������Ҫ�����͵���action���� */
            if (strcmp(Buffer, pMsgEntry->pEndName) == 0)
            {
                Msg_SetOffset(&MsgInfo, MsgInfo.Index + ReadNum);
                if (pMsgEntry->Action)
                {
                    pMsgEntry->Action(pRound);
                }
                break;
            }

            /* ������ȡ������Ϣ */
            if (pMsgEntry->LinerReader)
            {
                pMsgEntry->LinerReader(Buffer, pRound);
            }
            Msg_SetOffset(&MsgInfo, MsgInfo.Index + ReadNum);
        }
    }
    return;
}



