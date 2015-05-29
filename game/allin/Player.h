#ifndef __PLAYER_H__
#define __PLAYER_H__

#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>

#define TRACE(format, args...) \
    do \
    {\
        printf("%s:%d:", __FILE__, __LINE__);\
        printf(format, ## args);\
    }while(0)

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
} CARD_TYPES;

/* ������Ϸ���̶���ĸ�����Ϣ���� */
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

/* ������Ϣ�ṹ */
typedef struct GAME_MSG_
{
    SER_MSG_TYPES Type;
    char MsgRawData[1024];      /* ����־����ʱû��������1024����Ϣ */
} GAME_MSG;

typedef struct GAME_MSG_reg_
{
    const char * MsgName;
    //
    int PlayerID;
    char PlayerName[32];
} GAME_MSG_reg;

/* ��Ƭ��ɫ */
typedef enum CARD_COLOR_
{
    CARD_COLOR_Unknow,          /* ���ڱ�ʶ���ֵ�δ֪��Ƭ */
    CARD_COLOR_SPADES = 1,
    CARD_COLOR_HEARTS,
    CARD_COLOR_CLUBS,
    CARD_COLOR_DIAMONDS,
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
} CARD_POINT;

/* play card */
typedef struct CARD_
{
    CARD_POINT Point;
    CARD_COLOR Color;
} CARD;

typedef enum CLIENT_MSG_TYPES_
{
    // player������������Ϣ
    CLIENT_MSG_TYPES_Reg,
    CLIENT_MSG_TYPES_ACT_check,
    CLIENT_MSG_TYPES_ACT_call,
    CLIENT_MSG_TYPES_ACT_raise,
    CLIENT_MSG_TYPES_ACT_all_in,
    CLIENT_MSG_TYPES_ACT_fold,
} CLIENT_MSG_TYPES;

/* ��ҵĴ������ */
typedef enum PLAYER_Action_
{
    ACTION_check,       /* ���ƣ�����ǰ�����ң�ʲôҲ�������ѻ������������ */
    ACTION_call,        /* ��������ǰ������raise������re-raise��Ҳ�����ƣ���call�� */
    ACTION_allin,
    ACTION_raise,
    ACTION_fold,
} PLAYER_Action;

typedef enum PLAYER_SEAT_TYPES_
{
    // һ����player����
    PLAYER_SEAT_TYPES_none,
    PLAYER_SEAT_TYPES_button,
    PLAYER_SEAT_TYPES_small_blind,
    PLAYER_SEAT_TYPES_big_blind,
} PLAYER_SEAT_TYPES;

/* �����Ϣ */
typedef struct PLAYER_
{
    /* ��̬���� */
    char PlayerID[32];
    char PlayerName[32];    /* player name, length of not more than 20 bytes */

    /* ��̬player ��Ϣ */
    int Status;             /* ��ǰ�ֵĵ�ǰ״̬ */
    int SeatIndex;          /* ��ǰ�ֵ�λ�� */
    CARD HoldCards[2];      /* ѡ�ֵ����ŵ��ƣ���ȷ���� */

} PLAYER;

/****************************************************************************************/
SER_MSG_TYPES Msg_GetMsgType(const char * pMsg, int MaxLen);

const char * Msg_GetMsgNameByType(SER_MSG_TYPES Type);

/****************************************************************************************/

#endif

