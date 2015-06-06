#include <stdio.h>
//#include <WinSock2.h>
#include <assert.h>
#include <time.h>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "Player.h"
int m_socket_id = -1;
char player_name[32] = "2026";      //����Ķ���
char server_ip[32] = "127.0.0.1";
unsigned long server_port = 6000;

void ParseCmdPara(int argc, char * argv[],unsigned long* player_id, unsigned long* server_port_no,char* server_ip)
{
    //��һ����������Ϊserver ip����ѡ��
    if(argc > 1)
    {
        strncpy(server_ip, argv[1], 32);
    }

    //�ڶ�����������Ϊserver�˿ںţ���ѡ��
    if(argc > 2)
    {
        *server_port_no = atol(argv[2]);
    }

    unsigned long exenamelen = (unsigned long)strlen(argv[0]);
    const char* ch = NULL;
    for(ch=argv[0] + exenamelen; ch!=argv[0];ch--)
    {
        if(*ch == '\\')
        {
            ch++;
            break;
        }
    }

    *player_id=atol(ch);
}

int main_ex(int argc, char * argv[])
{
    int w_version_requested;
    WSADATA ws_adata;
    int err;
    unsigned long player_id = 0;

    ParseCmdPara(argc, argv, &player_id, &server_port, server_ip);

#if 0
    ANSItoUTF8(player_name);

    srand( (unsigned)time( NULL ) * player_id);

	SetPlayerId(player_id);

    w_version_requested = MAKEWORD(1,1);
    err = WSAStartup(w_version_requested,&ws_adata);

    if(0 != err)
    {
        return;
    }

    if((LOBYTE(ws_adata.wVersion)!=1)||

        (HIBYTE(ws_adata.wVersion)!=1))
    {
        WSACleanup();
        return;
    }
#endif

    GameStart();

    int sock_client = socket(AF_INET,SOCK_STREAM, 0);
    struct sockaddr_in addr_srv;

    addr_srv.sin_family = AF_INET;
    addr_srv.sin_port = htons((unsigned short)server_port);
    addr_srv.sin_addr.s_addr = inet_addr(server_ip);

//    stAdd.sin_family = AF_INET;
//    stAdd.sin_port = g_EchoServer.stArg.mPort;
//    stAdd.sin_addr.s_addr = g_EchoServer.stArg.mAddr;

    while(0 != connect(sock_client,(struct sockaddr*)&addr_srv, sizeof(struct sockaddr)))
    {
        sleep(10);
    };

    char hello[64]={0};

 //   sprintf(hello, "reg: %d %s \n", player_id, player_name);

    send(sock_client,hello, (int)strlen(hello)+1, 0);

    while(1)
    {
        //printf("\n-------round--------\n");
        if(GAME_OVER == RoundEntry(sock_client))
        {
            break;
        }
    }

	GameOver();
    shutdown(sock_client, SHUT_RDWR);
	close(sock_client);
	//WSACleanup();
    return 0;
}

int main(int argc, char* argv[])
{
    int ret = 0;
    pthread_t subThread;

    if(argc != 6)
    {
        return -1;
    }

    GameStart();

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
    printf("smart try to connect server(%s:%u)\n", inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));
    while(0 != connect(m_socket_id, (sockaddr*)&server_addr, sizeof(sockaddr)))
    {
        usleep(100*1000);
    };
    printf("smart connect server success\n", inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));

    /* ��serverע�� */
    char reg_msg[50]="";
    snprintf(reg_msg, sizeof(reg_msg) - 1, "reg: %d %s \n", my_id, my_name);
    send(m_socket_id, reg_msg, (int)strlen(reg_msg)+1, 0);

    //TRACE("%d\r\n", g_msg_queue.exit);

    /* ��ʼ��Ϸ */
    while(1)
    {
        if(GAME_OVER == RoundEntry(m_socket_id))
        {
            break;
        }
    }

	GameOver();
    shutdown(m_socket_id, SHUT_RDWR);
	close(m_socket_id);

    return 0;
}

