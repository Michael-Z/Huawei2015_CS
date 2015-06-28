#ifndef __PLAYER_H__
#define __PLAYER_H__

#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>


typedef struct queue_entry_
{
    struct queue_entry_ * pNextMsg;
    void * pObjData;
} queue_entry;

typedef struct obj_queue_
{
    int MsgCount;
    pthread_mutex_t Lock;
    queue_entry * pFirstMsg;
    queue_entry * pLastMsg;
} obj_queue;

void QueueInit(obj_queue * pQueue);

void QueueAdd(obj_queue * pQueue, void * pMsg, int size);

queue_entry * QueueGet(obj_queue * pQueue);

#ifdef DEBUG
#define TRACE(format, args...) \
    do \
    {   \
        struct timeval time;                \
        gettimeofday(&time, NULL);          \
        printf("[%6d.%0d]", (int)time.tv_sec, (int)time.tv_usec);    \
        printf("%s:%d:", __FILE__, __LINE__);\
        printf(format, ## args);\
    }while(0)

#else

void TRACE_Log(const char *file, int len, const char *fmt, ...);

#define TRACE(format, args...) \
        TRACE_Log(__FILE__, __LINE__, format, ## args);

#endif
/*****************************************************************************************
��Ϸ����:
1.	player��serverע���Լ���id��name��reg-msg��
2.	while������2����������Ҳ���δ������������
    a)	����������Ϣ��seat-info-msg��������ׯ��
    b)	ǿ��Ѻäע��blind-msg
    c)	Ϊÿλ���ַ����ŵ��ƣ�hold-cards-msg
    d)	����ǰ��ע�� inquire-msg/action-msg����Σ�
    e)	�������Ź����ƣ�flop-msg
    f)	����Ȧ��ע��inquire-msg/action-msg����Σ�
    g)	����һ�Ź����ƣ�ת�ƣ���turn-msg
    h)	ת��Ȧ��ע�� inquire-msg/action-msg����Σ�
    i)	����һ�Ź����ƣ����ƣ���river-msg
    j)	����Ȧ��ע��inquire-msg/action-msg����Σ�
    k)	������������δ������̯�Ʊȴ�С��showdown-msg
    l)	�����ʳط�������pot-win-msg
3.	��������������game-over-msg��
******************************************************************************************/

/*****************************************************************************************
��Ƭ���ֵĸ���:
Poker Hand      �Ƶ����        ���۳��ָ���        ���Գ��ָ���(3�����)
Royal Flush     �ʼ�ͬ��˳      649739:1            270:0
Straight Flush  ͬ��˳          64973:1             270:0
Four-of-a-Kind  ����            4164:1              270:2
Full House      ��«            693:1               270:12
Flush           ͬ��ɫ          508:1               270:23
------------------------------------------------------------------------------------------
Straight        ˳��            254:1               270:49
Three-of-a-Kind ����            46:1                270:48
TwoPair         ����            20:1                270:181
OnePair         һ��            1.25:1              270:334
NoPair          û����          1.002:1             270:158

����ͬ��˳  > ͬ��˳         > ����           > ��«       > ͬ��  > ˳��     > ����            > ����    > һ��    > ����
Royal_Flush > Straight_Flush > Four_Of_A_Kind > Full_House > Flush > Straight > Three_Of_A_Kind > TwoPair > OnePair > High

����: ��ע > ���� > ��ע(����)

*****************************************************************************************/

/* �Ƶ�������� */
typedef enum CARD_TYPES_
{
    CARD_TYPES_None = 0,
    CARD_TYPES_High,
    CARD_TYPES_OnePair,
    CARD_TYPES_TwoPair,
    CARD_TYPES_Three_Of_A_Kind,
    CARD_TYPES_Straight,
    CARD_TYPES_Flush,
    CARD_TYPES_Full_House,
    CARD_TYPES_Four_Of_A_Kind,
    CARD_TYPES_Straight_Flush,
    CARD_TYPES_Royal_Flush,
    CARD_TYPES_Butt,
} CARD_TYPES;

/* ������Ϸ���̶���ĸ�����Ϣ���� */
typedef enum SER_MSG_TYPES_
{
    SER_MSG_TYPE_none = 0,
    // ��������player����Ϣ
    SER_MSG_TYPE_seat_info,
    SER_MSG_TYPE_blind,
    SER_MSG_TYPE_hold_cards,
    SER_MSG_TYPE_hold_cards_inquire,
    //
    SER_MSG_TYPE_flop,
    SER_MSG_TYPE_flop_inquire,
    SER_MSG_TYPE_turn,
    SER_MSG_TYPE_turn_inquire,
    SER_MSG_TYPE_river,
    SER_MSG_TYPE_river_inquire,
    SER_MSG_TYPE_showdown,
    SER_MSG_TYPE_pot_win,
    SER_MSG_TYPE_notify,
    SER_MSG_TYPE_game_over,
    //
} SER_MSG_TYPES;

/* ��Ƭ��ɫ */
typedef enum CARD_COLOR_
{
    CARD_COLOR_Unknow,          /* ���ڱ�ʶ���ֵ�δ֪��Ƭ */
    CARD_COLOR_SPADES = 1,
    CARD_COLOR_HEARTS,
    CARD_COLOR_CLUBS,
    CARD_COLOR_DIAMONDS,
    CARD_COLOR_BUTT,
} CARD_COLOR;

/* ��Ƭ��С */
typedef enum CARD_POINTT_
{
    CARD_POINTT_Unknow,      /* ���ڱ�ʶ���ֵ�δ֪��Ƭ */
    CARD_POINTT_2 = 2,
    CARD_POINTT_3,
    CARD_POINTT_4,
    CARD_POINTT_5,
    CARD_POINTT_6,
    CARD_POINTT_7,
    CARD_POINTT_8,
    CARD_POINTT_9,
    CARD_POINTT_10,
    CARD_POINTT_J,
    CARD_POINTT_Q,
    CARD_POINTT_K,
    CARD_POINTT_A,
    CARD_POINTT_BUTT,
} CARD_POINT;

/* play card */
typedef struct CARD_
{
    CARD_POINT Point;
    CARD_COLOR Color;
} CARD;

/* ��ҵĴ������ */
typedef enum PLAYER_Action_
{
    ACTION_NONE,
    ACTION_fold,
    ACTION_check,       /* ���ƣ�����ǰ�����ң�ʲôҲ�������ѻ������������ */
    ACTION_call,        /* ��������ǰ������raise������re-raise��Ҳ�����ƣ���call�� */
    ACTION_raise,       /* ��1��ע */
//    ACTION_raise_5,     /* ��2-5��ע */
//    ACTION_raise_10,    /* ��6-10��ע */
//    ACTION_raise_20,    /* ��11-20��ע */
//    ACTION_raise_much,  /* �ӵĸ��� */
    ACTION_allin,
    ACTION_BUTTON,
    //
} PLAYER_Action;

typedef enum PLAYER_SEAT_TYPES_
{
    // һ����player����
    PLAYER_SEAT_TYPES_none,
    PLAYER_SEAT_TYPES_button,
    PLAYER_SEAT_TYPES_small_blind,
    PLAYER_SEAT_TYPES_big_blind,
} PLAYER_SEAT_TYPES;

/* ��Ϣ��ȡʱ���ݽṹ */
typedef struct MSG_READ_INFO_
{
    const char * pMsg;
    int MaxLen;
    int Index;
    void * pData;
} MSG_READ_INFO;

/* �����λ��Ϣ */
typedef struct PLAYER_SEAT_INFO_
{
    unsigned int PlayerID;
    PLAYER_SEAT_TYPES Type;
    int Jetton;
    int Money;
} PLAYER_SEAT_INFO;

/* ÿһ���е������λ */
typedef struct MSG_SEAT_INFO_
{
    int PlayerNum;
    PLAYER_SEAT_INFO Players[8];
} MSG_SEAT_INFO;

/* ÿһ�ֵĹ�������Ϣ */
typedef struct MSG_CARD_INFO_
{
    int CardNum;
    CARD Cards[5];    /* ǰ3�Ź��ƣ���4��ת�ƣ���5�ź��ƣ���������ƣ���ֻ������ */
} MSG_CARD_INFO;

typedef struct PLAYER_JETTION_INFO_
{
    unsigned int PlayerID;
    int Jetton;
} PLAYER_JETTION_INFO;

typedef struct MSG_BLIND_INFO_
{
    int BlindNum;
    PLAYER_JETTION_INFO BlindPlayers[2];    /* ֻ�д�Сäע */
} MSG_BLIND_INFO;

typedef struct MSG_POT_WIN_INFO_
{
    int PotWinCount;
    PLAYER_JETTION_INFO PotWin[2];  /* ���ڶ����ƽ������������һ�������ˣ���������ͳ���ˣ���ϵ���� */
} MSG_POT_WIN_INFO;

typedef struct MSG_SHOWDWON_PLAYER_CARD_
{
    int Index;          /* ѡ������ */
    unsigned int PlayerID;
    //char PlayerID[32];
    CARD HoldCards[2];
    char CardType[32];
} MSG_SHOWDWON_PLAYER_CARD;

/* ��һ��ÿ�ֶ���show��Ϣ */
typedef struct MSG_SHOWDWON_INFO_
{
    int CardNum;
    int PlayerNum;
    CARD PublicCards[5];                    /* 5�Ź��� */
    MSG_SHOWDWON_PLAYER_CARD Players[8];    /* ѡ�ֵ����� */
} MSG_SHOWDWON_INFO;

/* ��ѯ��ҵ�inquire��Ϣ */
typedef struct MSG_INQUIRE_PLAYER_ACTION_
{
    unsigned int PlayerID;
    int Jetton;     /* ������ */
    int Money;      /* ����� */
    int Bet;        /* ������ע�� */
    PLAYER_Action Action;   /* ��ǰ���� */
} MSG_INQUIRE_PLAYER_ACTION;

/* ѯ�����ʱ��ÿ�����8����ң����һ��ѯ���У�����8���ģ�ѭ�����Ǽ��� */
typedef struct MSG_INQUIRE_INFO_
{
    int PlayerNum;  /* ��ȡinquire��Ϣʱ��д�������� */
    int TotalPot;   /* �����ܲʳ��� */
    MSG_INQUIRE_PLAYER_ACTION PlayerActions[8];
} MSG_INQUIRE_INFO;

/* ÿһ����Ϣ */
typedef struct RoundInfo_
{
    int RoundIndex;
    SER_MSG_TYPES CurrentMsgType;
    SER_MSG_TYPES RoundStatus;                /* ��ǰ��״̬ */

    int RaiseTimes;
    MSG_SEAT_INFO SeatInfo;
    MSG_CARD_INFO PublicCards;
    MSG_CARD_INFO HoldCards;
    MSG_BLIND_INFO Blind;
    MSG_SHOWDWON_INFO ShowDown;
    MSG_POT_WIN_INFO PotWin;
    //
    MSG_INQUIRE_INFO Inquires[4];   /* ���ơ����ơ�ת�ơ����Ʒֱ�4�β�ѯ */
    /* ����Ҫnotify��Ϣ����inquire���Ѿ�ȫ������ */
} RoundInfo;

typedef void (* MSG_LineReader)(char Buffer[256], RoundInfo * pArg);
typedef void (* STG_Action)(RoundInfo * pArg);

/* ÿ����Ϣ�����Ľṹ */
typedef struct MSG_NAME_TYPE_ENTRY_
{
    const char * pStartName;
    const char * pEndName;
    int NameLen;
    SER_MSG_TYPES MsgType;
    MSG_LineReader LinerReader;
    STG_Action Action;
} MSG_NAME_TYPE_ENTRY;

/* ѡ��ģ�� */
typedef struct PLAYER_
{
    unsigned int PlayerID;
    int PlayerLevel;        /* ѡ�ָ��Լ���, 1-9����1������9���� */

    int RoundIndex;         /* ���� */

    /* holdʱ����Ϊ */

    /* flopʱ����Ϊ */

    /* turnʱ����Ϊ */

    /* riverʱ����Ϊ */

} PLAYER;

/****************************************************************************************/

void ResponseAction(const char * pMsg, int size);

SER_MSG_TYPES Msg_GetMsgType(const char * pMsg, int MaxLen);

void Msg_Read_Ex(const char * pMsg, int MaxLen, RoundInfo * pRound);

const char * Msg_GetMsgNameByType(SER_MSG_TYPES Type);

CARD_TYPES STG_GetCardTypes(CARD *pCards, int CardNum, CARD_POINT MaxPoints[CARD_TYPES_Butt]);

const char *Msg_GetCardTypeName(CARD_TYPES Type);

const char * GetCardPointName(CARD_POINT point);

void Debug_ShowRoundInfo(RoundInfo *pRound);

const char * GetCardColorName(CARD * pCard);

void Debug_PrintChardInfo(CARD * pCard, int CardNum);

void Debug_PrintShowDown(MSG_SHOWDWON_INFO *pShowDown);

/*****************************stg ����***********************************************************/

/* ÿһ�ֿ�ʼǰ���������� */
void STG_SaveRoundData(RoundInfo * pRoundInfo);

void STG_SaveStudyData(void);

void STG_Init(void);

void STG_Dispose(void);

const char * STG_GetAction(RoundInfo * pRound);

const char * GetActionName(PLAYER_Action act);

/****************************************************************************************/

/*****************************player ����***********************************************/

/****************************************************************************************/

#endif

