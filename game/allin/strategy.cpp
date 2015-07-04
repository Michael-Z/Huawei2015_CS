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
    STG_WIN_CARDS AllWinCards[8][CARD_TYPES_Butt][CARD_POINTT_BUTT];
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
    int UserNum;
    for (UserNum = 1; UserNum < 8; UserNum ++)
    {
        TRACE("Player num:%d\r\n", UserNum);
        for (Type =  CARD_TYPES_High; Type < CARD_TYPES_Butt; Type ++)
        {
            for (Point = CARD_POINTT_2; Point < CARD_POINTT_BUTT; Point ++)
            {
                TRACE("Type:%s Max: %s Show:%5d; Win:%5d;\r\n",
                       Msg_GetCardTypeName((CARD_TYPES)Type),
                       GetCardPointName((CARD_POINT)Point),
                       g_stg.AllWinCards[UserNum][Type][Point].ShowTimes,
                       g_stg.AllWinCards[UserNum][Type][Point].WinTimes);
            }
        }
    }
}

/* ����������ݿ� */
void STG_LoadStudyData(void)
{
    int fid = -1;
    int read_size = 0;

    memset(&g_stg.AllWinCards, 0, sizeof(g_stg.AllWinCards));

    fid = open("db.bin", O_RDONLY);
    if (fid == -1)
    {
        TRACE("Load db.bin error.\r\n");
        return;
    }

    read_size = read(fid, &g_stg.AllWinCards, sizeof(g_stg.AllWinCards));
    if (read_size != sizeof(g_stg.AllWinCards))
    {
        memset(&g_stg.AllWinCards, 0, sizeof(g_stg.AllWinCards));
        TRACE("Load db.bin error.\r\n");
    }
    close(fid);

    STG_Debug_PrintWinCardData();

    return;
}

/* ����ѧϰ������������ */
void STG_SaveStudyData(void)
{
    int fid = -1;
    fid = open("db.bin", O_CREAT|O_WRONLY|O_TRUNC|O_SYNC);
    if (fid == -1)
    {
        TRACE("Save db.bin error.\r\n");
        return;
    }
    write(fid, &g_stg.AllWinCards, sizeof(g_stg.AllWinCards));
    close(fid);
    TRACE("Save db.bin\r\n");
    STG_Debug_PrintWinCardData();
    return;
}

/* ����һ�ֵ����� */
void STG_SaveRoundData(RoundInfo * pRound)
{
    static int RoundIndex = 0;
    QueueAdd(&g_round_queue, pRound, sizeof(RoundInfo));
    memset(pRound, 0, sizeof(RoundInfo));
    RoundIndex ++;
    printf("Save data for round %d\r\n", RoundIndex);
    pRound->RoundIndex = RoundIndex;
    pRound->MyPlayerID = GetMyPlayerID();
    return;
}

/* ��ѯȫ�����ݿ⣬ȡ��ָ�����ͺ���������ʤ��  */
int STG_CheckWinRation(CARD_TYPES Type, CARD_POINT MaxPoint, int PlayerNum)
{
    //printf("STG_CheckWinRation:%d %d %d;\r\n", PlayerNum, (int)Type, (int)MaxPoint);
    int WinNum = g_stg.AllWinCards[PlayerNum - 1][Type][MaxPoint].WinTimes;
    int ShowNum = g_stg.AllWinCards[PlayerNum - 1][Type][MaxPoint].ShowTimes;

    if (ShowNum > 0)
    {
        return (WinNum * 100) / ShowNum;
    }
    
    return 0;
}

/* ��ͨ����������ѯʤ�ʣ��ڲ�ͬ�������бȽϺõ����ƣ�����һЩ������ */
int STG_CheckWinRationByType(CARD_TYPES Type, int PlayerNum)
{
    int index = 0;
    int WinNum = 0;
    int ShowNum  = 0;

    /* ��ȡ���ݣ��������� */
    for (index = CARD_POINTT_2; index < CARD_POINTT_BUTT; index ++)
    {
        WinNum += g_stg.AllWinCards[PlayerNum - 1][Type][index].WinTimes;
        ShowNum += g_stg.AllWinCards[PlayerNum - 1][Type][index].ShowTimes;
    }
    if (ShowNum > 0)
    {
        return (WinNum * 100) / ShowNum;
    }
    return 0;
}

void SGT_BuildAction(PLAYER_Action * pAct, PLAYER_Action_EN act, int arg)
{
    pAct->ActType = act;
    pAct->Args = arg;
    return;
}

/* ȡ�ñ��ֻ��ڳ������ */
int STG_GetActivePlayer(RoundInfo * pRound)
{
    //pRound->Inquires.PlayerActions

}

/* �������ƽ׶ε�inquire */
int STG_GetHoldAction(RoundInfo * pRound, PLAYER_Action * pAction)
{
    int WinRation = 0;
    CARD AllCards[2];
    CARD_POINT MaxPoint[CARD_TYPES_Butt];// = {(CARD_POINT)0};
    CARD_TYPES Type = CARD_TYPES_None;
    AllCards[0] = pRound->HoldCards.Cards[0];
    AllCards[1] = pRound->HoldCards.Cards[1];

    /* ��ȡ�������͵�ʤ����� */
    Type = STG_GetCardTypes(AllCards, 2, MaxPoint);
    WinRation = STG_CheckWinRation(Type, MaxPoint[Type], pRound->SeatInfo.PlayerNum);
    if (WinRation > 50)
    {
        return ACTION_raise;
    }
    else
    {
        
    }
    
    /* �ٷ�����ǰ������� */


    /*  */

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
void STG_GetRiverAction(RoundInfo * pRound, PLAYER_Action * pAction)
{
    int WinRation = 0;
    CARD AllCards[7];
    CARD_POINT MaxPoint[CARD_TYPES_Butt];// = {(CARD_POINT)0};
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
    WinRation = STG_CheckWinRation(Type, MaxPoint[Type], pRound->SeatInfo.PlayerNum);

    bool IsMaxInMyHand = STG_IsMaxPointInHand(MaxPoint[Type], pRound->HoldCards.Cards);
//    printf("Total pot on ground:%d win:%d ismax:%d;\r\n",
//           pRound->Inquires[3].TotalPot,
//           WinRation, IsMaxInMyHand);
    if (IsMaxInMyHand)
    {
        if (WinRation <= 30)
        {
    //        printf("Round %d allin:%d; \r\n", pRound->RoundIndex, WinRation,
    //               Msg_GetCardTypeName(Type),
    //               GetCardPointName(MaxPoint[Type]));
            //pRound-> ��¼raise�Ĵ������ܽ���������������Ͳ���raise������check����fold
            if (pRound->Inquires[3].TotalPot > 1000)
            {
                return SGT_BuildAction(pAction, ACTION_fold, 0);
            }
            else
            {
                return SGT_BuildAction(pAction, ACTION_check, 0);
            }
        }
        else if (WinRation <= 50)
        {
            if (pRound->RaiseTimes ++ < 2)
            {
                return SGT_BuildAction(pAction, ACTION_raise, 1);
            }
            return SGT_BuildAction(pAction, ACTION_check, 1);
        }
        else if (WinRation <= 70)
        {
            if (pRound->RaiseTimes ++ < 3)
            {
                return SGT_BuildAction(pAction, ACTION_raise, 3);
            }
            return SGT_BuildAction(pAction, ACTION_check, 1);
        }
        else
        {
            if (pRound->RaiseTimes ++ < 5)
            {
                return SGT_BuildAction(pAction, ACTION_raise, 5);
            }
            return SGT_BuildAction(pAction, ACTION_check, 1);
        }
    }
    return SGT_BuildAction(pAction, ACTION_fold, 1);
}

#define INIT_TWOCARD_NUM 40
#define INIT_TWOCARD_NUM_EXTEND 38

typedef struct
{
	int cardinfo;  //�����ƣ���AA��ʾ��1414��KK��ʾ��13130,��λ��ʾ��ɫ 0����ͬ��ɫ��1��ͬ��ɫ
	int No_Pet;    //����֮ǰ���˼�ע
	int With_Pet;  //����֮ǰ���˼�ע
}INIT_TWOCARD_INFO_EXTEND;


typedef enum
{
	TWOCARD_INFO_R, // ������ǰ����������ע���㶼Ҫ��ע
	TWOCARD_INFO_C, //�����ж��ٸ���ҽ�����Ϸ���㶼Ҫ��ע
	TWOCARD_INFO_C_RFI_LP, //�����ж��ٸ���ҽ�����Ϸ���㶼Ҫ��ע ���ں�λ ��ע
	TWOCARD_INFO_C1, //����֮ǰ������һ����Ҹ�ע����ſ��Ը�ע����������
	TWOCARD_INFO_C2, //����֮ǰ������������������ϵ���ҽ�����Ϸ����ſ��Ը�ע����������
	TWOCARD_INFO_C2_RFI_LP, //����֮ǰ������������������ϵ���ҽ�����Ϸ����ſ��Ը�ע���������� ���ں�λ��ע
	TWOCARD_INFO_C3, //����֮ǰ������������������ϵ���ҽ�����Ϸ����ſ��Ը�ע����������
	TWOCARD_INFO_C3_RFI_LP, //����֮ǰ������������������ϵ���ҽ�����Ϸ����ſ��Ը�ע���������� ���ں�λ��ע
	TWOCARD_INFO_C4, //����֮ǰ������ĸ����ĸ����ϵ���ҽ�����Ϸ����ſ��Ը�ע����������
	TWOCARD_INFO_RR, // ��Ӧ���ټ�ע
	TWOCARD_INFO_F, // ��Ӧ������

	TWOCARD_INFO_BUTT
}TWOCARD_INFO_BEHAIVOR;

INIT_TWOCARD_INFO_EXTEND Init_TwoCard_Info_Extend[INIT_TWOCARD_NUM_EXTEND] =
{
    {14140,TWOCARD_INFO_R,TWOCARD_INFO_RR},
    {13130,TWOCARD_INFO_R,TWOCARD_INFO_RR},
    {12120,TWOCARD_INFO_R,TWOCARD_INFO_RR},
    {14131,TWOCARD_INFO_R,TWOCARD_INFO_RR},
    {11110,TWOCARD_INFO_R,TWOCARD_INFO_C},
    {10100,TWOCARD_INFO_R,TWOCARD_INFO_C},
    {14130,TWOCARD_INFO_R,TWOCARD_INFO_C},
    {14121,TWOCARD_INFO_R,TWOCARD_INFO_C},
    {9090, TWOCARD_INFO_C,TWOCARD_INFO_C2},
    {14120,TWOCARD_INFO_C,TWOCARD_INFO_C2},
    {14111,TWOCARD_INFO_C,TWOCARD_INFO_C2},
    {13121,TWOCARD_INFO_C,TWOCARD_INFO_C2},
    {13120,TWOCARD_INFO_C,TWOCARD_INFO_F},
    {8080, TWOCARD_INFO_C3,TWOCARD_INFO_C4},
    {7070, TWOCARD_INFO_C3,TWOCARD_INFO_C4},
    {6060, TWOCARD_INFO_C3,TWOCARD_INFO_C4},
    {5050, TWOCARD_INFO_C3,TWOCARD_INFO_C4},
    {4040, TWOCARD_INFO_C3,TWOCARD_INFO_C4},
    {3030, TWOCARD_INFO_C3,TWOCARD_INFO_C4},
    {2020, TWOCARD_INFO_C3,TWOCARD_INFO_C4},
    {14021,TWOCARD_INFO_C3,TWOCARD_INFO_C4},
    {14031,TWOCARD_INFO_C3,TWOCARD_INFO_C4},
    {14041,TWOCARD_INFO_C3,TWOCARD_INFO_C4},
    {14051,TWOCARD_INFO_C3,TWOCARD_INFO_C4},
    {14061,TWOCARD_INFO_C3,TWOCARD_INFO_C4},
    {14071,TWOCARD_INFO_C3,TWOCARD_INFO_C4},
    {14081,TWOCARD_INFO_C3,TWOCARD_INFO_C4},
    {14091,TWOCARD_INFO_C3,TWOCARD_INFO_C4},
    {14101,TWOCARD_INFO_C3,TWOCARD_INFO_C4},
    {13111,TWOCARD_INFO_C3,TWOCARD_INFO_C4},
    {13101,TWOCARD_INFO_C3,TWOCARD_INFO_F},
    {12111,TWOCARD_INFO_C2,TWOCARD_INFO_C4},
    {12101,TWOCARD_INFO_C3,TWOCARD_INFO_F},
    {11101,TWOCARD_INFO_C3,TWOCARD_INFO_C4},
    {10091,TWOCARD_INFO_C3,TWOCARD_INFO_C4},
    {9081,TWOCARD_INFO_C3,TWOCARD_INFO_C4},
    {8071,TWOCARD_INFO_C3,TWOCARD_INFO_C4},
    {7061,TWOCARD_INFO_C3,TWOCARD_INFO_C4},
};

//��ȡ��������֮ǰ�ж��ٴ�������
int ST_GetAlivePlayerNum(RoundInfo *pRound)
{
    int cnt = 0;
    MSG_INQUIRE_INFO *pInquireInfo = Msg_GetCurrentInquireInfo(pRound);

	for(unsigned int i = 0; i < pInquireInfo->PlayerNum; i++)
	{
		if (pInquireInfo->PlayerActions[i].Action != ACTION_fold)
		{
			cnt++;
		}
	}

	return cnt;
}
//��ȡ��������֮ǰ�ж��ټ�ע�����
int ST_GetAlivePetPlayerNum(RoundInfo *pRound)
{
    int cnt = 0;
    MSG_INQUIRE_INFO *pInquireInfo = Msg_GetCurrentInquireInfo(pRound);

	for(unsigned int i = 0; i < pInquireInfo->PlayerNum; i++)
	{
		if (pInquireInfo->PlayerActions[i].Action == ACTION_raise)
		{
			cnt++;
		}
	}

	return cnt;
}

/*��ǰ���������С�ڵ���3������2����ʱ�Ĳ��� */
/************************************************************************/
/* ���ܣ���ʼ��ͨ��Ԥ֪�����ж�                                        */
/* ���룺��ʼ��                                                       */
/* �����                                                           */
/* �����ÿ�ֿ�ʼʱ����                                                 */
/************************************************************************/
void Get_Init_Two_Card_Action_Extend(RoundInfo *pRound, PLAYER_Action * pAction)
{
	unsigned int i;
	int cardinfo;
	int Max,Min;
	CARD CardInfo[2];
	int type = 0;
	int alive_num;
	int alive_pet_num;
	int No_Pet_Action = TWOCARD_INFO_BUTT;    //��֮ǰ���˼�ע
	int With_Pet_Action = TWOCARD_INFO_BUTT;  //��֮ǰ���˼�ע

    DOT(pRound->RoundStatus);

    /* ȡ���������� */
	memcpy(CardInfo, pRound->HoldCards.Cards, sizeof(CardInfo));

	if (CardInfo[0].Point > CardInfo[1].Point)
	{
		Max = CardInfo[0].Point;
		Min = CardInfo[1].Point;
	}
	else
	{
		Max = CardInfo[1].Point;
		Min = CardInfo[0].Point;
	}
	if (CardInfo[0].Color == CardInfo[1].Color)
	{
		type = 1;
	}
	cardinfo = Max*1000 + Min*10 + type;

	for (i = 0;i<INIT_TWOCARD_NUM_EXTEND;i++)
	{
		if (Init_TwoCard_Info_Extend[i].cardinfo == cardinfo)
		{
			No_Pet_Action = Init_TwoCard_Info_Extend[i].No_Pet;
			With_Pet_Action = Init_TwoCard_Info_Extend[i].With_Pet;
			break;
		}
	}

	alive_num = ST_GetAlivePlayerNum(pRound);
	alive_pet_num = ST_GetAlivePetPlayerNum(pRound);

	if (alive_pet_num == 0)
	{
		/*������ǰ����������ע���㶼Ҫ��ע*/
		if (No_Pet_Action == TWOCARD_INFO_R)
		{
		    return SGT_BuildAction(pAction, ACTION_raise, 1);
		}
		/*�����ж��ٸ���ҽ�����Ϸ���㶼Ҫ��ע*/
		else if(No_Pet_Action == TWOCARD_INFO_C)
		{
		    return SGT_BuildAction(pAction, ACTION_call, 0);
		}
		/*����֮ǰ������������������ϵ���ҽ�����Ϸ����ſ��Ը�ע����������*/
		else if(No_Pet_Action == TWOCARD_INFO_C2)
		{
			if (alive_num > 1)
			{
                return SGT_BuildAction(pAction, ACTION_call, 0);
			}
			else
			{
                return SGT_BuildAction(pAction, ACTION_fold, 0);
			}
		}		
		else if(No_Pet_Action == TWOCARD_INFO_C3)
		{
    		/*����֮ǰ������������������ϵ���ҽ�����Ϸ����ſ��Ը�ע����������*/
			if (alive_num > 2)
			{
                return SGT_BuildAction(pAction, ACTION_call, 0);
			}
			else
			{
                return SGT_BuildAction(pAction, ACTION_fold, 0);
			}
		}		
		else if(No_Pet_Action == TWOCARD_INFO_C4)
		{
    		/*����֮ǰ������ĸ����ĸ����ϵ���ҽ�����Ϸ����ſ��Ը�ע����������*/
			if (alive_num > 3)
			{
                return SGT_BuildAction(pAction, ACTION_call, 0);
			}
			else
			{
                return SGT_BuildAction(pAction, ACTION_fold, 0);
			}
		}
	}
	else
	{
		/*��Ӧ���ټ�ע*/
		if (With_Pet_Action == TWOCARD_INFO_RR)
		{
            return SGT_BuildAction(pAction, ACTION_raise, 1);
		}		
		else if(With_Pet_Action == TWOCARD_INFO_C)
		{
    		/*�����ж��ٸ���ҽ�����Ϸ���㶼Ҫ��ע*/
            return SGT_BuildAction(pAction, ACTION_call, 0);
		}		
		else if(With_Pet_Action == TWOCARD_INFO_C2)
		{
            /*����֮ǰ������������������ϵ���ҽ�����Ϸ����ſ��Ը�ע����������*/		
			if (alive_num > 1)
			{
                return SGT_BuildAction(pAction, ACTION_call, 0);
			}
			else
			{
                return SGT_BuildAction(pAction, ACTION_fold, 0);
			}
		}
		/*����֮ǰ������ĸ����ĸ����ϵ���ҽ�����Ϸ����ſ��Ը�ע����������*/
		else if(With_Pet_Action == TWOCARD_INFO_C4)
		{
			if (alive_num > 3)
			{
				return SGT_BuildAction(pAction, ACTION_call, 0);
			}
			else
			{
				return SGT_BuildAction(pAction, ACTION_fold, 0);
			}
		}		
		else if(With_Pet_Action == TWOCARD_INFO_F)
		{
    		/*��Ӧ������*/
			return SGT_BuildAction(pAction, ACTION_fold, 0);
		}
	}

	return SGT_BuildAction(pAction, ACTION_fold, 0);
}

/* ֻ����inquire�ж�ȡ������ */
int STG_GetAction(RoundInfo * pRound, char ActionBuf[128])
{
    const char * pAction = NULL;
    PLAYER_Action  Action = {ACTION_fold};

    DOT(pRound->PublicCards.CardNum);

    switch (pRound->PublicCards.CardNum)
    {
        default:
        case 0:/* ֻ����������ʱ��inqurie */
            Get_Init_Two_Card_Action_Extend(pRound, &Action);
            break;
        case 3:     /* ���Ź��ƺ��inqurie */
            Get_Init_Two_Card_Action_Extend(pRound, &Action);
            break;
        case 4:     /* ���Ź��ƺ��inqurie */
            Get_Init_Two_Card_Action_Extend(pRound, &Action);
            break;
        case 5:
            STG_GetRiverAction(pRound, &Action);
            break;
    }

    pAction = GetActionName(Action.ActType);
    if (Action.ActType == ACTION_raise)
    {
        return sprintf(ActionBuf, "%s %d", pAction, Action.Args);
    }
    else
    {
        return sprintf(ActionBuf, "%s", pAction);
    }
    
}

/* ����inquire�Ļظ� */
void STG_Inquire_Action(RoundInfo * pRound)
{
    //TRACE("Response check.\r\n");
    //const char* response = "check";
    char ActionBufer[128] = {0};
    int ResNum = STG_GetAction(pRound, ActionBufer);
    printf("Get Action:%s.\r\n", ActionBufer);
    ResponseAction(ActionBufer, ResNum);
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

    Debug_PrintChardInfo(__FILE__, __LINE__, pCards, CardNum);

    memset(&AllPoints, 0, sizeof(AllPoints));
    memset(&AllColors, 0, sizeof(AllColors));
    memset(FlushCards, 0, sizeof(FlushCards));

    STG_SortCardByPoint(pCards, CardNum);

    /* �������ľ������һ�� */
    MaxPoints[CARD_TYPES_High] = pCards[CardNum - 1].Point;

    //TRACE("STG_GetCardTypes:%d; \r\n", CardNum);

    for (index = 0; index < CardNum; index ++)
    {
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
            if ((Pairs >= 1) || (Three >= 2))
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

void STG_AnalyseWinCard_AllCards(CARD AllCards[7], int WinIndex, int PlayerNum)
{
    CARD_POINT MaxPoints[CARD_TYPES_Butt] = {(CARD_POINT)0};
    CARD_TYPES CardType = CARD_TYPES_None;
    CardType = STG_GetCardTypes(AllCards, 7, MaxPoints);
    //
    g_stg.AllWinCards[PlayerNum - 1][CardType][MaxPoints[CardType]].ShowTimes ++;
    if (WinIndex == 1)
    {
        g_stg.AllWinCards[PlayerNum - 1][CardType][MaxPoints[CardType]].WinTimes ++;
    }
//    printf("Save analyse data:[%d]Type %s, max %d, index %d;\r\n",
//           PlayerNum,
//           Msg_GetCardTypeName(CardType),
//          (int)MaxPoints[CardType], WinIndex);
    return;
}

/* ������ѡ�ֵ����빫�Ƶ���ϣ�Ȼ���¼Ӯ�ƺͳ����ƵĴ��� */
void STG_AnalyseWinCard(RoundInfo *pRound)
{
    CARD AllCards[7] = {(CARD_POINT)0};
    int index = 0;
    MSG_SHOWDWON_PLAYER_CARD * pPlayCard = NULL;

//    printf("pRound->ShowDown.PlayerNum =%d;\r\n",  pRound->ShowDown.PlayerNum );
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
        STG_AnalyseWinCard_AllCards(AllCards,
                                    pRound->ShowDown.Players[index].Index,
                                    pRound->ShowDown.PlayerNum);
    }
}

/* ��̬����������Ϊ */
void STG_AnalysePlayer(RoundInfo *pRound)
{
    /*  */

    return;
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
            //usleep(1000);
            continue;
        }

        pRound = (RoundInfo *)pQueueEntry->pObjData;

        //Debug_ShowRoundInfo(pRound);

        /* ����Ӯ������ */
        STG_AnalyseWinCard(pRound);

        /* ����������Ϊ */
        STG_AnalysePlayer(pRound);

        /* �ͷ�һ�ֵ��ڴ� */
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

