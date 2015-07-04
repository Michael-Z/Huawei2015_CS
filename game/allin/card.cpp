#include <stdio.h>
#include <stdlib.h>  //
#include <string.h>  //strlen
#include <unistd.h>  //usleep/close
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "Player.h"

/* �������͵Ĵ���ͼ��� */

/*****************************************************************************
 Function name: Card_Calculate
 Description  : ���ݹ��ƺ������������ƵĽ��
 Input        : CARD pPublicCards[3]    ���Ź���(���ܰ���)
                CARD * pTurnCard        ת��
                CARD * pRiverCard       ����
                CARD HandCards[2]       ��������
 Output       : None
 Return Value : 
    Successed : CARD_TYPES
    Failed    : 
*****************************************************************************/
CARD_TYPES Card_Calculate(CARD pPublicCards[3], CARD * pTurnCard, CARD * pRiverCard, CARD HandCards[2])
{
    CARD_TYPES CardType = CARD_TYPES_High;
    return CardType;
}

CARD_TYPES Card_CalculateEx(CARD * pPublicCards, int CardNum)
{
    CARD_TYPES CardType = CARD_TYPES_High;
    return CardType;
}

/* �ж����������Ƿ���һ�� */
bool Card_IsHoldPair(CARD HoldCards[2])
{
    return HoldCards[0].Point == HoldCards[1].Point;
}

/* �ж����������Ƿ���ͬ��ɫ */
bool Card_IsHoldSameColor(CARD HoldCards[2])
{
    return HoldCards[0].Color == HoldCards[1].Color;
}



