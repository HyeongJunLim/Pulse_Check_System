#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>      // 타이머 사용을 위한 헤더
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <semaphore.h>

// 정의
#define SETTING_FILE_ADDRESS   "SERVER_SETTING.txt"
//---------------------- ↑파일 설정 -----------------------
#define STATUS_WARNING		0            // 멈춘상태는 0
#define STATUS_RUN            	1            // 진행중인 상태는 1
#define STATUS_EXIT            	2            // 종료된 상태는 2
#define STATUS_FULL_QUEUE     	3            // 큐가 꽉찼을경우 3
#define STATUS_TIME_OVER      	4            // 타임오버일경우 4
//---------------------- ↑상태 정의 -----------------------
#define BUF_MAX_SIZE         	3            // 서버와 주고받을 buf 크기 2
#define   BUF_STATUS            	0            // buf에서 상태값이 들어가있는 위치 0
#define BUF_PULSE          	1            // buf에서 맥박값이 들어가있는 위치 1
#define BUF_TIME         		2            // buf에서 시간값이 들어가있는 위치 2
//-------------------- ↑buf 관련 정의 ---------------------
#define CLIENT_MAX_NUM        	10           // 클라이언트 최대갯수 10
#define NODE_MAX_NUM        	100          // 노드 최대갯수 100
//---------------------- ↑각종 값들 -----------------------


// 구조체 정의
typedef struct double_list_queue *queue_pointer;
typedef struct queue_head *head_pointer;

//구조체 선언
struct double_list_queue   {
	int            pulse;
	int            status;
	queue_pointer   left_link, right_link;
};
struct queue_head   {
	queue_pointer   right_link;
	int            start_time;                 // 시작 시간을 저장
	int            now_time;                 // 현재 시간을 저장
	int            now_status;               // 현재 상태를 저장
	int            now_pulse;                // 현재 맥박값 저장
	int            sum_pulse;                // 맥박값 총합을 저장하기위한 변수
	int            cnt_pulse;                // 맥박 카운트 변수
	double         ave_pulse;              // 평균 맥박값 저장하는 변수
	int            min_pulse;              // 저장된 맥박값중 가장 작은 맥박값 저장하는 변수
	int            max_pulse;             // 저장된 맥박값중 가장 큰 맥박값 저장하는 변수
	char           ip[20];                  // 접속한 클라이언트 ip 주소 저장을 위한 변수
	int            level[10];             // 도수분포표의 계급의 도수를 count하기 위한 배열
};

// 함수 선언
void initialize_queue( head_pointer quit_queue )   // 모든 큐 초기화 or 종료된 클라이언트의 큐 초기화
int insert_queue( char *ip, int pulse );            // 큐 삽입
void PrintError( char *msg );                     // 에러메세지 출력
void PrintIndex( void );                         // 인덱스화면 출력
int SearchID( char *ip );                        // 들어갈 큐 위치찾기
void PulseMaxMin( head_pointer temp );         // 맥박의 최대, 최소값 구해서 큐에 저장
void PulseGraph( head_pointer queue );         // 맥박의 도수분포표 구하기
void PrintInfo( head_pointer queue );           // 맥박에 대한 정보 출력
void * ThreadFunc1(void *arg);
void * ThreadFunc2(void *arg);

int AllClientInfo ( void );                           // 접속된 모든 클라이언트의 정보
void PrintMenu(void);                              // 메뉴 함수
void DetailClientInfo();

// 전역변수 설정
char      SERV_IP[20];                      // SERVER_SETTING.txt로부터 받아오는 서버 아이피
int         SERV_PORT;                     // SERVER_SETTING.txt로부터 받아오는 서버 포트
int         PULSE_MIN_LIMIT;               // 맥박 최소치 변수
int         PULSE_MAX_LIMIT;               // 맥박 최대치 변수
int         TIMER;                          // 타이머 (분) 단위
//----------------------------------------------------------------------------------------------------------
int serv_sock;
struct sockaddr_in serv_adr, clnt_adr;
//----------------------------------------------------------------------------------------------------------
int         buf[BUF_MAX_SIZE];             // 클라이언트와 주고받기위한 buf배열
char      cmd;                            // 메뉴선택 변수
int         type;                           // 타입 설정
int         quit;                            // 프린트 쓰레드 종료를 위한 변수
int         ID;                             // 몇번째 queue인지 확인을위한 ID변수
int         info_id;                         // 세부정보 볼때 필요한 변수
FILE      *fp;                              // 파일 포인터
static sem_t delay;
//----------------------------------------------------------------------------------------------------------
head_pointer   queue[CLIENT_MAX_NUM];

int main()
{
	int i = 0;
	pthread_t thread1, thread2;

	// 소켓 생성 - UDP
	serv_sock = socket(PF_INET, SOCK_DGRAM, 0);
	if(serv_sock == -1)
		PrintError("UDP 소켓 생성 실패");

	chdir("SETTING");
	fp = fopen(SETTING_FILE_ADDRESS, "r");
	fscanf(fp, "IP = %s \n",SERV_IP);
	fscanf(fp, "PORT = %d \n",&SERV_PORT);
	fscanf(fp, "MIN_PULSE = %d \n", &PULSE_MIN_LIMIT);
	fscanf(fp, "MAX_PULSE = %d \n", &PULSE_MAX_LIMIT);
	fscanf(fp, "TIMER = %d", &TIMER);
	TIMER = TIMER * 1;      // TIMER을 분단위로 바꿔줌.
	if(fp==NULL)   {
		printf("파일 오픈 오류 \n");
		exit(1);
	}   
	chdir("..");	chdir("DATA");


	// 서버 주소설정
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = inet_addr(SERV_IP);
	serv_adr.sin_port = htons(SERV_PORT);


	//sAddr.sin_family      =   AF_INET;
	//sAddr.sin_addr.s_addr   =   inet_addr(SERV_IP);
	//sAddr.sin_port         =   htons(SERV_PORT);


	if(bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
		PrintError("bind() error");
	// -------------------------------- 통신을 위한 UDP 서버 설정 완료 --------------------------------
	initialize_queue(NULL);   // 리스트 초기화

	printf(" > 클라이언트 접속을 기다리는중입니다... \n");

	sem_init(&delay, 1, 1);

	pthread_create(&thread1, NULL, ThreadFunc1, NULL);
	pthread_create(&thread2, NULL, ThreadFunc2, NULL);

	while(1)   {
		scanf("%c",&cmd);               // 메뉴 선택
		fflush(stdin);
		int cnt = 0;
		//--------- 모든 클라이언트 접속종료되었을때 메뉴선택 X-------
		for(i=0; i<CLIENT_MAX_NUM; i++)   {
			if(queue[i]->right_link == NULL)
				cnt++;
		}
		if(cnt == CLIENT_MAX_NUM)   {
			system("clear");
			printf(" > 모든 클라이언트가 접속 종료되었습니다... \n");
			printf(" > 클라이언트 접속을 기다리는중입니다... \n");
			continue;
		}
		//--------------------------------------------------------------
		switch(cmd)   {
			case '1':
				// 클라이언트 세부정보 열기
				sem_wait(&delay);
				type = 1;
				printf(">> 클라이언트를 선택해주세요(1 ~ %d) : ", CLIENT_MAX_NUM);
				scanf("%d",&info_id);
				while( info_id > CLIENT_MAX_NUM || info_id < 1 || queue[info_id-1]->right_link == NULL )   {
					printf("[ ERROR ] 유효하지 않은 클라이언트 넘버입니다. 다시입력해주세요. \n");
					printf(">> 클라이언트를 선택해주세요(1 ~ %d) : ", CLIENT_MAX_NUM);
					scanf("%d",&info_id);
				}
				info_id--;
				sem_post(&delay);
				break;

			case '2':
				// 클라이언트 세부정보 닫기
				type = 0;
				break;
			case 'q':
				// 프로그램 종료하기
				pthread_join(thread1, NULL);
				pthread_join(thread2, NULL);
				PrintIndex();                     // 인덱스화면 출력
				printf("\n\n");
				printf(" >>> 프로그램 종료 준비 중입니다. 기다려주세요... \n");
				break;
		}

		if(cmd == 'q')
			break;
	}


	sem_destroy(&delay);
	system("clear");
	printf(" >>> 프로그램 정상 종료 완료... \n");

	return 0;
}





// 큐 초기화 함수
void initialize_queue(head_pointer quit_queue)   {
	int i = 0;
	if(quit_queue == NULL)   {
		// 모든 큐 초기화
		for(i=0; i<CLIENT_MAX_NUM; i++)
		{
			queue[i] = (head_pointer)malloc(sizeof(struct queue_head));
			queue[i]->right_link = NULL;
			queue[i]->start_time = 0;         // 시작 시간을 저장하는 변수 초기화
			queue[i]->now_time = 0;         // 현재 시간을 저장하는 변수 초기화
			queue[i]->now_status = 0;       // 현재 상태를 저장하는 변수 초기화
			queue[i]->now_pulse = 0;       // 현재 맥박값을 저장하는 변수 초기화
			queue[i]->sum_pulse = 0;       // 맥박값 저장하는 변수 초기화
			queue[i]->cnt_pulse = 0;        // 맥박값 카운트 변수 초기화
			queue[i]->ave_pulse = 0.0;      // 평균 맥박값 저장하는 변수 초기화
			queue[i]->min_pulse = 0;        // 최소 맥박값 저장하는 변수 초기화
			queue[i]->max_pulse = 0;       // 최대 맥박값 저장하는 변수 초기화
			memset(queue[i]->ip, 0, sizeof(queue[i]->ip));
					            // ip주소 저장하는 char 배열 초기화
			memset(queue[i]->level, 0 , sizeof(queue[i]->level)); 		
						  // 계급의 도수를 저장하는 int 배열 초기화
		}
	}
	else if(quit_queue->now_status == STATUS_EXIT)   {
		// 종료된 클라이언트가 들어가있는 큐만 초기화
		free(quit_queue);                  // 메모리 초기화

		// 재생성 ---------------------------------------------------
		quit_queue = (head_pointer)malloc(sizeof(struct queue_head));
		quit_queue->right_link = NULL;
		quit_queue->start_time = 0;          // 시작 시간을 저장하는 변수 초기화
		quit_queue->now_time = 0;          // 현재 시간을 저장하는 변수 초기화
		quit_queue->now_status = 0;        // 현재 상태를 저장하는 변수 초기화
		quit_queue->now_pulse = 0;        // 현재 맥박값을 저장하는 변수 초기화
		quit_queue->sum_pulse = 0;       // 맥박값 저장하는 변수 초기화
		quit_queue->cnt_pulse = 0;         // 맥박값 카운트 변수 초기화
		quit_queue->ave_pulse = 0.0;       // 평균 맥박값 저장하는 변수 초기화
		quit_queue->min_pulse = 0;        // 최소 맥박값 저장하는 변수 초기화
		quit_queue->max_pulse = 0;       // 최대 맥박값 저장하는 변수 초기화
		memset(quit_queue->ip, 0, sizeof(quit_queue->ip)); 
					     // ip주소 저장하는 char 배열 초기화
		memset(quit_queue->level, 0 , sizeof(quit_queue->level));   
					    // 계급의 도수를 저장하는 int 배열 초기화
	}
	else if( quit_queue->now_status == STATUS_TIME_OVER )   {
		// IP주소를 제외한 나머지 변수들 다 초기화
		quit_queue->right_link = NULL;
		quit_queue->start_time = 0;       // 시작 시간을 저장하는 변수 초기화
		quit_queue->now_time = 0;       // 현재 시간을 저장하는 변수 초기화
		quit_queue->now_status = 0;     // 현재 상태를 저장하는 변수 초기화
		quit_queue->now_pulse = 0;     // 현재 맥박값을 저장하는 변수 초기화
		quit_queue->sum_pulse = 0;     // 맥박값 저장하는 변수 초기화
		quit_queue->cnt_pulse = 0;      // 맥박값 카운트 변수 초기화
		quit_queue->ave_pulse = 0.0;    // 평균 맥박값 저장하는 변수 초기화
		quit_queue->min_pulse = 0;     // 최소 맥박값 저장하는 변수 초기화
		quit_queue->max_pulse = 0;     // 최대 맥박값 저장하는 변수 초기화
		memset(quit_queue->level, 0 , sizeof(quit_queue->level));   
						// 계급의 도수를 저장하는 int 배열 초기화
	// --------------------------------------------------------------------------------------------------------
	}   
}
// 큐 삽입 함수
int insert_queue(char *ip, int pulse)   {
	queue_pointer new_node = (queue_pointer)malloc(sizeof(struct double_list_queue));
							   // 새노드
	if(new_node == NULL)
		PrintError("메모리 할당 에러");
	// ---------------------------- ↓큐 헤드 작업 ---------------------------------
	ID = SearchID(ip);               // 들어갈 큐 찾기


	if(ID == -1)   {
		// 모든 큐가 꽉차서 할당 못받았을경우
		buf[BUF_STATUS] = STATUS_FULL_QUEUE;
		sendto(serv_sock, (char *)buf, sizeof(buf), 0, (struct sockaddr*)&clnt_adr, sizeof(clnt_adr));
		return 0;
	}


	if( queue[ID]->right_link != NULL && TIMER <= (queue[ID]->now_time-queue[ID]->start_time) )   {
		// 설정한 타이머를 초과했을경우
		queue[ID]->now_status = STATUS_TIME_OVER;
		initialize_queue(queue[ID]);  
	          // 타이머 초과한 큐 IP를 제외한 나머지 다초기화

		fseek(fp, 0, SEEK_END);   // 파일 맨끝으로 이동
		fprintf(fp, "\n");
	}

	queue[ID]->now_time = buf[BUF_TIME];     // 해당큐의 현재 시간에 타임값 저장
	queue[ID]->now_pulse = pulse;            // 해당큐의 현재맥박값 변수에 들어온 맥박값저장
	queue[ID]->cnt_pulse++;                  // 해당큐의 맥박 카운트변수 1 증가
	queue[ID]->sum_pulse += pulse;          // 해당큐의 맥박값 총합에 맥박값 저장
	queue[ID]->ave_pulse = (double)queue[ID]->sum_pulse/queue[ID]->cnt_pulse;    
					  // 해당큐의 평균값 저장
	PulseMaxMin( queue[ID] );          
			 // 해당큐의 맥박값의 최댓값 최솟값 구해서 queue[ID]의 변수에 저장

	fseek(fp, 0, SEEK_END);  		 // 파일 맨끝으로 이동
	fprintf(fp, "%d \t %d \t\t %d \t\t %d \t\t %.2lf \n",queue[ID]->cnt_pulse, queue[ID]->now_pulse, queue[ID]->max_pulse, queue[ID]->min_pulse, queue[ID]->ave_pulse);
	fclose(fp);           		 // 파일 입력종료
	// ---------------------------- ↓새 노드 작업 ---------------------------------
	new_node->right_link = NULL;
	new_node->pulse = pulse;
	if( pulse < PULSE_MIN_LIMIT )   { 
              // 맥박값 < PUlSE_MIN_LIMIT 이면
		queue[ID]->now_status = STATUS_WARNING;    // 큐의 현재상태 WARNING
		new_node->status = STATUS_WARNING;        // 상태 WARNING( = 0 )
		buf[BUF_STATUS] = STATUS_WARNING;         // 상태 WARNING( = 0 )
		buf[BUF_PULSE] = queue[ID]->now_pulse;     // 버퍼 맥박에 들어온 맥박값 넣어줌
		sendto(serv_sock, (char *)buf, sizeof(buf), 0, (struct sockaddr*)&clnt_adr, sizeof(clnt_adr));
	}
	else if(pulse < PULSE_MAX_LIMIT)   {
               // 맥박값 정상수치이면
		queue[ID]->now_status = STATUS_RUN;      // 큐의 현재상태 RUN
		new_node->status = STATUS_RUN;           // 상태 RUN ( = 1 )
		buf[BUF_STATUS] = STATUS_RUN;            // 상태 WARNING( = 0 )
		buf[BUF_PULSE] = queue[ID]->now_pulse;    // 버퍼 맥박에 들어온 맥박값 넣어줌
		sendto(serv_sock, (char *)buf, sizeof(buf), 0, (struct sockaddr*)&clnt_adr, sizeof(clnt_adr));
	}


	else   { 
             // 맥박값 > PULSE_MAX_LIMIT이면                           
		queue[ID]->now_status = STATUS_WARNING;     // 큐의 현재상태 WARNING
		new_node->status = STATUS_WARNING;         // 상태 WARNING( = 0 )
		buf[BUF_STATUS] = STATUS_WARNING;          // 상태 WARNING( = 0 )
		buf[BUF_PULSE] = queue[ID]->now_pulse;     // 버퍼 맥박에 들어온 맥박값 넣어줌
		sendto(serv_sock, (char *)buf, sizeof(buf), 0, (struct sockaddr*)&clnt_adr, sizeof(clnt_adr));
	}   
	// -----------------------------------------------------------------------------
	if( queue[ID]->right_link == NULL )   {
		// 첫 노드일때의 큐 삽입
		queue[ID]->right_link = new_node;
		queue[ID]->start_time = queue[ID]->now_time; 
			        // 첫노드일경우 start_time과 now_time 동기화
	}
	else   {   
	// 첫노드가 아닐때의 큐 삽입
		// --------------------------- ↓맨 앞 노드 삭제 -------------------------------
		if( queue[ID]->cnt_pulse % NODE_MAX_NUM == 1 )   {
			// 맥박카운트를 NODE_MAX_NUM으로 나누었을때 1이면, 예를들어 NODE_MAX_NUM가 100일때 101번째 노드이면 맨 앞 노드 삭제
			// (첫노드는 신경쓸필요 없음. 이 부분엔 접근하지 않으므로.)
			queue_pointer   delete_temp;         // 맨 앞 노드 삭제용 delete_temp
			delete_temp = queue[ID]->right_link;

			queue[ID]->right_link = delete_temp->right_link;
			queue[ID]->right_link->left_link = NULL;
			free(delete_temp);
		}
		// --------------------------- ↓맨 끝 노드에 추가 -----------------------------
		queue_pointer move_temp;               // 맨 끝 노드로 이동용 move_temp
		move_temp = queue[ID]->right_link;

		while( move_temp->right_link != NULL )   {
			//맨 끝노드로 이동
			move_temp = move_temp->right_link;
		}
		move_temp->right_link = new_node;         // 새노드 추가
		new_node->left_link = move_temp;         // 새노드랑 마지막 노드 연결
		//------------------------------------------------------------------------------
	}
	sem_wait(&delay);
	// --------------------- ↓큐에 삽입 완료후 여러가지 작업 ----------------------
	PulseGraph( queue[ID] );            // 해당큐의 맥박들의 분포를 확인 할수 있는 표 생성
	PrintIndex();                         // 인덱스화면 출력
	PrintInfo( queue[ID] );               // 맥박값에 대한 정보를 출력
	AllClientInfo();                      // 모든 클라이언트 정보 출력
	if(type == 1)                     // 클라이언트 자세한정보 보고싶다는 요청을 받았을때
		DetailClientInfo();               // 함수호출
	PrintMenu();                     // 메뉴 출력

	//------------------------------------------------------------------------------
	sem_post(&delay);
	return 0;
}
// 인덱스화면 출력 함수
void PrintIndex(void)   {
	system("clear");
	printf("┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓\n");
	printf("┃                                                                          ┃\n");
	printf("┃                                                                          ┃\n");
	printf("┃                 PULSE_CHECK                                              ┃\n");
	printf("┃                                        System  (Server)                  ┃\n");
	printf("┃                                                                          ┃\n");
	printf("┃                                                                          ┃\n");
	printf("┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛\n\n");
}
// 들어갈 큐찾는 함수
int SearchID(char *ip)   {
	int i = 0;
	int id = -1 ;

	char file_name[50];

	for(i=0; i<CLIENT_MAX_NUM; i++)   {
		if( !strcmp(queue[i]->ip,ip) )   {
			// 동일한 아이피가 있는지 검색
			sprintf(file_name, "%d CLIENT %s.txt", i+1, ip);

			fp = fopen(file_name, "r+");
			id = i;      // id에 i값 저장
			break;
		}
	}

	if( id == -1 )   {
		// 검색된 동일한 아이피주소가 없을때 새로 추가
		for(i=0; i<CLIENT_MAX_NUM; i++)   {
			if( *queue[i]->ip == 0  )   {
				strncpy(queue[i]->ip, ip,strlen(ip));      // queue->ip에 복사

				sprintf(file_name, "%d CLIENT %s.txt", i+1, ip);

				fp = fopen(file_name, "w+");
				fprintf(fp, "Client IP = %s \n\n",ip);
				fprintf(fp, "Count \t Pulse \t Max_Pulse \t Min_Pulse \t Average_Pulse \n");
				id = i;
				break;
			}
		}
	}
	return id;
}
// 에러메세지 출력 함수
void PrintError(char *msg)   {
	printf("> [ERROR] %s \n",msg);
	exit(0);
}
// 맥박의 최대, 최소값 계산후 저장
void PulseMaxMin( head_pointer queue  )   {
	if( queue->cnt_pulse == 1 )   {
		// 노드가 한개밖에없을때
		queue->max_pulse = queue->now_pulse;      // 그 큐의 최댓값에 맥박값 저장
		queue->min_pulse = queue->now_pulse;      // 그 큐의 최솟값에 맥박값 저장
	}

	else   {
		// 노드가 두 개 이상일때
		if(queue->max_pulse < queue->now_pulse )
		      // 그 큐의 최댓값이 방금 들어온 맥박값보다 작으면 변경 
			queue->max_pulse = queue->now_pulse;
		else
			queue->max_pulse;

		if(queue->min_pulse > queue->now_pulse )
		      // 그 큐의 최솟값이 방금 들어온 맥박값보다 크면 변경
			queue->min_pulse = queue->now_pulse;
		else
			queue->min_pulse;
	}
}

// 해당 큐의 맥박들의 분포를 확인 할수 있는 표 생성
void PulseGraph(head_pointer queue)
{
	queue_pointer last_node;

	int temp;

	last_node = queue->right_link;

	temp = queue->now_pulse / 20;   // 맥박의 계급 계산
	queue->level[temp]++;             // 해당 계급의 도수를 1씩 증가

}

// 해당 큐의 정보 & 모든 큐의 정보 & 메뉴를 보는함수
void PrintInfo(head_pointer enter_queue)   {
	if(enter_queue->now_status == STATUS_EXIT)   {
		// 종료되서 호출되었을때
		printf("\t   ┌─────────────────────────────────────────────────────┐\n");
		printf("\t   │\t\tLately\t\t\t\t\t │\n");
		printf("\t   │\t\t\tClient [ %s ]\t │\n", enter_queue->ip);
		printf("\t   │\t\t\t\t\t\t\t │\n");
		printf("\t   │\t\t\t\t\t\t\t │\n");
		printf("\t   │\t\t     접   속   종   료\t\t\t │\n");
		printf("\t   │\t\t\t\t\t\t\t │\n");
		printf("\t   │\t\t\t\t\t\t\t │\n");
		printf("\t   └─────────────────────────────────────────────────────┘\n");
	}
	else   {
		// 종료되서 호출된게 아닐때
		printf("\t   ┌─────────────────────────────────────────────────────┐\n");
		printf("\t   │\t\tLately\t\t\t\t\t │\n");
		printf("\t   │\t\t\tClient [ %s ]\t │\n", enter_queue->ip);
		printf("\t   │\t\t\t\t\t\t\t │\n");
		printf("\t   │\t\t 맥박값 : %d \t\t\t\t │\n", enter_queue->now_pulse);
		printf("\t   │\t\t 최댓값 : %d \t\t\t\t │\n", enter_queue->max_pulse);
		printf("\t   │\t\t 최솟값 : %d \t\t\t\t │\n", enter_queue->min_pulse);
		printf("\t   │\t\t 평균값 : %.2lf \t\t\t │\n", enter_queue->ave_pulse);
		printf("\t   └─────────────────────────────────────────────────────┘\n");

	}

}
// 전체 정보 출력 함수
int AllClientInfo(void)   {
	int i = 0;
	printf("\n\n");
	printf(" ─────────────────────────[ 모든 접속 클라이언트정보 ]───────────────────────── \n");
	printf(" NUM \t IP \t\t\t LATELY_PULSE \t AVERAGE_PULSE \tSTATUS \n");
	for(i=0; i<CLIENT_MAX_NUM; i++)   {
		if( queue[i]->right_link != NULL )   {
		// 첫노드가 연결되었을경우,즉 클라이언트로부터 하나라도 맥박값을 받았을경우
			switch(queue[i]->now_status)   {
				case STATUS_RUN:
					if(queue[i]->ave_pulse >= 100)
						printf(" %d \t %s\t %d \t\t %.2lf \t[ G O O D ] \n", i+1, queue[i]->ip, queue[i]->now_pulse, queue[i]->ave_pulse);
					else
						printf(" %d \t %s\t %d \t\t %.2lf \t\t[ G O O D ] \n", i+1, queue[i]->ip, queue[i]->now_pulse, queue[i]->ave_pulse);
					break;
				case STATUS_WARNING:
					if(queue[i]->ave_pulse >= 100)
						printf(" %d \t %s\t %d \t\t %.2lf \t[ W A R N I N G ] \n", i+1, queue[i]->ip, queue[i]->now_pulse, queue[i]->ave_pulse);
					else
						printf(" %d \t %s\t %d \t\t %.2lf \t\t[ W A R N I N G ] \n", i+1, queue[i]->ip, queue[i]->now_pulse, queue[i]->ave_pulse);
					break;
				case STATUS_EXIT:
					if(queue[i]->ave_pulse >= 100)
						printf(" %d \t %s\t %d \t\t %.2lf \t[ E X I T ] \n", i+1, queue[i]->ip, queue[i]->now_pulse, queue[i]->ave_pulse);
					else
						printf(" %d \t %s\t %d \t\t %.2lf \t\t[ E X I T ] \n", i+1, queue[i]->ip, queue[i]->now_pulse, queue[i]->ave_pulse);
					break;
			}
		}
		else
			printf(" %d \t\t\t    [ NOT CONNECTED ] \n", i+1);
	}
	printf(" ────────────────────────────────────────────────────────────────────────────── \n\n");

	return 0;
}
// 메뉴 함수
void PrintMenu(void)   {

	printf("\n");
	printf("[ 메 뉴 ] ───────────────────────────────────────────────────────────────── \n");
	printf("  1) 클라이언트 정보 자세히 보기 \n");
	printf("  2) 클라이언트 정보 자세히 보기 닫기 \n");
	printf("  q) 종료하기 \n");
	//printf("> 선택하세요 :  ");

}
// 선택한 클라이언트 정보 자세히보는 함수
void DetailClientInfo()   {
	int i = 0, j = 0;
	int cnt=0;

	if(queue[info_id]->now_status == STATUS_EXIT)   {
		printf("\t   ┌─────────────────────────────────────────────────────┐\n");
		printf("\t   │\t\tSelected\t\t\t\t │\n");
		printf("\t   │\t\t\tClient [ %s ]\t │\n", queue[info_id]->ip);
		printf("\t   │\t\t\t\t\t\t\t │\n");
		printf("\t   │\t\t\t\t\t\t\t │\n");
		printf("\t   │\t\t     접   속   종   료\t\t\t │\n");
		printf("\t   │\t\t\t\t\t\t\t │\n");
		printf("\t   │\t\t\t\t\t\t\t │\n");
		printf("\t   └─────────────────────────────────────────────────────┘\n");
		type=0;
	}
	else   {
		printf("\t   ┌─────────────────────────────────────────────────────┐\n");
		printf("\t   │\t\tSelected\t\t\t\t │\n");
		printf("\t   │\t\t\tClient [ %s ]\t │\n", queue[info_id]->ip);
		printf("\t   │\t\t\t\t\t\t\t │\n");
		printf("\t   │\t\t 맥박값 : %d \t\t\t\t │\n", queue[info_id]->now_pulse);
		printf("\t   │\t\t 최댓값 : %d \t\t\t\t │\n", queue[info_id]->max_pulse);
		printf("\t   │\t\t 최솟값 : %d \t\t\t\t │\n",   queue[info_id]->min_pulse);
		printf("\t   │\t\t 평균값 : %.2lf \t\t\t │\n", queue[info_id]->ave_pulse);
		printf("\t   └─────────────────────────────────────────────────────┘\n");

		printf("\n\t\t───────────────[ 도수 분포표 ]─────────────── \n");
		for(i=0; i < 10; i++)   {
			// 도수분포표 출력
			if(cnt >= 100)   
				printf("\t\t\t%d ~ %d｜", cnt, cnt+19);
			else
				printf("\t\t\t%d ~ %d\t ｜", cnt, cnt+19);

			for(j=0; j<queue[info_id]->level[i]; j++){
				printf("■");
			}
			cnt += 20;
			printf("\n");

		}
		printf("\t\t──────────────────────────────────────────────\n");
	}
}

// 쓰레드 함수1
//unsigned WINAPI ThreadFunc1(void *arg)   {
void * ThreadFunc1(void *arg) {
	int i = 0;
	int cSize;
	cSize = sizeof(clnt_adr);

	while(cmd != 'q' )   {
		recvfrom(serv_sock, (char *)buf, sizeof(buf), 0, (struct sockaddr*)&clnt_adr, &cSize);
		if(buf[BUF_STATUS] == STATUS_EXIT)   {
			// 클라이언트로부터 받은 BUF_STATUS가 2, 즉 종료처리이면
			ID= SearchID( inet_ntoa(clnt_adr.sin_addr) );
			queue[ID]->now_status = STATUS_EXIT;

			sem_wait(&delay);
			//-----------------------------------------------------------------------------
			PrintIndex();                     // 인덱스화면 출력
			PrintInfo( queue[ID] );               // 큐 정보 출력
			AllClientInfo();                  // 모든 클라이언트 정보 출력
			if(type == 1)                     // 클라이언트 자세한정보 보고싶다는 요청을 받았을때
				DetailClientInfo();               // 함수호출
			PrintMenu();                     // 메뉴 출력
			//-----------------------------------------------------------------------------
			sem_post(&delay);

			initialize_queue( queue[ID] );         // 큐 초기화 진행

			int cnt=0;
			for(i=0; i<CLIENT_MAX_NUM; i++)   {
				if(queue[i]->right_link == NULL)
					cnt++;
			}
			if(cnt == CLIENT_MAX_NUM)   {
				sleep(2);
				system("clear");
				printf(" > 모든 클라이언트가 접속 종료되었습니다... \n");
				printf(" > 클라이언트 접속을 기다리는중입니다... \n");
			}

		}
		else
			// 클라이언트로부터 받은 BUF_STATUS가 2가아닌경우, 런 상태이면
			insert_queue( inet_ntoa(clnt_adr.sin_addr), buf[BUF_PULSE] );
	}

	while(cmd == 'q')   {
		// 종료메뉴를 선택받았을시
		int cnt=0;
		
		recvfrom(serv_sock, (char *)buf, sizeof(buf), 0, (struct sockaddr*)&clnt_adr, &cSize );

		ID= SearchID( inet_ntoa(clnt_adr.sin_addr) );

		buf[BUF_STATUS] = STATUS_EXIT;                     // 상태 EXIT( = 2 )
		queue[ID]->now_status = STATUS_EXIT;

		initialize_queue( queue[ID] );                     // 큐 초기화 진행

		sendto(serv_sock, (char *)&buf[BUF_STATUS], sizeof(buf[BUF_STATUS]), 0, (struct sockaddr*)&clnt_adr, sizeof(clnt_adr));

		for(i=0; i<CLIENT_MAX_NUM; i++)   {
			if(queue[i]->right_link == NULL)
				cnt++;
		}
		if(cnt == CLIENT_MAX_NUM)   
			break;      

	} 
	return 0;
}

// 쓰레드 함수1
//unsigned WINAPI ThreadFunc2(void *arg)   {
void * ThreadFunc2(void *arg) {
	int i = 0;

	while(cmd != 'q' )   {
		while(cmd != 'q' )   {
			int cnt=0;
			for(i=0; i<CLIENT_MAX_NUM; i++)   {
				if(queue[i]->right_link == NULL)
					cnt++;
			}
			if(cnt == CLIENT_MAX_NUM)   {				
				break;
			}

			sem_wait(&delay);
			//-----------------------------------------------------------------------------
			PrintIndex();                     // 인덱스화면 출력
			PrintInfo( queue[ID] );               // 큐 정보 출력
			AllClientInfo();                  // 모든 클라이언트 정보 출력
			if(type == 1)       // 클라이언트 자세한정보 보고싶다는 요청을 받았을때
				DetailClientInfo();        // 함수호출
			PrintMenu();                     // 메뉴 출력
//-----------------------------------------------------------------------------
			sem_post(&delay);
			sleep(3);   // 3초마다 화면출력
		}
	}

	return 0;
