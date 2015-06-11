#include <stdio.h>
#include <stdlib.h>  //
#include <string.h>  //strlen
#include <unistd.h>  //usleep/close
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "pthread.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>
#include "Player.h"

/* ��������� */

obj_queue g_round_queue;

void QueueInit(obj_queue * pQueue)
{
    memset(pQueue, 0, sizeof(obj_queue));
    pthread_mutex_init(&pQueue->Lock, NULL);
    return;
}

void QueueAdd(obj_queue * pQueue, void * pMsg, int size)
{
    queue_entry *pNewMsg = NULL;
    do
    {
        pNewMsg = (queue_entry *)malloc(sizeof(queue_entry) + size);
        if (pNewMsg == NULL)
        {
            TRACE("%lu %d\r\n", sizeof(queue_entry) + size, errno);
            return;
        }
    } while (pNewMsg == NULL);

    memset(pNewMsg, 0, sizeof(queue_entry) + size);

    pNewMsg->pObjData = pNewMsg + 1;
    pNewMsg->pNextMsg = NULL;
    memcpy(pNewMsg->pObjData, pMsg, size);

    pthread_mutex_lock(&pQueue->Lock);
    if (pQueue->MsgCount == 0)
    {
        /* ������ȻͬʱΪ�� */
        pQueue->pFirstMsg = pNewMsg;
    }
    else
    {
        pQueue->pLastMsg->pNextMsg = pNewMsg;
    }
    pQueue->pLastMsg = pNewMsg;
    pQueue->MsgCount ++;
    pthread_mutex_unlock(&pQueue->Lock);

    return;
}

queue_entry * QueueGet(obj_queue * pQueue)
{
    queue_entry *pMsg = NULL;
    pthread_mutex_lock(&pQueue->Lock);
    if (pQueue->MsgCount > 0)
    {
        pMsg = pQueue->pFirstMsg;
        pQueue->pFirstMsg = (queue_entry *)pQueue->pFirstMsg->pNextMsg;
        if (pQueue->pFirstMsg == NULL)
        {
            pQueue->pLastMsg = NULL;
        }
        pQueue->MsgCount --;
    }
    pthread_mutex_unlock(&pQueue->Lock);
    return pMsg;
}

/* Ӯ������ */
typedef struct STG_WIN_CARDS_
{
    int ShowTimes;
    int WinTimes;
    //CARD_TYPES WinType;     /* Ӯ�Ƶ����� */
    //CARD_POINT MaxPoint;    /* Ӯ�Ƶ������� */
    //CARD_POINT SedPoint;    /* Ӯ�ƵĴδ����������Ƿ���Ҫ���ǣ����Բ����ǣ���������Ե�������δ��ƿ�����ͨ��һ�Ե�������ж� */
} STG_WIN_CARDS;

typedef struct STG_DATA_
{
    int RunningFlag;

    pthread_mutex_t Lock;
    pthread_t subThread;

    /* Ӯ�Ƶ����ݣ�һ�Ŵ�����ݱ������������ */
    STG_WIN_CARDS AllWinCards[CARD_TYPES_Butt][CARD_POINTT_BUTT];
} STG_DATA;

STG_DATA g_stg = {0};

static void STG_Lock(void)
{
    pthread_mutex_lock(&g_stg.Lock);
}

static void STG_Unlock(void)
{
    pthread_mutex_unlock(&g_stg.Lock);
}

void STG_Debug_PrintWinCardData(void)
{
    int Type;
    int Point;
    for (Type =  CARD_TYPES_High; Type < CARD_TYPES_Butt; Type ++)
    {
        for (Point = CARD_POINTT_2; Point < CARD_POINTT_BUTT; Point ++)
        {
            TRACE("Type:%s Max point: %s ShowTimes:%5d; WinTimes:%5d;\r\n",
                   Msg_GetCardTypeName((CARD_TYPES)Type),
                   GetCardPointName((CARD_POINT)Point),
                   g_stg.AllWinCards[Type][Point].ShowTimes,
                   g_stg.AllWinCards[Type][Point].WinTimes);
        }
    }
}

/* ����������ݿ� */
void STG_LoadStudyData(void)
{
    int fid = -1;
    int read_size = 0;

    memset(&g_stg.AllWinCards, 0, sizeof(g_stg.AllWinCards));

    fid = open("game_win_data.bin", O_RDONLY);
    if (fid == -1)
    {
        TRACE("Load game_win_data.bin error.\r\n");
        return;
    }

    read_size = read(fid, &g_stg.AllWinCards, sizeof(g_stg.AllWinCards));
    if (read_size != sizeof(g_stg.AllWinCards))
    {
        memset(&g_stg.AllWinCards, 0, sizeof(g_stg.AllWinCards));
        TRACE("Load game_win_data.bin error.\r\n");
    }
    close(fid);

    STG_Debug_PrintWinCardData();

    return;
}

/* ����ѧϰ������������ */
void STG_SaveStudyData(void)
{
    int fid = -1;
    fid = open("game_win_data.bin", O_CREAT|O_WRONLY|O_TRUNC|O_SYNC);
    if (fid == -1)
    {
        TRACE("Save game_win_data.bin error.\r\n");
        return;
    }
    write(fid, &g_stg.AllWinCards, sizeof(g_stg.AllWinCards));
    close(fid);
    STG_Debug_PrintWinCardData();
    TRACE("Save game_win_data.bin\r\n");
    return;
}

/* ����һ�ֵ����� */
void STG_SaveRoundData(RoundInfo * pRound)
{
    static int RoundIndex = 0;
    printf("====save round %d =========\r\n", pRound->RoundIndex);

//    TRACE("Add round %d data. public card num %d;\r\n",
//           pRound->RoundIndex, pRound->PublicCards.CardNum);

    QueueAdd(&g_round_queue, pRound, sizeof(RoundInfo));

    memset(pRound, 0, sizeof(RoundInfo));
    //
    RoundIndex ++;
    pRound->RoundIndex = RoundIndex;

    return;
}

/* ��ѯȫ�����ݿ⣬ȡ��ָ�����ͺ���������ʤ��  */
int STG_CheckWinRation(CARD_TYPES Type, CARD_POINT MaxPoint)
{
    int WinNum = g_stg.AllWinCards[Type][MaxPoint].WinTimes;
    int ShowNum = g_stg.AllWinCards[Type][MaxPoint].ShowTimes;
    #if 0
    printf("Check Win Ration: %s %s: %d / %d;\r\n",
          Msg_GetCardTypeName(Type),
          GetCardPointName(MaxPoint),
          WinNum, ShowNum);
    #endif
    if (ShowNum > 0)
    {
        return (WinNum * 100) / ShowNum;
    }
    return 0;
}

/* �������ƽ׶ε�inquire */
PLAYER_Action STG_GetHoldAction(RoundInfo * pRound)
{
    int WinRation = 0;
    CARD AllCards[2];
    CARD_POINT MaxPoint[CARD_TYPES_Butt];
    CARD_TYPES Type = CARD_TYPES_None;
    AllCards[0] = pRound->HoldCards.Cards[0];
    AllCards[1] = pRound->HoldCards.Cards[1];

    Type = STG_GetCardTypes(AllCards, 2, MaxPoint);
    WinRation = STG_CheckWinRation(Type, MaxPoint[Type]);
    if (WinRation > 50)
    {
        return ACTION_raise;
    }
    return ACTION_check;
}

/* ���ݸ������������ƺ͵������������������Ƿ����Լ����� */
bool STG_IsMaxPointInHand(CARD_POINT MaxPoint, CARD Cards[2])
{
    if (   (Cards[0].Point == MaxPoint)
        || (Cards[1].Point == MaxPoint))
    {
        return true;
    }
    return false;
}

/*
    ����ʱ�Ĵ����ؼ�
*/
PLAYER_Action STG_GetRiverAction(RoundInfo * pRound)
{
    int WinRation = 0;
    CARD AllCards[7];
    CARD_POINT MaxPoint[CARD_TYPES_Butt];
    CARD_TYPES Type = CARD_TYPES_None;
    bool IsMaxInHand = false;
    AllCards[0] = pRound->PublicCards.Cards[0];
    AllCards[1] = pRound->PublicCards.Cards[1];
    AllCards[2] = pRound->PublicCards.Cards[2];
    AllCards[3] = pRound->PublicCards.Cards[3];
    AllCards[4] = pRound->PublicCards.Cards[4];
    AllCards[5] = pRound->HoldCards.Cards[0];
    AllCards[6] = pRound->HoldCards.Cards[1];
    Type = STG_GetCardTypes(AllCards, 7, MaxPoint);
    WinRation = STG_CheckWinRation(Type, MaxPoint[Type]);

    if ((WinRation >= 30) && STG_IsMaxPointInHand(MaxPoint[Type], pRound->HoldCards.Cards))
    {
//        printf("Round %d allin:%d; \r\n", pRound->RoundIndex, WinRation,
//               Msg_GetCardTypeName(Type),
//               GetCardPointName(MaxPoint[Type]));
        return ACTION_allin;
    }
    else
    {
        return ACTION_check;
    }
}

/* ֻ����inquire�ж�ȡ������ */
const char * STG_GetAction(RoundInfo * pRound)
{
    PLAYER_Action  Action = ACTION_fold;

    switch (pRound->RoundStatus)
    {
    default:
         Action = ACTION_check;
         break;
    case SER_MSG_TYPE_hold_cards:/* ֻ����������ʱ��inqurie */
        Action = ACTION_check;
        //Action = STG_GetHoldAction(pRound);
        break;
    case SER_MSG_TYPE_river:/* ֻ����������ʱ��inqurie */
        Action = STG_GetRiverAction(pRound);
        break;
    }
    return GetActionName(Action);
}

/* ����inquire�Ļظ� */
void STG_InquireAction(RoundInfo * pRound)
{
    //TRACE("Response check.\r\n");
    //const char* response = "check";
    const char* response = STG_GetAction(pRound);
    ResponseAction(response, strlen(response) + 1);
    return;
}

/* �����ƵĴ�С���� */
void STG_SortCardByPoint(CARD * Cards, int CardNum)
{
    int i = 0;
    int j = 0;
    CARD TempCard;

    for (i = 0; i < CardNum; i ++)
    {
        for (j = i + 1; j < CardNum; j ++)
        {
            if (Cards[j].Point < Cards[i].Point)
            {
                TempCard = Cards[i];
                Cards[i] = Cards[j];
                Cards[j] = TempCard;
            }
        }
    }
    return;
}

/* �ж����������Ƿ�����ͬ�ĵ��� */
bool STG_IsTwoHoldCardHasSamePoint(CARD CardsA[2], CARD CardsB[2])
{
    STG_SortCardByPoint(CardsA, 2);
    STG_SortCardByPoint(CardsB, 2);
    if (   (CardsA[0].Point == CardsB[0].Point)
        && (CardsA[1].Point == CardsB[1].Point))
    {
        return true;
    }
    return false;
}

static void print_card(CARD * pCard, int CardNum)
{
    int index = 0;
    printf("Card Info:\r\n");
    for (index = 0; index < CardNum; index ++)
    {
        printf("card_%d %s %s\n", index, GetCardColorName(pCard),
               GetCardPointName(pCard->Point));
        pCard ++;
    }
    return;
}

/* �ж��Ƿ���ͬ��˳���Լ��ʼ�ͬ��˳, pCards�Ѿ��� */
CARD_TYPES SET_GetStraightType(CARD FlushCards[8], int CardNum, CARD_POINT * pMaxPoint)
{
    int index = 0;
    int StriaghtNum = 1;
    int MaxIndex = 0;
    CARD_POINT PrePoint = CARD_POINTT_Unknow;
    //printf("Get %d card for flush:\r\n", CardNum);
    //print_card(FlushCards, CardNum);
    for (index = 0; index < CardNum; index ++)
    {
        if (FlushCards[index].Point == CARD_POINTT_Unknow)
        {
            /* ����û����д�Ļ�ɫ�ƣ����ſ�����������ɫ�ģ��������� */
            continue;
        }
        if (PrePoint == CARD_POINTT_Unknow)
        {
            PrePoint =  FlushCards[index].Point;
            continue;
        }
        //printf("%d %d %d %d\r\n", index, (int)PrePoint, StriaghtNum,(int)FlushCards[index].Point);
        if (FlushCards[index].Point == PrePoint + 1)
        {
            MaxIndex = index;
            StriaghtNum ++;
        }
        else
        {
            StriaghtNum = 1;
        }
        PrePoint = FlushCards[index].Point;
    }

    //printf("result %d %d;\r\n", StriaghtNum, MaxIndex);

    if ((StriaghtNum >= 5) && (FlushCards[MaxIndex].Point == CARD_POINTT_A))
    {
        *pMaxPoint = FlushCards[MaxIndex].Point;
        return CARD_TYPES_Royal_Flush;
    }
    if (StriaghtNum >= 5)
    {
        *pMaxPoint = FlushCards[MaxIndex].Point;
        return CARD_TYPES_Straight_Flush;
    }
    return CARD_TYPES_None;
}

/* ���ݲ�ͬ�������������Ƶ����� */
CARD_TYPES STG_GetCardTypes(CARD *pCards, int CardNum, CARD_POINT MaxPoints[CARD_TYPES_Butt])
{
    int AllPoints[CARD_POINTT_BUTT];// = {0};
    int AllColors[CARD_COLOR_BUTT];// = {0};
    CARD FlushCards[CARD_COLOR_BUTT][8];    /* ������7���� */
    int index = 0;
    int Pairs = 0;
    int Three = 0;
    int Four = 0;
    int Straight = 0;
    CARD_TYPES CardType = CARD_TYPES_High;
    CARD_TYPES ColorType = CARD_TYPES_None;
    CARD_COLOR FlushColor = CARD_COLOR_Unknow;

    memset(&AllPoints, 0, sizeof(AllPoints));
    memset(&AllColors, 0, sizeof(AllColors));
    memset(FlushCards, 0, sizeof(FlushCards));

    STG_SortCardByPoint(pCards, CardNum);

    /* �������ľ������һ�� */
    MaxPoints[CARD_TYPES_High] = pCards[CardNum - 1].Point;

    //TRACE("STG_GetCardTypes:%d; \r\n", CardNum);

    for (index = 0; index < CardNum; index ++)
    {
        //TRACE("%d:%s %s\r\n", index, GetCardColorName(&pCards[index]),
        //       GetCardPointName(pCards[index].Point));
        AllColors[pCards[index].Color] ++;
        AllPoints[pCards[index].Point] ++;

        /* ���Ʒŵ�ͬһ��ɫ������ */
        FlushCards[pCards[index].Color][index] = pCards[index];
        //
        if (AllColors[pCards[index].Color] >= 5)
        {
            /* ��¼ͬ����ͬ��˳���ʼ�ͬ��˳�������� */
            MaxPoints[CARD_TYPES_Flush] = pCards[index].Point;
            FlushColor = pCards[index].Color;
            ColorType = CARD_TYPES_Flush;
        }
    }

    //TRACE("STG_GetCardTypes:\r\n");
    for (index = CARD_POINTT_2; index < CARD_POINTT_A + 1; index ++)
    {
        if (AllPoints[index] == 0)
        {
            /* ˳��ֻҪ�м���һ���Ͽ����Ͳ����ˣ�������Ѿ�����5�ˣ��Ͳ��� */
            Straight = Straight >=5 ? Straight : 0;
        }
        if (AllPoints[index] >= 1)
        {
            //TRACE("[%d]%d ", index, AllPoints[index]);
            Straight ++;
            if (Straight >= 5)
            {
                MaxPoints[CARD_TYPES_Straight] = (CARD_POINT)index;
            }
            if ((index == CARD_POINTT_5) && (Straight == 4) && (AllPoints[CARD_POINTT_A] > 0))
            {
                /* A,2,3,4,5��˳�� */
                static int spical_straight = 0;
                Straight ++;
                MaxPoints[CARD_TYPES_Straight] = (CARD_POINT)index;
                //printf("spical_straight:%d\r\n", spical_straight++);
            }
        }
        if (AllPoints[index] == 2)
        {
            Pairs ++;
            MaxPoints[CARD_TYPES_OnePair] = (CARD_POINT)index;
            if (Pairs >= 2)
            {
                MaxPoints[CARD_TYPES_TwoPair] = (CARD_POINT)index;
            }
            if (Three >= 1)
            {
                MaxPoints[CARD_TYPES_Full_House] = (CARD_POINT)index;
            }
        }
        if (AllPoints[index] == 3)
        {
            Three ++;
            MaxPoints[CARD_TYPES_Three_Of_A_Kind] = (CARD_POINT)index;
            if (Pairs >= 1)
            {
                MaxPoints[CARD_TYPES_Full_House] = (CARD_POINT)index;
            }
        }
        if (AllPoints[index] == 4)
        {
            Four ++;
            MaxPoints[CARD_TYPES_Four_Of_A_Kind] = (CARD_POINT)index;
        }
    }

    /* Ӯ�����ʹӴ�С�ж� */

    /* ͬ��˳�ͻʼ�ͬ��˳����ʱ���жϣ�����Ϊ0 */
    if ((Straight >= 5) && (ColorType == CARD_TYPES_Flush))
    {
        CARD_POINT SpicalMaxPoint;
        CARD_TYPES SpicalType = SET_GetStraightType(FlushCards[FlushColor],
                                                    7,
                                                    &SpicalMaxPoint);
        if (SpicalType != CARD_TYPES_None)
        {
            MaxPoints[SpicalType] = SpicalMaxPoint;
            return SpicalType;
        }
    }

//    TRACE("\r\nSTG_GetCardTypes_end. four:%d;three:%d;pair:%d;straight:%d;color:%d;\r\n",
//           Four, Three, Pairs, Straight, (int)ColorType);

    if (Four > 0)
    {
        return CARD_TYPES_Four_Of_A_Kind;
    }

    if (((Three > 0) && (Pairs > 0)) || (Three >= 2))
    {
        return CARD_TYPES_Full_House;
    }

    if (ColorType == CARD_TYPES_Flush)
    {
        return CARD_TYPES_Flush;
    }

    if (Straight >= 5)
    {
        return CARD_TYPES_Straight;
    }

    if (Three > 0)
    {
        return CARD_TYPES_Three_Of_A_Kind;
    }

    if (Pairs >= 2)
    {
        return CARD_TYPES_TwoPair;
    }

    if (Pairs >= 1)
    {
        return CARD_TYPES_OnePair;
    }

    return CARD_TYPES_High;
}

void STG_AnalyseWinCard_AllCards(CARD AllCards[7], int WinIndex)
{
    CARD_POINT MaxPoints[CARD_TYPES_Butt] = {(CARD_POINT)0};
    CARD_TYPES CardType = CARD_TYPES_None;
    CardType = STG_GetCardTypes(AllCards, 7, MaxPoints);
    //
    g_stg.AllWinCards[CardType][MaxPoints[CardType]].ShowTimes ++;
    if (WinIndex == 1)
    {
        g_stg.AllWinCards[CardType][MaxPoints[CardType]].WinTimes ++;
    }
//    TRACE("Win Type %s, max point %d, %d;\r\n",
//          Msg_GetCardTypeName(CardType),
//          (int)MaxPoints[CardType], WinIndex);
}

/* ������ѡ�ֵ����빫�Ƶ���ϣ�Ȼ���¼Ӯ�ƺͳ����ƵĴ��� */
void STG_AnalyseWinCard(RoundInfo *pRound)
{
    CARD AllCards[7] = {(CARD_POINT)0};
    int index = 0;
    MSG_SHOWDWON_PLAYER_CARD * pPlayCard = NULL;

    for (index = 0; index < pRound->ShowDown.PlayerNum - 1; index ++)
    {
        /* �����㷨Ҫ��AllCards������Ҫÿ�ζ����¸�ֵ */
        memcpy(&AllCards, &pRound->ShowDown.PublicCards, sizeof(pRound->ShowDown.PublicCards));
        pPlayCard = &pRound->ShowDown.Players[index];
        AllCards[5] = pPlayCard->HoldCards[0];
        AllCards[6] = pPlayCard->HoldCards[1];
        //TRACE("Debug_PrintShowDown:%d player %d;\r\n", pRound->RoundIndex, index);
        //Debug_PrintShowDown(&pRound->ShowDown);
        //Debug_PrintChardInfo(AllCards, 7);
        //
        STG_AnalyseWinCard_AllCards(AllCards, pRound->ShowDown.Players[index].Index);
    }
}


/* �������ͺͶ������ */
void * STG_ProcessThread(void *pArgs)
{
    int ret = 0;
    RoundInfo *pRound = NULL;
    queue_entry * pQueueEntry = NULL;

    printf("STG_ProcessThread.\r\n");

    while (g_stg.RunningFlag == true)
    {
        /* ���µ�round���� */
        //pRound = STG_GetNextRound();
        pQueueEntry = QueueGet(&g_round_queue);
        if (pQueueEntry == NULL)
        {
            usleep(1000);
            continue;
        }

        pRound = (RoundInfo *)pQueueEntry->pObjData;

        //Debug_ShowRoundInfo(pRound);

//        printf("Get round %d data to anylize. public card num %d;\r\n",
//               pRound->RoundIndex, pRound->PublicCards.CardNum);

        STG_AnalyseWinCard(pRound);

        free(pQueueEntry);
        pQueueEntry = NULL;
        pRound = NULL;

        /* �����ϵ����� */
    }
    return NULL;
}

void STG_Init(void)
{
    int ret;

    QueueInit(&g_round_queue);

    /* ����ѧϰ���� */
    STG_LoadStudyData();
    g_stg.RunningFlag = true;
    pthread_mutex_init(&g_stg.Lock, NULL);

    ret = pthread_create(&g_stg.subThread, NULL, STG_ProcessThread, NULL);
    if (ret == -1)
    {
        printf("STG_Init pthread_create error!%d \r\n", errno);
    }

    pthread_detach(g_stg.subThread);
    return;
}

void STG_Dispose(void)
{
    g_stg.RunningFlag = false;
    pthread_cancel(g_stg.subThread);
    STG_SaveStudyData();
    return;
}
