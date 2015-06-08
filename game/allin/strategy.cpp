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

/* ÿ��500�� */
#define MAX_ROUND_COUNT 500

/* ��������� */

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
    int StgWriteIndex;
    int StgReadIndex;

    pthread_mutex_t Lock;
    pthread_t subThread;
    RoundInfo Rounds[MAX_ROUND_COUNT]; /* 500 * 13K */

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
            TRACE("Type:%s Max point: %s ShowTimes:%d; WinTimes:%d;\r\n",
                   Msg_GetCardTypeName((CARD_TYPES)Type),
                   GetCardPointName((CARD_POINT)Point),
                   g_stg.AllWinCards[Type][Point].ShowTimes,
                   g_stg.AllWinCards[Type][Point].WinTimes);
        }
    }
}

void STG_LoadStudyData(void)
{
    int fid = -1;
    int read_size = 0;

    memset(&g_stg.AllWinCards, 0, sizeof(g_stg.AllWinCards));

    fid = open("game_win_data.bin", O_WRONLY);
    if (fid == -1)
    {
        return;
    }

    read_size = read(fid, &g_stg.AllWinCards, sizeof(g_stg.AllWinCards));
    if (read_size != sizeof(g_stg.AllWinCards))
    {
        memset(&g_stg.AllWinCards, 0, sizeof(g_stg.AllWinCards));
        TRACE("Load game_win_data.bin\r\n");
    }
    close(fid);

    STG_Debug_PrintWinCardData();

    return;
}

void STG_SaveStudyData(void)
{
    int fid = -1;
    fid = open("game_win_data.bin", O_CREAT|O_WRONLY|O_TRUNC|O_SYNC);
    if (fid == -1)
    {
        return;
    }
    write(fid, &g_stg.AllWinCards, sizeof(g_stg.AllWinCards));
    close(fid);
    STG_Debug_PrintWinCardData();
    TRACE("Save game_win_data.bin\r\n");
    return;
}

void STG_SaveRoundData(RoundInfo * pRoundInfo)
{
    if (g_stg.StgWriteIndex >= MAX_ROUND_COUNT)
    {
        return;
    }
    STG_Lock();
    memcpy(&g_stg.Rounds[g_stg.StgWriteIndex], pRoundInfo, sizeof(RoundInfo));
    g_stg.StgWriteIndex ++; //= (g_stg.StgWriteIndex + 1) % MAX_ROUND_COUNT;
    TRACE("Save round %d data.\r\n", g_stg.StgWriteIndex);
    STG_Unlock();
    return;
}

RoundInfo * STG_GetNextRound(void)
{
    RoundInfo *pRound = NULL;
    STG_Lock();
    if (g_stg.StgReadIndex < g_stg.StgWriteIndex)
    {
        pRound = &g_stg.Rounds[g_stg.StgReadIndex];
        g_stg.StgReadIndex ++;
    }
    STG_Unlock();
    return pRound;
}

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

/* ���ݲ�ͬ�������������Ƶ����� */
CARD_TYPES STG_GetCardTypes(CARD *pCards, int CardNum, CARD_POINT MaxPoints[CARD_TYPES_Butt])
{
    int AllPoints[CARD_POINTT_BUTT];// = {0};
    int AllColors[CARD_COLOR_BUTT];// = {0};
    int index = 0;
    int Pairs = 0;
    int Three = 0;
    int Four = 0;
    int Straight = 0;
    CARD_TYPES CardType = CARD_TYPES_High;
    CARD_TYPES ColorType = CARD_TYPES_None;

    memset(&AllPoints, 0, sizeof(AllPoints));
    memset(&AllColors, 0, sizeof(AllColors));

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
        //TRACE("%d;\r\n", AllColors[pCards[index].Color]);
        if (AllColors[pCards[index].Color] >= 5)
        {
            /* ��¼ͬ����ͬ��˳���ʼ�ͬ��˳�������� */
            MaxPoints[CARD_TYPES_Flush] = pCards[index].Point;
//            MaxPoints[CARD_TYPES_Straight_Flush] = pCards[index].Point;
//            MaxPoints[CARD_TYPES_Royal_Flush] = pCards[index].Point;
            ColorType = CARD_TYPES_Flush;
        }
    }

    //TRACE("\r\n");

    for (index = CARD_POINTT_2; index < CARD_POINTT_A + 1; index ++)
    {
        if (AllPoints[index] == 0)
        {
            //TRACE("\r\n");
            Straight = 0;   /* ˳��ֻҪ�м���һ���Ͽ����Ͳ����� */
        }
        if (AllPoints[index] >= 1)
        {
            //TRACE("\r\n");
            Straight ++;
            if (Straight >= 5)
            {
                MaxPoints[CARD_TYPES_Straight] = (CARD_POINT)index;
            }
        }
        if (AllPoints[index] == 2)
        {
            //TRACE("\r\n");
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
            //TRACE("\r\n");
            Three ++;
            MaxPoints[CARD_TYPES_Three_Of_A_Kind] = (CARD_POINT)index;
            if (Pairs >= 1)
            {
                MaxPoints[CARD_TYPES_Full_House] = (CARD_POINT)index;
            }
        }
        if (AllPoints[index] == 4)
        {
            //TRACE("\r\n");
            Four ++;
            MaxPoints[CARD_TYPES_Four_Of_A_Kind] = (CARD_POINT)index;
        }
    }

    //TRACE("\r\n");

    /* Ӯ�����ʹӴ�С�ж� */

    /* ͬ��˳�ͻʼ�ͬ��˳����ʱ���жϣ�����Ϊ0 */
    if ((Straight >= 5) && (ColorType == CARD_TYPES_Flush))
    {
        CARD_TYPES SpicalType = CARD_TYPES_Flush;
        if (   (SpicalType == CARD_TYPES_Straight_Flush)
            || (SpicalType == CARD_TYPES_Royal_Flush))
        {
                return SpicalType;
        }
    }

    if (Four > 0)
    {
        return CARD_TYPES_Four_Of_A_Kind;
    }

    if ((Three > 0) && (Pairs > 0))
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
    TRACE("Win Type %s, max point %d, %d;\r\n",
          Msg_GetCardTypeName(CardType),
          (int)MaxPoints[CardType], WinIndex);
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
        TRACE("Debug_PrintShowDown:%d player %d;\r\n", pRound->RoundIndex, index);
        //Debug_PrintShowDown(&pRound->ShowDown);
        Debug_PrintChardInfo(AllCards, 7);
        //
        STG_AnalyseWinCard_AllCards(AllCards, pRound->ShowDown.Players[index].Index);
    }
}


/* �������ͺͶ������ */
void * STG_ProcessThread(void *pArgs)
{
    int ret = 0;
    RoundInfo *pRound = NULL;

    printf("STG_ProcessThread.\r\n");

    while (g_stg.RunningFlag == true)
    {
        /* ���µ�round���� */
        pRound = STG_GetNextRound();
        if (pRound == NULL)
        {
            usleep(1000);
            continue;
        }

        Debug_ShowRoundInfo(pRound);

        //printf("Get round %d data to anylize.\r\n", pRound->RoundIndex);

        STG_AnalyseWinCard(pRound);

        /* �����ϵ����� */
    }
    return NULL;
}

void STG_Init(void)
{
    int ret;

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
