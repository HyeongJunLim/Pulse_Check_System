#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>      // Ÿ�̸� ����� ���� ���
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <semaphore.h>


#define SETTING_FILE_ADDRESS	"CLIENT_SETTING.txt"
//---------------------- ������ ���� -----------------------
#define STATUS_WARNING		0		// ������´� 0
#define STATUS_RUN		1		// �������� ���´� 1
#define STATUS_EXIT		2		// ����� ���´� 2
#define STATUS_FULL_QUEUE		3		// ť�� ��á����� 3
#define STATUS_TIME_OVER		4		// Ÿ�ӿ����ϰ�� 4
//---------------------- ����� ���� -----------------------
#define BUF_MAX_SIZE		3		// ������ �ְ���� buf ũ�� 3
#define	BUF_STATUS		0		// buf���� ���°��� ���ִ� ��ġ 0
#define BUF_PULSE			1		// buf���� �ƹڰ��� ���ִ� ��ġ 1
#define BUF_TIME			2		// buf���� �ð����� ���ִ� ��ġ 2
//-------------------- ��buf ���� ���� ---------------------
#define SERVER_OFF		0		// ���� �����ִ� ����
#define SERVER_ON		1		// ���� �����ִ� ����
//---------------------- �輭�� ���� -----------------------
#define SWITCH_OFF		0		// ���� �ƹڰ� ��� X
#define SWITCH_ON		1		// ���� �ƹڰ� ��� O
#define RE_RUN			1
#define EXIT			2
//---------------------- �谢�� ���� -----------------------

// �Լ�����
void PrintError(char *msg);				// ���� ȣ�� �Լ�
void PrintIndex(void);				// �ε��� ȭ�� ��� �Լ�
void * ThreadFunc1(void *arg);
void * ThreadFunc2(void *arg);
// �������� ����
char			SERV_IP[20];	// CLIENT_SETTING.txt�κ��� �޾ƿ��� ���� ������
int			SERV_PORT;	// CLINET_SETTING.txt�κ��� �޾ƿ��� ���� ��Ʈ
// -----------------------------------------------------------------------------------------------------
int sock;
struct sockaddr_in serv_adr;
//------------------------------------------------------------------------------------------------------
int			buf[BUF_MAX_SIZE];		// ������ �ְ���� buf
int			cmd;			// �޴� ������ ���� ���� cmd
int			type;
				// �����κ��� rcv�� ������������ send�� �׸��ΰ��ϴ� ����
int			time_var;			// ������ ���� ������
int			switch_var;		// �����ƹڰ� ��¿� ����ġ����	
int			check;			// �ð� üũ�� ����
static sem_t	delay1, delay2;
FILE			*fp;			// ����������
int main()
{
	pthread_t thread1, thread2;
	// ���� ���̺귯�� �ʱ�ȭ
	// ���� ���� - UDP
	sock = socket(PF_INET, SOCK_DGRAM, 0);
	if(sock == -1)
		PrintError("UDP ���� ���� ����");

	chdir("SETTING");
	fp = fopen(SETTING_FILE_ADDRESS, "r");
	fscanf(fp, "SERVER_IP = %s \n", SERV_IP);
	fscanf(fp, "SERVER_PORT = %d \n", &SERV_PORT);

	//���� �ּҼ���
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = inet_addr(SERV_IP);
	serv_adr.sin_port = htons(SERV_PORT);
// -------------------------------- ����� ���� UDP Ŭ���̾�Ʈ ���� �Ϸ� --------------------------------
	srand((unsigned)time(NULL));								// �������� ��� �ٲ�� ����
	sem_init(&delay1, 1, 1);
	sem_init(&delay2, 0, 1);
	
	type=SERVER_ON;
	pthread_create(&thread1, NULL, ThreadFunc1, NULL);
	pthread_create(&thread2, NULL, ThreadFunc2, NULL);

	PrintIndex();						// �ε���ȭ�� ���
	fflush(stdin);
	printf("> �� �ʸ��� �������� �����ðڽ��ϱ�? ");
	scanf("%d",&time_var);

	while( 1 )	{
		if(type == SERVER_OFF)	{
			if( buf[BUF_STATUS] == STATUS_EXIT )	{
				PrintIndex();
				printf(">>���� �ƹڰ� : %d \n", buf[BUF_PULSE]);
				printf(">>> �ƹڰ� ���۽���. ������ �������ϴ�. \n");
				break;
			}
			else if( buf[BUF_STATUS] == STATUS_FULL_QUEUE)	{
				PrintIndex();
				printf(">>���� �ƹڰ� : %d \n", buf[BUF_PULSE]);
				printf(">>> �ƹڰ� ���۽���. ������ ��á���ϴ�. \n");
				break;
			}
			else	{
				PrintIndex();
				printf(">>���� �ƹڰ� : %d \n", buf[BUF_PULSE]);
				printf(">>> �ƹڰ� ���۽���. ������ �����ֽ��ϴ�. \n");
				break;
			}	
		}
		sem_wait(&delay1);

		if(cmd == EXIT)
			break;

		if( switch_var == SWITCH_ON )	{
			PrintIndex();
			printf(">> ���� �ƹڰ� : %d \n", buf[BUF_PULSE]);
		}
		buf[BUF_STATUS] = STATUS_RUN;								// ������� RUN����	buf[1 (=BUF_STATUS) ]�� ����
		buf[BUF_PULSE] = ( rand()%200 ) + 1;								// 1~200 ������ ���� buf[0 (=BUF_PULSE) ]�� ����
		buf[BUF_TIME] = (int)time(NULL);								// ���� �ð��� buf[2 (=BUF_TIME) ]�� ����			
		if( switch_var == SWITCH_ON)	
				printf(">> ���� PULSE�� = %d \n", buf[BUF_PULSE]);
		else if( switch_var == SWITCH_OFF)	{
			PrintIndex();
			printf(">> ���� PULSE�� = %d \n", buf[BUF_PULSE]);
		}
		

		sendto( sock, (char *)buf, sizeof(buf), 0, (struct sockaddr *)&serv_adr  , sizeof(serv_adr) );			// ������ �� Ŭ���̾�Ʈ���� ������ ����
	
		sem_post(&delay2);
	}
	pthread_join( thread1, NULL );
	sem_destroy(&delay1);
	sem_destroy(&delay2);
	
	
	return 0;
}

// �����޼��� ����Լ�
void PrintError(char *msg)	{
	printf(">>>[ERROR] %s \n",msg);
	exit(0);
}

// �ε���ȭ�� ��� �Լ�
void PrintIndex(void)	{
	system("clear	");
	printf("��������������������������������������������������������������������������������������������������������������������������������������������������������\n");
	printf("��                                                                          ��\n");
	printf("��                                                                          ��\n");
	printf("��                 PULSE_CHECK                                              ��\n");
	printf("��                                        System  (Client)                  ��\n");
	printf("��                                                                          ��\n");
	printf("��                                               (Running_Time : %8d��)��\n", check);
	printf("��������������������������������������������������������������������������������������������������������������������������������������������������������\n\n");
}

// rcv ������ �Լ�
void * ThreadFunc1(void *arg) {
	int ret;
	int sAddr_size;

	while( 1 )	{
		sem_wait(&delay2);

		sAddr_size = sizeof(&serv_adr);
		ret = recvfrom(sock, (char *)buf, sizeof(buf), 0, NULL, 0);	// �����κ��� ����.
		if(ret == -1)	{
			// ret ���� -1�϶�, �� �����κ��� rcv�� ���� ��������
			type = SERVER_OFF;		// type�� SERVER_OFF ���·κ���
			break;
		}
		else	{
			// �׿�. �����κ��� rcv�� �޾�����
			type = SERVER_ON;

			if( buf[BUF_STATUS] == STATUS_WARNING )	{
				// ���� buf�� ���°� STATUS_WARNING  �϶� 
				PrintIndex();
				printf("\n");
				printf(">> ���� PULSE�� = %d \t\t [ W A R N I N G ] \n", buf[BUF_PULSE]);
				printf("�ٽ� �����Ͻðڽ��ϱ�? ( 1:�簡�� / 2:���� )  : ");

				scanf("%d",&cmd);
				switch(cmd)	{
					case RE_RUN:
						// �簡�� :	1
						buf[BUF_STATUS] = STATUS_RUN;
						break;
					case EXIT:
						// ����	  :	2
						buf[BUF_STATUS] = STATUS_EXIT;
						sendto( sock, (char *)buf, sizeof(buf), 0, (struct sockaddr *)&serv_adr  , sizeof(serv_adr) );	// �ƹڰ� ������ ����
						printf("�̿����ּż� �����մϴ�. �����ϰڽ��ϴ�. \n");
						break;
				}
			}
			else if( buf[BUF_STATUS] == STATUS_EXIT )	{
				// ���� buf�� ���°� STATUS_EXIT �϶�
				printf(">>> ������ ����Ǿ����ϴ�. ���� ���� \n");
				sleep(1);
				type = SERVER_OFF;
				break;
			}
			else if(buf[BUF_STATUS] == STATUS_FULL_QUEUE )	{
				// ���� buf�� ���°� STATUS_FULL_QUEUE �϶�
				printf(">>> ���� ����͸� ���� Ŭ���̾�Ʈ�� ������ ���Ӱźδ��߽��ϴ�. ���۽��� \n");
				sleep(1);
				type = SERVER_OFF;
				break;
			}

			if(buf[BUF_STATUS] == STATUS_EXIT)
				break;
			else if(buf[BUF_STATUS] == STATUS_FULL_QUEUE)
				break;

		
			printf("\n\n%d�� �Ŀ� �������� �����մϴ�. \n",time_var);
			switch_var = SWITCH_ON;
			sleep(time_var);
			sem_post(&delay1);
		}
			
	}
	sem_post(&delay1);
	return 0;
}

// time_check �������Լ�
void * ThreadFunc2(void *arg) {
	int start_time = (int)time(NULL);
	int now_time = 0;

	while(cmd != EXIT)	{
		now_time = (int)time(NULL);
		check = now_time - start_time;
	}

	return 0;
}