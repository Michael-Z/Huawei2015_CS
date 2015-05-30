#include <stdio.h>
#include <stdlib.h>  //
#include <string.h>  //strlen
#include <unistd.h>  //usleep/close
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "Player.h"

int m_socket_id = -1;

RoundInfo LocalRoundInfo = {0};

bool server_msg_process(int size, const char* msg)
{
    if (NULL != strstr(msg, "game-over"))
    {
        return false;
    }

    SER_MSG_TYPES msg_type = Msg_Read(msg, size, NULL, &LocalRoundInfo);

    if (msg_type == SER_MSG_TYPE_inquire)
    {
        const char* response = "check";
        send(m_socket_id, response, (int)strlen(response)+1, 0);
    }

#if 0
    if (NULL != strstr(msg, "inquire/"))
    {
        //const char* response = "all_in";
        //SER_MSG_TYPES type = Msg_GetMsgType(msg, size);
        const char* response = "check";
        //printf("get msg %d: %s", (int)type, Msg_GetMsgNameByType(type));
        send(m_socket_id, response, (int)strlen(response)+1, 0);
    }
#endif

    return true;
}

int main(int argc, char* argv[])
{
    if(argc != 6)
    {
        return -1;
    }

    /* ��ȡ�����в��� */
    in_addr_t server_ip = inet_addr(argv[1]);
    in_port_t server_port = atoi(argv[2]);
    in_addr_t my_ip = inet_addr(argv[3]);
    in_port_t my_port = atoi(argv[4]);
    int my_id = atoi(argv[5]);
    char* my_name = strrchr(argv[0], '/');
    if (my_name == NULL)
    {
        my_name = argv[0];
    }
    else
    {
        my_name++;
    }

    /* ����socket */
    m_socket_id = socket(AF_INET, SOCK_STREAM, 0);

    /* ���Լ���IP */
    sockaddr_in my_addr;
    my_addr.sin_addr.s_addr = my_ip;
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(my_port);

    long flag = 1;
    setsockopt(m_socket_id, SOL_SOCKET, SO_REUSEADDR, (char *)&flag, sizeof(flag));

    if (bind(m_socket_id, (sockaddr*)&my_addr, sizeof(sockaddr)) < 0)
    {
        printf("bind failed: %m\n");
        return -1;
    }

    /* ���ӷ����� */
    sockaddr_in server_addr;
    server_addr.sin_addr.s_addr = server_ip;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    printf("try to connect server(%s:%u)\n", inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));
    while(0 != connect(m_socket_id, (sockaddr*)&server_addr, sizeof(sockaddr)))
    {
        usleep(100*1000);
    };
    printf("connect server success\n", inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));

    /* ��serverע�� */
    char reg_msg[50]="";
    snprintf(reg_msg, sizeof(reg_msg) - 1, "reg: %d %s \n", my_id, my_name);
    send(m_socket_id, reg_msg, (int)strlen(reg_msg)+1, 0);

    /* ��ʼ��Ϸ */
    while(1)
    {
        char buffer[1024] = {'\0'};
        int size = recv(m_socket_id, buffer, sizeof(buffer) - 1, 0);
        if (size > 0)
        {
            if (!server_msg_process(size, buffer))
            {
                break;
            }
        }
    }

	close(m_socket_id);

    return 0;
}

