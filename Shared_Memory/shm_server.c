// shared memory - server

#include "shm.h"

struct sendbuf
{
	int arr[50]; // 좌석 정보, 0이면 빈자리, 1이면 예약됨
};
struct recvbuf
{
	int data; // 반납 or 예약하려는 자리
	int myseat; // 소유중인 자리
	int serv_pid;
	int clnt_pid; // 클라이언트 pid
};
struct resultbuf
{
	int result; // 서버에서 클라이언트로 보내는 요청 결과 값
};
typedef struct Node
{
	int pid;
	struct Node* link;
}Node;

void* send_data(); // 서버에서 클라이언트로 좌석 정보를 보냄
void* read_input(); // 클라이언트의 요청을 받음
void* time_on(); // close_time까지 시간을 재는 함수
void send_result(int clnt_pid); // 클라이언트에게 결과값을 보냄
void ipc_remove(); // 인터럽트 종료 시, ipc 메모리를 지우고 종료
void input_read(); // recv_thread를 생성

struct sendbuf *sndbuf;
struct recvbuf *rcvbuf;
struct resultbuf *rsbuf; // 서버에서 처리한 결과를 보내줌
struct shmid_ds shm_status; // 메시지 큐의 상태 구조체

// 클라이언트 pid 관련
Node* head; // 클라이언트 pid 리스트를 가리키는 포인터
void addNodeLast(Node* head, int pid); // 연결리스트에 새로운 클라이언트의 pid값 추가 
Node* newNode(int pid); // 새로운 노드 생성

key_t key_id, key_id2, key_id3;
pthread_t send_thread, recv_thread, time_thread;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // 뮤텍스 초기화

int data;
int serv_pid;

/* 메인 함수 */
int main(void)
{
	struct sigaction sigact, inter;
	
	// Shared Memory 공간 요청
	key_id = shmget((key_t)60080, sizeof(struct sendbuf), IPC_CREAT | 0666); // send용
	key_id2 = shmget((key_t)60081, sizeof(struct recvbuf), IPC_CREAT | 0666); // recv용
	key_id3 = shmget((key_t)60082, sizeof(struct resultbuf), IPC_CREAT | 0666); // result send용

	// Shared Memory 제대로 할당되었는지
	if (key_id < 0 || key_id2 < 0 || key_id3 < 0) {
		perror("shmget error : ");
		return 0;
	}

	serv_pid = getpid();

	// shared memory 연결
	sndbuf = (struct sendbuf *)shmat(key_id, NULL, SHM_RND);
	if (sndbuf == -1){
		perror("sndbuf shmat error : ");
		exit(1);
	}
	rcvbuf = (struct recvbuf *)shmat(key_id2, NULL, SHM_RND);
	if (rcvbuf == -1){
		perror("rcvbuf shmat error : ");
		exit(1);
	}
	rsbuf = (struct sendbuf *)shmat(key_id3, NULL, SHM_RND);
	if (rsbuf == -1){
		perror("rsbuf shmat error : ");
		exit(1);
	}
	rcvbuf->serv_pid = getpid();	// rcvbuf에 서버의 pid 저장


	// 인터럽트 시그널 -> 키보드 입력 시그널
	inter.sa_handler = ipc_remove;
	sigemptyset(&inter.sa_mask);
	inter.sa_flags = SA_INTERRUPT;

	// 클라이언트에게서 signal이 들어오면 read_input 실행
	sigact.sa_handler = input_read;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = SA_INTERRUPT;

	if (sigaction(SIGUSR2, &sigact, NULL) < 0 || sigaction(SIGINT, &inter, NULL) < 0)
	{
		perror("sigaction error : ");
		return 0;
	}

	// 좌석 초기화
    for (int i = 0; i < 50; i++) {
		sndbuf->arr[i] = 0; // 0으로 초기화
	}

	while (1) {
		head = (Node*)malloc(sizeof(Node)); // 클라이언트 pid를 담는 연결리스트 head
		head->pid = 0;
		head->link = NULL;

		pthread_create(&time_thread, NULL, time_on, NULL); // 타이머 시작
		pthread_join(time_thread, NULL);

		free(head);
	}
	return 0;
}

/* 클라이언트에게 좌석 정보 보냈다고 출력 */
void* send_data() {
	printf("좌석 정보를 전송하였습니다.\n");
}

/* recv_thread를 생성하는 함수 */
void input_read() {
	pthread_create(&recv_thread, NULL, read_input, NULL);
	pthread_join(recv_thread, NULL);
	return;
}

/* 클라이언트의 요청을 받고 이를 처리하는 함수 */
void* read_input() {

	addNodeLast(head, rcvbuf->clnt_pid); // 클라이언트List에 pid 추가

	pthread_mutex_lock(&mutex); // 뮤텍스 lock -> 클라이언트가 좌석 배열에(공유 변수) 대해 write 하기때문에 race condition이 일어나지 않도록 mutex를 사용해야한다.

	int data = rcvbuf->data; // 클라이언트가 보낸 요청 값
	int myseat = rcvbuf->myseat; // 클라이언트가 선점하고 있는 좌석 (없으면 -1)
	//sndbuf.msgtype = rcvbuf.clnt_pid; // 요청을 보낸 클라이언트로 타겟팅
	
	if (rcvbuf->data == -1) { // 보낸 요청 값이 -1 이면 좌석 정보를 보내줌
		pthread_create(&send_thread, NULL, send_data, NULL); // sendt_thread 생성 및 실행
		pthread_join(send_thread, NULL);
	}
	else { // 좌석 선점, 반납 및 중복 처리
		if (sndbuf->arr[data - 1] == 0) { // 선점 가능
			sndbuf->arr[data - 1] = 1; // 해당 좌석을 선점하고
			rsbuf->result = 1; // 결과값 1 - 성공
			send_result(rcvbuf->clnt_pid);	// 공유메모리에 result값 저장됐음을 클라이언트에게 알림
		}
		else { // 해당 좌석을 누군가 선점 중
			if (data == myseat) { // 본인이 선점중-> 반납
				sndbuf->arr[data - 1] = 0;
				rsbuf->result = 1;
				send_result(rcvbuf->clnt_pid);
			}
			else {
				rsbuf->result = 0;
				send_result(rcvbuf->clnt_pid);
			}
		}
	}
	pthread_mutex_unlock(&mutex); // 뮤텍스 unlock
}

/* 폐장 시간까지의 시간을 재고 폐장 시간이 되면 모든 좌석을 반납, 클라이언트들과의 연결을 끊음 */
void* time_on() {

	sleep(100);
	for (int i = 0; i < 50; i++) {
		sndbuf->arr[i] = 0; // 0으로 모두 초기화 -> 좌석 모두 반납 
	}
	Node* now = head;
	while (now->link != NULL) { // 연결신호를 보낸 모든 클라이언트 pid 를 참조해 signal을 날림
		now = now->link;
		kill((*now).pid, SIGUSR2);
	}
	printf("모든 클라이언트와 연결 끊음\n");
	exit(0);
}

// 클라이언트에게 공유메모리가 업데이트됐음을 알리는 시그널 보냄
void send_result(int clnt_pid){
	kill(clnt_pid, SIGUSR1);
}

/* 클라이언트 pid list에 새로운 pid 추가 (중복처리) */
void addNodeLast(Node* head, int pid) {
	Node* now = head;
	Node* node = newNode(pid);

	while (now->link != NULL) {
		now = now->link;
		if (now->pid == pid) { // 이미 존재하는 pid면 pass
			return;
		}
	}
	if (head->link == NULL) // 존재하는 pid가 없으면
		head->link = node;
	else
		now->link = node;
}

/* 새로운 노드 생성 */
Node* newNode(int pid) {
	Node* node = (Node*)malloc(sizeof(Node));
	node->pid = pid;
	node->link = NULL;
}

/* ctrl-c interrupt시 공유메모리 제거 */
void ipc_remove() {
	if (shmctl(key_id, IPC_RMID, 0) < 0 || shmctl(key_id2, IPC_RMID, 0) < 0 || shmctl(key_id3, IPC_RMID, 0) < 0) {
		perror("shared memroy remove fail : ");
		exit(1);
	}
	printf("공유메모리가 제거되었습니다.\n");
	exit(1);
}

