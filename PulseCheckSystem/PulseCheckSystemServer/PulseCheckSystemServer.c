#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>      // Ÿ�̸� ����� ���� ���
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <semaphore.h>

// ����
#define SETTING_FILE_ADDRESS   "SERVER_SETTING.txt"
//---------------------- ������ ���� -----------------------
#define STATUS_WARNING		0            // ������´� 0
#define STATUS_RUN            	1            // �������� ���´� 1
#define STATUS_EXIT            	2            // ����� ���´� 2
#define STATUS_FULL_QUEUE     	3            // ť�� ��á����� 3
#define STATUS_TIME_OVER      	4            // Ÿ�ӿ����ϰ�� 4
//---------------------- ����� ���� -----------------------
#define BUF_MAX_SIZE         	3            // ������ �ְ���� buf ũ�� 2
#define   BUF_STATUS            	0            // buf���� ���°��� ���ִ� ��ġ 0
#define BUF_PULSE          	1            // buf���� �ƹڰ��� ���ִ� ��ġ 1
#define BUF_TIME         		2            // buf���� �ð����� ���ִ� ��ġ 2
//-------------------- ��buf ���� ���� ---------------------
#define CLIENT_MAX_NUM        	10           // Ŭ���̾�Ʈ �ִ밹�� 10
#define NODE_MAX_NUM        	100          // ��� �ִ밹�� 100
//---------------------- �谢�� ���� -----------------------


// ����ü ����
typedef struct double_list_queue *queue_pointer;
typedef struct queue_head *head_pointer;

//����ü ����
struct double_list_queue   {
	int            pulse;
	int            status;
	queue_pointer   left_link, right_link;
};
struct queue_head   {
	queue_pointer   right_link;
	int            start_time;                 // ���� �ð��� ����
	int            now_time;                 // ���� �ð��� ����
	int            now_status;               // ���� ���¸� ����
	int            now_pulse;                // ���� �ƹڰ� ����
	int            sum_pulse;                // �ƹڰ� ������ �����ϱ����� ����
	int            cnt_pulse;                // �ƹ� ī��Ʈ ����
	double         ave_pulse;              // ��� �ƹڰ� �����ϴ� ����
	int            min_pulse;              // ����� �ƹڰ��� ���� ���� �ƹڰ� �����ϴ� ����
	int            max_pulse;             // ����� �ƹڰ��� ���� ū �ƹڰ� �����ϴ� ����
	char           ip[20];                  // ������ Ŭ���̾�Ʈ ip �ּ� ������ ���� ����
	int            level[10];             // ��������ǥ�� ����� ������ count�ϱ� ���� �迭
};

// �Լ� ����
void initialize_queue( head_pointer quit_queue )   // ��� ť �ʱ�ȭ or ����� Ŭ���̾�Ʈ�� ť �ʱ�ȭ
int insert_queue( char *ip, int pulse );            // ť ����
void PrintError( char *msg );                     // �����޼��� ���
void PrintIndex( void );                         // �ε���ȭ�� ���
int SearchID( char *ip );                        // �� ť ��ġã��
void PulseMaxMin( head_pointer temp );         // �ƹ��� �ִ�, �ּҰ� ���ؼ� ť�� ����
void PulseGraph( head_pointer queue );         // �ƹ��� ��������ǥ ���ϱ�
void PrintInfo( head_pointer queue );           // �ƹڿ� ���� ���� ���
void * ThreadFunc1(void *arg);
void * ThreadFunc2(void *arg);

int AllClientInfo ( void );                           // ���ӵ� ��� Ŭ���̾�Ʈ�� ����
void PrintMenu(void);                              // �޴� �Լ�
void DetailClientInfo();

// �������� ����
char      SERV_IP[20];                      // SERVER_SETTING.txt�κ��� �޾ƿ��� ���� ������
int         SERV_PORT;                     // SERVER_SETTING.txt�κ��� �޾ƿ��� ���� ��Ʈ
int         PULSE_MIN_LIMIT;               // �ƹ� �ּ�ġ ����
int         PULSE_MAX_LIMIT;               // �ƹ� �ִ�ġ ����
int         TIMER;                          // Ÿ�̸� (��) ����
//----------------------------------------------------------------------------------------------------------
int serv_sock;
struct sockaddr_in serv_adr, clnt_adr;
//----------------------------------------------------------------------------------------------------------
int         buf[BUF_MAX_SIZE];             // Ŭ���̾�Ʈ�� �ְ�ޱ����� buf�迭
char      cmd;                            // �޴����� ����
int         type;                           // Ÿ�� ����
int         quit;                            // ����Ʈ ������ ���Ḧ ���� ����
int         ID;                             // ���° queue���� Ȯ�������� ID����
int         info_id;                         // �������� ���� �ʿ��� ����
FILE      *fp;                              // ���� ������
static sem_t delay;
//----------------------------------------------------------------------------------------------------------
head_pointer   queue[CLIENT_MAX_NUM];

int main()
{
	int i = 0;
	pthread_t thread1, thread2;

	// ���� ���� - UDP
	serv_sock = socket(PF_INET, SOCK_DGRAM, 0);
	if(serv_sock == -1)
		PrintError("UDP ���� ���� ����");

	chdir("SETTING");
	fp = fopen(SETTING_FILE_ADDRESS, "r");
	fscanf(fp, "IP = %s \n",SERV_IP);
	fscanf(fp, "PORT = %d \n",&SERV_PORT);
	fscanf(fp, "MIN_PULSE = %d \n", &PULSE_MIN_LIMIT);
	fscanf(fp, "MAX_PULSE = %d \n", &PULSE_MAX_LIMIT);
	fscanf(fp, "TIMER = %d", &TIMER);
	TIMER = TIMER * 1;      // TIMER�� �д����� �ٲ���.
	if(fp==NULL)   {
		printf("���� ���� ���� \n");
		exit(1);
	}   
	chdir("..");	chdir("DATA");


	// ���� �ּҼ���
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = inet_addr(SERV_IP);
	serv_adr.sin_port = htons(SERV_PORT);


	//sAddr.sin_family      =   AF_INET;
	//sAddr.sin_addr.s_addr   =   inet_addr(SERV_IP);
	//sAddr.sin_port         =   htons(SERV_PORT);


	if(bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
		PrintError("bind() error");
	// -------------------------------- ����� ���� UDP ���� ���� �Ϸ� --------------------------------
	initialize_queue(NULL);   // ����Ʈ �ʱ�ȭ

	printf(" > Ŭ���̾�Ʈ ������ ��ٸ������Դϴ�... \n");

	sem_init(&delay, 1, 1);

	pthread_create(&thread1, NULL, ThreadFunc1, NULL);
	pthread_create(&thread2, NULL, ThreadFunc2, NULL);

	while(1)   {
		scanf("%c",&cmd);               // �޴� ����
		fflush(stdin);
		int cnt = 0;
		//--------- ��� Ŭ���̾�Ʈ ��������Ǿ����� �޴����� X-------
		for(i=0; i<CLIENT_MAX_NUM; i++)   {
			if(queue[i]->right_link == NULL)
				cnt++;
		}
		if(cnt == CLIENT_MAX_NUM)   {
			system("clear");
			printf(" > ��� Ŭ���̾�Ʈ�� ���� ����Ǿ����ϴ�... \n");
			printf(" > Ŭ���̾�Ʈ ������ ��ٸ������Դϴ�... \n");
			continue;
		}
		//--------------------------------------------------------------
		switch(cmd)   {
			case '1':
				// Ŭ���̾�Ʈ �������� ����
				sem_wait(&delay);
				type = 1;
				printf(">> Ŭ���̾�Ʈ�� �������ּ���(1 ~ %d) : ", CLIENT_MAX_NUM);
				scanf("%d",&info_id);
				while( info_id > CLIENT_MAX_NUM || info_id < 1 || queue[info_id-1]->right_link == NULL )   {
					printf("[ ERROR ] ��ȿ���� ���� Ŭ���̾�Ʈ �ѹ��Դϴ�. �ٽ��Է����ּ���. \n");
					printf(">> Ŭ���̾�Ʈ�� �������ּ���(1 ~ %d) : ", CLIENT_MAX_NUM);
					scanf("%d",&info_id);
				}
				info_id--;
				sem_post(&delay);
				break;

			case '2':
				// Ŭ���̾�Ʈ �������� �ݱ�
				type = 0;
				break;
			case 'q':
				// ���α׷� �����ϱ�
				pthread_join(thread1, NULL);
				pthread_join(thread2, NULL);
				PrintIndex();                     // �ε���ȭ�� ���
				printf("\n\n");
				printf(" >>> ���α׷� ���� �غ� ���Դϴ�. ��ٷ��ּ���... \n");
				break;
		}

		if(cmd == 'q')
			break;
	}


	sem_destroy(&delay);
	system("clear");
	printf(" >>> ���α׷� ���� ���� �Ϸ�... \n");

	return 0;
}





// ť �ʱ�ȭ �Լ�
void initialize_queue(head_pointer quit_queue)   {
	int i = 0;
	if(quit_queue == NULL)   {
		// ��� ť �ʱ�ȭ
		for(i=0; i<CLIENT_MAX_NUM; i++)
		{
			queue[i] = (head_pointer)malloc(sizeof(struct queue_head));
			queue[i]->right_link = NULL;
			queue[i]->start_time = 0;         // ���� �ð��� �����ϴ� ���� �ʱ�ȭ
			queue[i]->now_time = 0;         // ���� �ð��� �����ϴ� ���� �ʱ�ȭ
			queue[i]->now_status = 0;       // ���� ���¸� �����ϴ� ���� �ʱ�ȭ
			queue[i]->now_pulse = 0;       // ���� �ƹڰ��� �����ϴ� ���� �ʱ�ȭ
			queue[i]->sum_pulse = 0;       // �ƹڰ� �����ϴ� ���� �ʱ�ȭ
			queue[i]->cnt_pulse = 0;        // �ƹڰ� ī��Ʈ ���� �ʱ�ȭ
			queue[i]->ave_pulse = 0.0;      // ��� �ƹڰ� �����ϴ� ���� �ʱ�ȭ
			queue[i]->min_pulse = 0;        // �ּ� �ƹڰ� �����ϴ� ���� �ʱ�ȭ
			queue[i]->max_pulse = 0;       // �ִ� �ƹڰ� �����ϴ� ���� �ʱ�ȭ
			memset(queue[i]->ip, 0, sizeof(queue[i]->ip));
					            // ip�ּ� �����ϴ� char �迭 �ʱ�ȭ
			memset(queue[i]->level, 0 , sizeof(queue[i]->level)); 		
						  // ����� ������ �����ϴ� int �迭 �ʱ�ȭ
		}
	}
	else if(quit_queue->now_status == STATUS_EXIT)   {
		// ����� Ŭ���̾�Ʈ�� ���ִ� ť�� �ʱ�ȭ
		free(quit_queue);                  // �޸� �ʱ�ȭ

		// ����� ---------------------------------------------------
		quit_queue = (head_pointer)malloc(sizeof(struct queue_head));
		quit_queue->right_link = NULL;
		quit_queue->start_time = 0;          // ���� �ð��� �����ϴ� ���� �ʱ�ȭ
		quit_queue->now_time = 0;          // ���� �ð��� �����ϴ� ���� �ʱ�ȭ
		quit_queue->now_status = 0;        // ���� ���¸� �����ϴ� ���� �ʱ�ȭ
		quit_queue->now_pulse = 0;        // ���� �ƹڰ��� �����ϴ� ���� �ʱ�ȭ
		quit_queue->sum_pulse = 0;       // �ƹڰ� �����ϴ� ���� �ʱ�ȭ
		quit_queue->cnt_pulse = 0;         // �ƹڰ� ī��Ʈ ���� �ʱ�ȭ
		quit_queue->ave_pulse = 0.0;       // ��� �ƹڰ� �����ϴ� ���� �ʱ�ȭ
		quit_queue->min_pulse = 0;        // �ּ� �ƹڰ� �����ϴ� ���� �ʱ�ȭ
		quit_queue->max_pulse = 0;       // �ִ� �ƹڰ� �����ϴ� ���� �ʱ�ȭ
		memset(quit_queue->ip, 0, sizeof(quit_queue->ip)); 
					     // ip�ּ� �����ϴ� char �迭 �ʱ�ȭ
		memset(quit_queue->level, 0 , sizeof(quit_queue->level));   
					    // ����� ������ �����ϴ� int �迭 �ʱ�ȭ
	}
	else if( quit_queue->now_status == STATUS_TIME_OVER )   {
		// IP�ּҸ� ������ ������ ������ �� �ʱ�ȭ
		quit_queue->right_link = NULL;
		quit_queue->start_time = 0;       // ���� �ð��� �����ϴ� ���� �ʱ�ȭ
		quit_queue->now_time = 0;       // ���� �ð��� �����ϴ� ���� �ʱ�ȭ
		quit_queue->now_status = 0;     // ���� ���¸� �����ϴ� ���� �ʱ�ȭ
		quit_queue->now_pulse = 0;     // ���� �ƹڰ��� �����ϴ� ���� �ʱ�ȭ
		quit_queue->sum_pulse = 0;     // �ƹڰ� �����ϴ� ���� �ʱ�ȭ
		quit_queue->cnt_pulse = 0;      // �ƹڰ� ī��Ʈ ���� �ʱ�ȭ
		quit_queue->ave_pulse = 0.0;    // ��� �ƹڰ� �����ϴ� ���� �ʱ�ȭ
		quit_queue->min_pulse = 0;     // �ּ� �ƹڰ� �����ϴ� ���� �ʱ�ȭ
		quit_queue->max_pulse = 0;     // �ִ� �ƹڰ� �����ϴ� ���� �ʱ�ȭ
		memset(quit_queue->level, 0 , sizeof(quit_queue->level));   
						// ����� ������ �����ϴ� int �迭 �ʱ�ȭ
	// --------------------------------------------------------------------------------------------------------
	}   
}
// ť ���� �Լ�
int insert_queue(char *ip, int pulse)   {
	queue_pointer new_node = (queue_pointer)malloc(sizeof(struct double_list_queue));
							   // �����
	if(new_node == NULL)
		PrintError("�޸� �Ҵ� ����");
	// ---------------------------- ��ť ��� �۾� ---------------------------------
	ID = SearchID(ip);               // �� ť ã��


	if(ID == -1)   {
		// ��� ť�� ������ �Ҵ� ���޾������
		buf[BUF_STATUS] = STATUS_FULL_QUEUE;
		sendto(serv_sock, (char *)buf, sizeof(buf), 0, (struct sockaddr*)&clnt_adr, sizeof(clnt_adr));
		return 0;
	}


	if( queue[ID]->right_link != NULL && TIMER <= (queue[ID]->now_time-queue[ID]->start_time) )   {
		// ������ Ÿ�̸Ӹ� �ʰ��������
		queue[ID]->now_status = STATUS_TIME_OVER;
		initialize_queue(queue[ID]);  
	          // Ÿ�̸� �ʰ��� ť IP�� ������ ������ ���ʱ�ȭ

		fseek(fp, 0, SEEK_END);   // ���� �ǳ����� �̵�
		fprintf(fp, "\n");
	}

	queue[ID]->now_time = buf[BUF_TIME];     // �ش�ť�� ���� �ð��� Ÿ�Ӱ� ����
	queue[ID]->now_pulse = pulse;            // �ش�ť�� ����ƹڰ� ������ ���� �ƹڰ�����
	queue[ID]->cnt_pulse++;                  // �ش�ť�� �ƹ� ī��Ʈ���� 1 ����
	queue[ID]->sum_pulse += pulse;          // �ش�ť�� �ƹڰ� ���տ� �ƹڰ� ����
	queue[ID]->ave_pulse = (double)queue[ID]->sum_pulse/queue[ID]->cnt_pulse;    
					  // �ش�ť�� ��հ� ����
	PulseMaxMin( queue[ID] );          
			 // �ش�ť�� �ƹڰ��� �ִ� �ּڰ� ���ؼ� queue[ID]�� ������ ����

	fseek(fp, 0, SEEK_END);  		 // ���� �ǳ����� �̵�
	fprintf(fp, "%d \t %d \t\t %d \t\t %d \t\t %.2lf \n",queue[ID]->cnt_pulse, queue[ID]->now_pulse, queue[ID]->max_pulse, queue[ID]->min_pulse, queue[ID]->ave_pulse);
	fclose(fp);           		 // ���� �Է�����
	// ---------------------------- ��� ��� �۾� ---------------------------------
	new_node->right_link = NULL;
	new_node->pulse = pulse;
	if( pulse < PULSE_MIN_LIMIT )   { 
              // �ƹڰ� < PUlSE_MIN_LIMIT �̸�
		queue[ID]->now_status = STATUS_WARNING;    // ť�� ������� WARNING
		new_node->status = STATUS_WARNING;        // ���� WARNING( = 0 )
		buf[BUF_STATUS] = STATUS_WARNING;         // ���� WARNING( = 0 )
		buf[BUF_PULSE] = queue[ID]->now_pulse;     // ���� �ƹڿ� ���� �ƹڰ� �־���
		sendto(serv_sock, (char *)buf, sizeof(buf), 0, (struct sockaddr*)&clnt_adr, sizeof(clnt_adr));
	}
	else if(pulse < PULSE_MAX_LIMIT)   {
               // �ƹڰ� �����ġ�̸�
		queue[ID]->now_status = STATUS_RUN;      // ť�� ������� RUN
		new_node->status = STATUS_RUN;           // ���� RUN ( = 1 )
		buf[BUF_STATUS] = STATUS_RUN;            // ���� WARNING( = 0 )
		buf[BUF_PULSE] = queue[ID]->now_pulse;    // ���� �ƹڿ� ���� �ƹڰ� �־���
		sendto(serv_sock, (char *)buf, sizeof(buf), 0, (struct sockaddr*)&clnt_adr, sizeof(clnt_adr));
	}


	else   { 
             // �ƹڰ� > PULSE_MAX_LIMIT�̸�                           
		queue[ID]->now_status = STATUS_WARNING;     // ť�� ������� WARNING
		new_node->status = STATUS_WARNING;         // ���� WARNING( = 0 )
		buf[BUF_STATUS] = STATUS_WARNING;          // ���� WARNING( = 0 )
		buf[BUF_PULSE] = queue[ID]->now_pulse;     // ���� �ƹڿ� ���� �ƹڰ� �־���
		sendto(serv_sock, (char *)buf, sizeof(buf), 0, (struct sockaddr*)&clnt_adr, sizeof(clnt_adr));
	}   
	// -----------------------------------------------------------------------------
	if( queue[ID]->right_link == NULL )   {
		// ù ����϶��� ť ����
		queue[ID]->right_link = new_node;
		queue[ID]->start_time = queue[ID]->now_time; 
			        // ù����ϰ�� start_time�� now_time ����ȭ
	}
	else   {   
	// ù��尡 �ƴҶ��� ť ����
		// --------------------------- ��� �� ��� ���� -------------------------------
		if( queue[ID]->cnt_pulse % NODE_MAX_NUM == 1 )   {
			// �ƹ�ī��Ʈ�� NODE_MAX_NUM���� ���������� 1�̸�, ������� NODE_MAX_NUM�� 100�϶� 101��° ����̸� �� �� ��� ����
			// (ù���� �Ű澵�ʿ� ����. �� �κп� �������� �����Ƿ�.)
			queue_pointer   delete_temp;         // �� �� ��� ������ delete_temp
			delete_temp = queue[ID]->right_link;

			queue[ID]->right_link = delete_temp->right_link;
			queue[ID]->right_link->left_link = NULL;
			free(delete_temp);
		}
		// --------------------------- ��� �� ��忡 �߰� -----------------------------
		queue_pointer move_temp;               // �� �� ���� �̵��� move_temp
		move_temp = queue[ID]->right_link;

		while( move_temp->right_link != NULL )   {
			//�� ������ �̵�
			move_temp = move_temp->right_link;
		}
		move_temp->right_link = new_node;         // ����� �߰�
		new_node->left_link = move_temp;         // ������ ������ ��� ����
		//------------------------------------------------------------------------------
	}
	sem_wait(&delay);
	// --------------------- ��ť�� ���� �Ϸ��� �������� �۾� ----------------------
	PulseGraph( queue[ID] );            // �ش�ť�� �ƹڵ��� ������ Ȯ�� �Ҽ� �ִ� ǥ ����
	PrintIndex();                         // �ε���ȭ�� ���
	PrintInfo( queue[ID] );               // �ƹڰ��� ���� ������ ���
	AllClientInfo();                      // ��� Ŭ���̾�Ʈ ���� ���
	if(type == 1)                     // Ŭ���̾�Ʈ �ڼ������� ����ʹٴ� ��û�� �޾�����
		DetailClientInfo();               // �Լ�ȣ��
	PrintMenu();                     // �޴� ���

	//------------------------------------------------------------------------------
	sem_post(&delay);
	return 0;
}
// �ε���ȭ�� ��� �Լ�
void PrintIndex(void)   {
	system("clear");
	printf("��������������������������������������������������������������������������������������������������������������������������������������������������������\n");
	printf("��                                                                          ��\n");
	printf("��                                                                          ��\n");
	printf("��                 PULSE_CHECK                                              ��\n");
	printf("��                                        System  (Server)                  ��\n");
	printf("��                                                                          ��\n");
	printf("��                                                                          ��\n");
	printf("��������������������������������������������������������������������������������������������������������������������������������������������������������\n\n");
}
// �� ťã�� �Լ�
int SearchID(char *ip)   {
	int i = 0;
	int id = -1 ;

	char file_name[50];

	for(i=0; i<CLIENT_MAX_NUM; i++)   {
		if( !strcmp(queue[i]->ip,ip) )   {
			// ������ �����ǰ� �ִ��� �˻�
			sprintf(file_name, "%d CLIENT %s.txt", i+1, ip);

			fp = fopen(file_name, "r+");
			id = i;      // id�� i�� ����
			break;
		}
	}

	if( id == -1 )   {
		// �˻��� ������ �������ּҰ� ������ ���� �߰�
		for(i=0; i<CLIENT_MAX_NUM; i++)   {
			if( *queue[i]->ip == 0  )   {
				strncpy(queue[i]->ip, ip,strlen(ip));      // queue->ip�� ����

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
// �����޼��� ��� �Լ�
void PrintError(char *msg)   {
	printf("> [ERROR] %s \n",msg);
	exit(0);
}
// �ƹ��� �ִ�, �ּҰ� ����� ����
void PulseMaxMin( head_pointer queue  )   {
	if( queue->cnt_pulse == 1 )   {
		// ��尡 �Ѱ��ۿ�������
		queue->max_pulse = queue->now_pulse;      // �� ť�� �ִ񰪿� �ƹڰ� ����
		queue->min_pulse = queue->now_pulse;      // �� ť�� �ּڰ��� �ƹڰ� ����
	}

	else   {
		// ��尡 �� �� �̻��϶�
		if(queue->max_pulse < queue->now_pulse )
		      // �� ť�� �ִ��� ��� ���� �ƹڰ����� ������ ���� 
			queue->max_pulse = queue->now_pulse;
		else
			queue->max_pulse;

		if(queue->min_pulse > queue->now_pulse )
		      // �� ť�� �ּڰ��� ��� ���� �ƹڰ����� ũ�� ����
			queue->min_pulse = queue->now_pulse;
		else
			queue->min_pulse;
	}
}

// �ش� ť�� �ƹڵ��� ������ Ȯ�� �Ҽ� �ִ� ǥ ����
void PulseGraph(head_pointer queue)
{
	queue_pointer last_node;

	int temp;

	last_node = queue->right_link;

	temp = queue->now_pulse / 20;   // �ƹ��� ��� ���
	queue->level[temp]++;             // �ش� ����� ������ 1�� ����

}

// �ش� ť�� ���� & ��� ť�� ���� & �޴��� �����Լ�
void PrintInfo(head_pointer enter_queue)   {
	if(enter_queue->now_status == STATUS_EXIT)   {
		// ����Ǽ� ȣ��Ǿ�����
		printf("\t   ��������������������������������������������������������������������������������������������������������������\n");
		printf("\t   ��\t\tLately\t\t\t\t\t ��\n");
		printf("\t   ��\t\t\tClient [ %s ]\t ��\n", enter_queue->ip);
		printf("\t   ��\t\t\t\t\t\t\t ��\n");
		printf("\t   ��\t\t\t\t\t\t\t ��\n");
		printf("\t   ��\t\t     ��   ��   ��   ��\t\t\t ��\n");
		printf("\t   ��\t\t\t\t\t\t\t ��\n");
		printf("\t   ��\t\t\t\t\t\t\t ��\n");
		printf("\t   ��������������������������������������������������������������������������������������������������������������\n");
	}
	else   {
		// ����Ǽ� ȣ��Ȱ� �ƴҶ�
		printf("\t   ��������������������������������������������������������������������������������������������������������������\n");
		printf("\t   ��\t\tLately\t\t\t\t\t ��\n");
		printf("\t   ��\t\t\tClient [ %s ]\t ��\n", enter_queue->ip);
		printf("\t   ��\t\t\t\t\t\t\t ��\n");
		printf("\t   ��\t\t �ƹڰ� : %d \t\t\t\t ��\n", enter_queue->now_pulse);
		printf("\t   ��\t\t �ִ� : %d \t\t\t\t ��\n", enter_queue->max_pulse);
		printf("\t   ��\t\t �ּڰ� : %d \t\t\t\t ��\n", enter_queue->min_pulse);
		printf("\t   ��\t\t ��հ� : %.2lf \t\t\t ��\n", enter_queue->ave_pulse);
		printf("\t   ��������������������������������������������������������������������������������������������������������������\n");

	}

}
// ��ü ���� ��� �Լ�
int AllClientInfo(void)   {
	int i = 0;
	printf("\n\n");
	printf(" ��������������������������������������������������[ ��� ���� Ŭ���̾�Ʈ���� ]�������������������������������������������������� \n");
	printf(" NUM \t IP \t\t\t LATELY_PULSE \t AVERAGE_PULSE \tSTATUS \n");
	for(i=0; i<CLIENT_MAX_NUM; i++)   {
		if( queue[i]->right_link != NULL )   {
		// ù��尡 ����Ǿ������,�� Ŭ���̾�Ʈ�κ��� �ϳ��� �ƹڰ��� �޾������
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
	printf(" ������������������������������������������������������������������������������������������������������������������������������������������������������������ \n\n");

	return 0;
}
// �޴� �Լ�
void PrintMenu(void)   {

	printf("\n");
	printf("[ �� �� ] ���������������������������������������������������������������������������������������������������������������������������������� \n");
	printf("  1) Ŭ���̾�Ʈ ���� �ڼ��� ���� \n");
	printf("  2) Ŭ���̾�Ʈ ���� �ڼ��� ���� �ݱ� \n");
	printf("  q) �����ϱ� \n");
	//printf("> �����ϼ��� :  ");

}
// ������ Ŭ���̾�Ʈ ���� �ڼ������� �Լ�
void DetailClientInfo()   {
	int i = 0, j = 0;
	int cnt=0;

	if(queue[info_id]->now_status == STATUS_EXIT)   {
		printf("\t   ��������������������������������������������������������������������������������������������������������������\n");
		printf("\t   ��\t\tSelected\t\t\t\t ��\n");
		printf("\t   ��\t\t\tClient [ %s ]\t ��\n", queue[info_id]->ip);
		printf("\t   ��\t\t\t\t\t\t\t ��\n");
		printf("\t   ��\t\t\t\t\t\t\t ��\n");
		printf("\t   ��\t\t     ��   ��   ��   ��\t\t\t ��\n");
		printf("\t   ��\t\t\t\t\t\t\t ��\n");
		printf("\t   ��\t\t\t\t\t\t\t ��\n");
		printf("\t   ��������������������������������������������������������������������������������������������������������������\n");
		type=0;
	}
	else   {
		printf("\t   ��������������������������������������������������������������������������������������������������������������\n");
		printf("\t   ��\t\tSelected\t\t\t\t ��\n");
		printf("\t   ��\t\t\tClient [ %s ]\t ��\n", queue[info_id]->ip);
		printf("\t   ��\t\t\t\t\t\t\t ��\n");
		printf("\t   ��\t\t �ƹڰ� : %d \t\t\t\t ��\n", queue[info_id]->now_pulse);
		printf("\t   ��\t\t �ִ� : %d \t\t\t\t ��\n", queue[info_id]->max_pulse);
		printf("\t   ��\t\t �ּڰ� : %d \t\t\t\t ��\n",   queue[info_id]->min_pulse);
		printf("\t   ��\t\t ��հ� : %.2lf \t\t\t ��\n", queue[info_id]->ave_pulse);
		printf("\t   ��������������������������������������������������������������������������������������������������������������\n");

		printf("\n\t\t������������������������������[ ���� ����ǥ ]������������������������������ \n");
		for(i=0; i < 10; i++)   {
			// ��������ǥ ���
			if(cnt >= 100)   
				printf("\t\t\t%d ~ %d��", cnt, cnt+19);
			else
				printf("\t\t\t%d ~ %d\t ��", cnt, cnt+19);

			for(j=0; j<queue[info_id]->level[i]; j++){
				printf("��");
			}
			cnt += 20;
			printf("\n");

		}
		printf("\t\t��������������������������������������������������������������������������������������������\n");
	}
}

// ������ �Լ�1
//unsigned WINAPI ThreadFunc1(void *arg)   {
void * ThreadFunc1(void *arg) {
	int i = 0;
	int cSize;
	cSize = sizeof(clnt_adr);

	while(cmd != 'q' )   {
		recvfrom(serv_sock, (char *)buf, sizeof(buf), 0, (struct sockaddr*)&clnt_adr, &cSize);
		if(buf[BUF_STATUS] == STATUS_EXIT)   {
			// Ŭ���̾�Ʈ�κ��� ���� BUF_STATUS�� 2, �� ����ó���̸�
			ID= SearchID( inet_ntoa(clnt_adr.sin_addr) );
			queue[ID]->now_status = STATUS_EXIT;

			sem_wait(&delay);
			//-----------------------------------------------------------------------------
			PrintIndex();                     // �ε���ȭ�� ���
			PrintInfo( queue[ID] );               // ť ���� ���
			AllClientInfo();                  // ��� Ŭ���̾�Ʈ ���� ���
			if(type == 1)                     // Ŭ���̾�Ʈ �ڼ������� ����ʹٴ� ��û�� �޾�����
				DetailClientInfo();               // �Լ�ȣ��
			PrintMenu();                     // �޴� ���
			//-----------------------------------------------------------------------------
			sem_post(&delay);

			initialize_queue( queue[ID] );         // ť �ʱ�ȭ ����

			int cnt=0;
			for(i=0; i<CLIENT_MAX_NUM; i++)   {
				if(queue[i]->right_link == NULL)
					cnt++;
			}
			if(cnt == CLIENT_MAX_NUM)   {
				sleep(2);
				system("clear");
				printf(" > ��� Ŭ���̾�Ʈ�� ���� ����Ǿ����ϴ�... \n");
				printf(" > Ŭ���̾�Ʈ ������ ��ٸ������Դϴ�... \n");
			}

		}
		else
			// Ŭ���̾�Ʈ�κ��� ���� BUF_STATUS�� 2���ƴѰ��, �� �����̸�
			insert_queue( inet_ntoa(clnt_adr.sin_addr), buf[BUF_PULSE] );
	}

	while(cmd == 'q')   {
		// ����޴��� ���ù޾�����
		int cnt=0;
		
		recvfrom(serv_sock, (char *)buf, sizeof(buf), 0, (struct sockaddr*)&clnt_adr, &cSize );

		ID= SearchID( inet_ntoa(clnt_adr.sin_addr) );

		buf[BUF_STATUS] = STATUS_EXIT;                     // ���� EXIT( = 2 )
		queue[ID]->now_status = STATUS_EXIT;

		initialize_queue( queue[ID] );                     // ť �ʱ�ȭ ����

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

// ������ �Լ�1
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
			PrintIndex();                     // �ε���ȭ�� ���
			PrintInfo( queue[ID] );               // ť ���� ���
			AllClientInfo();                  // ��� Ŭ���̾�Ʈ ���� ���
			if(type == 1)       // Ŭ���̾�Ʈ �ڼ������� ����ʹٴ� ��û�� �޾�����
				DetailClientInfo();        // �Լ�ȣ��
			PrintMenu();                     // �޴� ���
//-----------------------------------------------------------------------------
			sem_post(&delay);
			sleep(3);   // 3�ʸ��� ȭ�����
		}
	}

	return 0;
