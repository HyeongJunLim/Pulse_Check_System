#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>      // 타이머 사용을 위한 헤더
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <semaphore.h>


#define SETTING_FILE_ADDRESS	"CLIENT_SETTING.txt"
//---------------------- ↑파일 설정 -----------------------
#define STATUS_WARNING		0		// 멈춘상태는 0
#define STATUS_RUN		1		// 진행중인 상태는 1
#define STATUS_EXIT		2		// 종료된 상태는 2
#define STATUS_FULL_QUEUE		3		// 큐가 꽉찼을경우 3
#define STATUS_TIME_OVER		4		// 타임오버일경우 4
//---------------------- ↑상태 정의 -----------------------
#define BUF_MAX_SIZE		3		// 서버와 주고받을 buf 크기 3
#define	BUF_STATUS		0		// buf에서 상태값이 들어가있는 위치 0
#define BUF_PULSE			1		// buf에서 맥박값이 들어가있는 위치 1
#define BUF_TIME			2		// buf에서 시간값이 들어가있는 위치 2
//-------------------- ↑buf 관련 정의 ---------------------
#define SERVER_OFF		0		// 서버 꺼져있는 상태
#define SERVER_ON		1		// 서버 켜져있는 상태
//---------------------- ↑서버 정의 -----------------------
#define SWITCH_OFF		0		// 이전 맥박값 출력 X
#define SWITCH_ON		1		// 이전 맥박값 출력 O
#define RE_RUN			1
#define EXIT			2
//---------------------- ↑각종 값들 -----------------------

// 함수선언
void PrintError(char *msg);				// 에러 호출 함수
void PrintIndex(void);				// 인덱스 화면 출력 함수
void * ThreadFunc1(void *arg);
void * ThreadFunc2(void *arg);
// 전역변수 설정
char			SERV_IP[20];	// CLIENT_SETTING.txt로부터 받아오는 서버 아이피
int			SERV_PORT;	// CLINET_SETTING.txt로부터 받아오는 서버 포트
// -----------------------------------------------------------------------------------------------------
int sock;
struct sockaddr_in serv_adr;
//------------------------------------------------------------------------------------------------------
int			buf[BUF_MAX_SIZE];		// 서버와 주고받을 buf
int			cmd;			// 메뉴 구현을 위한 변수 cmd
int			type;
				// 서버로부터 rcv를 받지못했을때 send도 그만두게하는 변수
int			time_var;			// 센서값 보낼 딜레이
int			switch_var;		// 이전맥박값 출력용 스위치변수	
int			check;			// 시간 체크용 변수
static sem_t	delay1, delay2;
FILE			*fp;			// 파일포인터
int main()
{
	pthread_t thread1, thread2;
	// 소켓 라이브러리 초기화
	// 소켓 생성 - UDP
	sock = socket(PF_INET, SOCK_DGRAM, 0);
	if(sock == -1)
		PrintError("UDP 소켓 생성 실패");

	chdir("SETTING");
	fp = fopen(SETTING_FILE_ADDRESS, "r");
	fscanf(fp, "SERVER_IP = %s \n", SERV_IP);
	fscanf(fp, "SERVER_PORT = %d \n", &SERV_PORT);

	//서버 주소설정
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = inet_addr(SERV_IP);
	serv_adr.sin_port = htons(SERV_PORT);
// -------------------------------- 통신을 위한 UDP 클라이언트 설정 완료 --------------------------------
	srand((unsigned)time(NULL));								// 랜덤숫자 계속 바뀌게 설정
	sem_init(&delay1, 1, 1);
	sem_init(&delay2, 0, 1);
	
	type=SERVER_ON;
	pthread_create(&thread1, NULL, ThreadFunc1, NULL);
	pthread_create(&thread2, NULL, ThreadFunc2, NULL);

	PrintIndex();						// 인덱스화면 출력
	fflush(stdin);
	printf("> 몇 초마다 센서값을 보내시겠습니까? ");
	scanf("%d",&time_var);

	while( 1 )	{
		if(type == SERVER_OFF)	{
			if( buf[BUF_STATUS] == STATUS_EXIT )	{
				PrintIndex();
				printf(">>이전 맥박값 : %d \n", buf[BUF_PULSE]);
				printf(">>> 맥박값 전송실패. 서버가 닫혔습니다. \n");
				break;
			}
			else if( buf[BUF_STATUS] == STATUS_FULL_QUEUE)	{
				PrintIndex();
				printf(">>이전 맥박값 : %d \n", buf[BUF_PULSE]);
				printf(">>> 맥박값 전송실패. 서버가 꽉찼습니다. \n");
				break;
			}
			else	{
				PrintIndex();
				printf(">>이전 맥박값 : %d \n", buf[BUF_PULSE]);
				printf(">>> 맥박값 전송실패. 서버가 닫혀있습니다. \n");
				break;
			}	
		}
		sem_wait(&delay1);

		if(cmd == EXIT)
			break;

		if( switch_var == SWITCH_ON )	{
			PrintIndex();
			printf(">> 이전 맥박값 : %d \n", buf[BUF_PULSE]);
		}
		buf[BUF_STATUS] = STATUS_RUN;								// 현재상태 RUN으로	buf[1 (=BUF_STATUS) ]에 저장
		buf[BUF_PULSE] = ( rand()%200 ) + 1;								// 1~200 사이의 숫자 buf[0 (=BUF_PULSE) ]에 저장
		buf[BUF_TIME] = (int)time(NULL);								// 현재 시간값 buf[2 (=BUF_TIME) ]에 저장			
		if( switch_var == SWITCH_ON)	
				printf(">> 현재 PULSE값 = %d \n", buf[BUF_PULSE]);
		else if( switch_var == SWITCH_OFF)	{
			PrintIndex();
			printf(">> 현재 PULSE값 = %d \n", buf[BUF_PULSE]);
		}
		

		sendto( sock, (char *)buf, sizeof(buf), 0, (struct sockaddr *)&serv_adr  , sizeof(serv_adr) );			// 센서값 및 클라이언트상태 서버로 전송
	
		sem_post(&delay2);
	}
	pthread_join( thread1, NULL );
	sem_destroy(&delay1);
	sem_destroy(&delay2);
	
	
	return 0;
}

// 에러메세지 출력함수
void PrintError(char *msg)	{
	printf(">>>[ERROR] %s \n",msg);
	exit(0);
}

// 인덱스화면 출력 함수
void PrintIndex(void)	{
	system("clear	");
	printf("┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓\n");
	printf("┃                                                                          ┃\n");
	printf("┃                                                                          ┃\n");
	printf("┃                 PULSE_CHECK                                              ┃\n");
	printf("┃                                        System  (Client)                  ┃\n");
	printf("┃                                                                          ┃\n");
	printf("┃                                               (Running_Time : %8d초)┃\n", check);
	printf("┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛\n\n");
}

// rcv 쓰레드 함수
void * ThreadFunc1(void *arg) {
	int ret;
	int sAddr_size;

	while( 1 )	{
		sem_wait(&delay2);

		sAddr_size = sizeof(&serv_adr);
		ret = recvfrom(sock, (char *)buf, sizeof(buf), 0, NULL, 0);	// 서버로부터 받음.
		if(ret == -1)	{
			// ret 값이 -1일때, 즉 서버로부터 rcv를 받지 못했을때
			type = SERVER_OFF;		// type를 SERVER_OFF 상태로변경
			break;
		}
		else	{
			// 그외. 서버로부터 rcv를 받았을때
			type = SERVER_ON;

			if( buf[BUF_STATUS] == STATUS_WARNING )	{
				// 받은 buf의 상태가 STATUS_WARNING  일때 
				PrintIndex();
				printf("\n");
				printf(">> 현재 PULSE값 = %d \t\t [ W A R N I N G ] \n", buf[BUF_PULSE]);
				printf("다시 가동하시겠습니까? ( 1:재가동 / 2:종료 )  : ");

				scanf("%d",&cmd);
				switch(cmd)	{
					case RE_RUN:
						// 재가동 :	1
						buf[BUF_STATUS] = STATUS_RUN;
						break;
					case EXIT:
						// 종료	  :	2
						buf[BUF_STATUS] = STATUS_EXIT;
						sendto( sock, (char *)buf, sizeof(buf), 0, (struct sockaddr *)&serv_adr  , sizeof(serv_adr) );	// 맥박값 서버로 전송
						printf("이용해주셔서 감사합니다. 종료하겠습니다. \n");
						break;
				}
			}
			else if( buf[BUF_STATUS] == STATUS_EXIT )	{
				// 받은 buf의 상태가 STATUS_EXIT 일때
				printf(">>> 서버가 종료되었습니다. 전송 실패 \n");
				sleep(1);
				type = SERVER_OFF;
				break;
			}
			else if(buf[BUF_STATUS] == STATUS_FULL_QUEUE )	{
				// 받은 buf의 상태가 STATUS_FULL_QUEUE 일때
				printf(">>> 현재 모니터링 중인 클라이언트가 꽉차서 접속거부당했습니다. 전송실패 \n");
				sleep(1);
				type = SERVER_OFF;
				break;
			}

			if(buf[BUF_STATUS] == STATUS_EXIT)
				break;
			else if(buf[BUF_STATUS] == STATUS_FULL_QUEUE)
				break;

		
			printf("\n\n%d초 후에 센서값을 전송합니다. \n",time_var);
			switch_var = SWITCH_ON;
			sleep(time_var);
			sem_post(&delay1);
		}
			
	}
	sem_post(&delay1);
	return 0;
}

// time_check 쓰레드함수
void * ThreadFunc2(void *arg) {
	int start_time = (int)time(NULL);
	int now_time = 0;

	while(cmd != EXIT)	{
		now_time = (int)time(NULL);
		check = now_time - start_time;
	}

	return 0;
}