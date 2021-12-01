// Message Passing - client

#include "msg.h"

struct recvbuf {
	long msgtype; // 클라이언트 pid 값으로 받음
	int arr[50]; // 좌석 정보, 0이면 빈자리, 1이면 예약됨
};
struct sendbuf {
	long msgtype; // 서버 pid값 으로 보냄
	int data; // 반납 or 예약하려는 자리
	int myseat; // 현재 소유중인 자리, 없으면 -1
	int clnt_pid; // 클라이언트의 pid
};
struct resultbuf{
	long msgtype; // 클라이언트 pid값으로 받음
	int result; // 서버로 부터 받은 결과 값
};

void* read_data(); // 좌석 정보를 읽어옴
void* write_input(); // 좌석 정보를 원하면 -1, 좌석 예약/반납을 원하면 1~50번 까지의 좌석 번호를 넘김
void recv_result(); // 서버로 부터 처리된 결과값을 읽어옴
void* time_on(); // 좌석을 선점중인 시간을 재는 타이머
void close_time(); // 폐장 시간 시, 모든 클라이언트 종료

struct recvbuf rcvbuf; // 좌석 정보를 받는 용도의 구조체
struct sendbuf sndbuf; // 요청을 보내는 용도의 구조체
struct resultbuf rsbuf; // 결과값을 받는 용도의 구조체
struct msqid_ds msq_status, msq_status2; // 메시지 큐 상태 구조체

key_t key_id, key_id2, key_id3;
pthread_t recv_thread, send_thread, time_thread; 
pthread_mutex_t mutex  = PTHREAD_MUTEX_INITIALIZER;

int msgtype = 1;
int flag = 0, myseat = -1; // flag가 1이면 선점 중, myseat는 현재 선점하고 있는 좌석, -1 이면 선점중이 아니다
int serv_pid, clnt_pid; // 서버 pid, 클라이언트 pid

int main(int argc, char** argv)
{
	int menu, m, n;
	struct sigaction sigact;
	
	// Message queue 생성
	key_id = msgget((key_t)60021, IPC_CREAT | 0666); // recv용 queue
        key_id2 = msgget((key_t)60022, IPC_CREAT | 0666); // send용 queue
        key_id3 = msgget((key_t)60023, IPC_CREAT | 0666); // result recv용 queue

	if (key_id < 0 || key_id2 < 0 || key_id3 < 0) {
		perror("msgget error : ");
		return 0;
	}

	// 폐점 시간 시그널
	sigact.sa_handler = close_time; 
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = SA_INTERRUPT;

	if(sigaction(SIGUSR2, &sigact, NULL) <0)
	{	// 서버 pid 받아오기
		perror("sigaction error : ");	
		return 0;
	}

	// 서버 pid 받아오기
	if(msgctl(key_id, IPC_STAT, &msq_status) == -1){
		perror("msgctl failed");
		return 0;
	}

	// printf("Last send by proc %d\n", msq_status.msg_lspid);
	serv_pid = msq_status.msg_lspid;

	// 클라이언트 pid 받아오기
	clnt_pid = getpid();

	while(1){
		printf("--------------------\n1 = 좌석현황 보기\n2 = 반납or예약\n3 = 종료\n--------------------\n\n명령  입력 : ");
                scanf("%d", &menu);

		switch(menu){

		case 1 : // 좌석 정보 얻기
			m = -1;
			pthread_create(&send_thread, NULL, write_input, (void*)&m); // 서버에게 -1 이라는 요청값을 보냄
			pthread_join(send_thread, NULL);

			pthread_create(&recv_thread, NULL, read_data, NULL); // 좌석 정보 얻는 쓰레드 수행
			pthread_join(recv_thread, NULL);
			break;

		case 2 : // 반납 or 예약
			if (myseat > 0)
				printf("현재 선점하고 있는 좌석 : %d번\n", myseat);
			else
				printf("현재 선점하고 있는 좌석이 없습니다.\n");
			printf("좌석 번호를 입력해주세요 : ");
			scanf("%d", &n);

			if(flag == 0){ // 선점하고있는 좌석이 없으면
				pthread_mutex_lock(&mutex); // 뮤텍스 lock - 공유 변수인 좌석 값을 write 하기 때문에 mutex로 임계영역을 보호해야한다.

				pthread_create(&send_thread, NULL, write_input, (void*)&n); // send_thread를 통해 서버에게 선점할 좌석 값을 보냄
				pthread_join(send_thread, NULL);

				recv_result(); // 결과 값을 받음

				if(rsbuf.result == 0) // 이미 누군가가 선점한 자리였다면
					printf("이미 예약된 자리입니다. 다른 자리를 선택하세요\n");
				else{ // 아니면 선점 가능
					flag = 1;
					myseat = n;
					printf("%d번 자리 예약 완료\n", myseat);
					pthread_create(&time_thread, NULL, time_on, NULL); // timer 실행
					pthread_detach(time_thread);
				}
				pthread_mutex_unlock(&mutex); // mutex unlock
			}
			else{ // 이미 선점하고 있는 좌석이 있음
				if(myseat != n) // 다른 자리 예약 시도
					printf("이미 예약한 자리가 있습니다. 반납 먼저 해주세요.\n");
				else{ // 반납
					pthread_mutex_lock(&mutex); // mutex lock
					pthread_create(&send_thread, NULL, write_input, (void*)&n); // send_thread로 반납하고자 하는 좌석 번호를 보냄
					pthread_join(send_thread, NULL);

					recv_result(); // 결과값 받음

					if(rsbuf.result == 1){
						printf("자리를 반납하였습니다.\n");
						flag = 0; // 반납 후, flag = 0, myseat = -1로 초기화
						myseat = -1;
						pthread_cancel(time_thread); // 좌석을 반납했으므로 타이머는 취소
					}
					else
						printf("반납실패\n");
					pthread_mutex_unlock(&mutex); // mutex unlock
				}
			}
			break;

		case 3 : // 종료
			if(flag == 1){
				pthread_mutex_lock(&mutex); // 종료전 반납을 위해 mutex lock
				pthread_create(&send_thread, NULL, write_input, (void*)&myseat); // 반납할 좌석 번호를 서버에게 보냄
				pthread_join(send_thread, NULL);
				recv_result();

				if(rsbuf.result == 1){
					flag = 0; // 반납 후, flag = 0, myseat = -1로 초기화
					myseat = -1;
					printf("자리를 반납하였습니다.\n");
					printf("프로그램을 종료합니다\n");
				}
				pthread_mutex_unlock(&mutex); // mutex unlock
			}
			pthread_cancel(time_thread); // 타이머 쓰레드 취소
			pthread_exit(NULL); // 메인 쓰레드 exit
			break;

		default :
			printf("잘못 입력하셨습니다.\n");
			break;
		}
	}
	return 0;
}

/* 서버로부터 좌석 정보를 읽어옴 */
void* read_data() 
{
	if (msgrcv(key_id, (void*)&rcvbuf, sizeof(struct recvbuf), clnt_pid, 0) == -1)
	{
		perror("msgrcv error : ");
		return 0;
	}
	printf("열람실 좌석 정보입니다.\n");
	for (int i = 0; i < sizeof(rcvbuf.arr) / sizeof(int); i++) {
		printf("%d번 좌석 : %d \n",i+1, rcvbuf.arr[i]); // 좌석 정보 출력
	}
	pthread_exit(NULL);
}

/* 서버에게 요청 값을 보냄 - 좌석 정보를 원하면 -1, 좌석 예약/반납을 원하면 1~50 */
void* write_input(void* data) 
{
	sndbuf.msgtype = serv_pid; // 서버 pid를 가지고 서버에서 메시지를 받기 위해 설정
	sndbuf.data = *((int*)data); // 요청 좌석 값
	sndbuf.myseat = myseat; // 선점하고 있는 좌석 번호
	sndbuf.clnt_pid = clnt_pid; // 클라이언트 pid

	if (msgsnd(key_id2, (void*)&sndbuf, sizeof(struct sendbuf), IPC_NOWAIT) == -1)
	{
		perror("msgsnd error : ");
		return 0;
	}

	kill(serv_pid, SIGUSR2); // 서버에게 signal을 보내면 서버에서 read_input 함수 시행

	pthread_exit(NULL);
}

/* 서버로부터 처리된 결과값을 받아옴 */
void recv_result(){
	msgrcv(key_id3, (void*)&rsbuf, sizeof(struct resultbuf), clnt_pid, 0);
/*
	if(msgrcv(key_id3, (void*)&rsbuf, sizeof(struct resultbuf), clnt_pid, 0) == -1)
	{
		perror("recv result error : ");
		// return 0;
	}
*/
	if(msgctl(key_id3, IPC_STAT, &msq_status2) == -1){
		perror("msgctl failed");
		exit(1);
	}
	// printf("result : %d 받음\n", rsbuf.result);;
}

/* 선점하는 동안 돌아가는 타이머 */
void* time_on(){
        sleep(30); // 2시간 동안 자리 사용할 수 있음
        printf("\ntime out!! 좌석이 반납되었습니다.\n");
        pthread_create(&send_thread, NULL, write_input, (void*)&myseat); // 반납 루틴
        pthread_join(send_thread, NULL);

	flag = 0;
	myseat = -1; // 좌석 값 초기화

	recv_result();
	pthread_exit(NULL);
}

/* 폐점 시간 시, 서버가 시그널을 보내면 수행되는 시그널 핸들러 함수 */
void close_time(){
	printf("폐점 시간입니다. 안녕히 가세요\n");
	exit(1);
}
