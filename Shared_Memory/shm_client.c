// shared memory - client
#include "shm.h"
#define BILLION 1000000000L;

struct recvbuf {
	int arr[50]; // 좌석 정보, 0이면 빈자리, 1이면 예약됨
};
struct sendbuf {
	int data; // 반납 or 예약하려는 자리
	int myseat; // 현재 소유중인 자리, 없으면 -1
	int serv_pid;
	int clnt_pid; // 클라이언트의 pid
};
struct resultbuf {
	int result; // 서버로 부터 받은 결과 값
};

void* read_data(); // 좌석 정보를 읽어옴
void* write_input(); // 좌석 정보를 원하면 -1, 좌석 예약/반납을 원하면 1~50번 까지의 좌석 번호를 넘김
void recv_result(); // 서버로 부터 처리된 결과값을 읽어옴
void wait_result();
void* time_on(); // 좌석을 선점중인 시간을 재는 타이머
void close_time(); // 폐장 시간 시, 모든 클라이언트 종료

struct recvbuf *rcvbuf; // 좌석 정보를 받는 용도의 구조체
struct sendbuf *sndbuf; // 요청을 보내는 용도의 구조체
struct resultbuf *rsbuf; // 결과값을 받는 용도의 구조체
struct shmid_ds shm_status, shm_status2; // 메시지 큐 상태 구조체

key_t key_id, key_id2, key_id3;	// 3개의 shm 공간 만들 것이므로 키값 3개 할당받아야 함
pthread_t recv_thread, send_thread, rs_thread, time_thread;	// thread 생성 시, id 저장할 공간
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;	// Sychronization tool - mutext lock 사용

int msgtype = 1;
int flag = 0, myseat = -1; // flag가 1이면 선점 중, myseat는 현재 선점하고 있는 좌석, -1 이면 선점중이 아니다
int serv_pid, clnt_pid; // 서버 pid, 클라이언트 pid
int is_done = 0;

int main(int argc, char** argv)
{
	int menu, m, n;
	struct sigaction sigact, sigact_cli;
	struct timespec start,stop;
	double accum;
	
	// Shared Memory 할당
	key_id = shmget((key_t)60080, sizeof(struct recvbuf), IPC_CREAT | 0666); // recv용
	key_id2 = shmget((key_t)60081, sizeof(struct sendbuf), IPC_CREAT | 0666); // send용
	key_id3 = shmget((key_t)60082, sizeof(struct resultbuf), IPC_CREAT | 0666); // result recv용

	// -1 반환 시, shmget 실패
	if (key_id < 0 || key_id2 < 0 || key_id3 < 0) {
		perror("shmget error : ");
		return 0;
	}

	// 폐점 시간 시그널
	sigact.sa_handler = close_time;		// 핸들러 지정
	sigemptyset(&sigact.sa_mask);	// 시그널 처리 중 블록될 시그널 없음
	sigact.sa_flags = SA_INTERRUPT;	// 시그널 핸들러 동작 중 다른 인터럽트 발생 막음

	// 서버에게서 signal이 들어오면 recv_result 실행
	sigact_cli.sa_handler = recv_result;
	sigemptyset(&sigact_cli.sa_mask);
	sigact_cli.sa_flags = SA_INTERRUPT;

	// sigaction 반환값 0 : 성공, -1 : 실패
	if (sigaction(SIGUSR2, &sigact, NULL) < 0 || sigaction(SIGUSR1, &sigact_cli, NULL) < 0)
	{
		perror("sigaction error : ");
		return 0;
	}
	// 서버 pid 받아오기 -> IPC_STAT일 경우 shmid_ds에 shm 상태 저장
	if (shmctl(key_id, IPC_STAT, &shm_status) == -1) {
		perror("shmctl failed");
		return 0;
	}

	// 클라이언트 pid 받아오기
	clnt_pid = getpid();
	printf("clng_pid : %d\n", clnt_pid);

	// shared memory 연결
	rcvbuf = (struct recvbuf *)shmat(key_id, NULL, SHM_RND);
	if (rcvbuf == -1)
	{
		perror("rcvbuf shmat error : ");
		return 0;
	}
	sndbuf = (struct sendbuf *)shmat(key_id2, NULL, SHM_RND);
	if (sndbuf == -1)
	{
		perror("sndbuf shmat error : ");
		return 0;
	}
	rsbuf = (struct resultbuf *)shmat(key_id3, NULL, SHM_RND);
	if (rsbuf == -1) {
		perror("rsbuf recv error : ");
		return 0;
	}
	serv_pid = sndbuf->serv_pid;

	while (1) {
		printf("\n안녕하세요! 산기시네마입니다.!\n");
		printf("--------------------\n1 = 좌석현황 보기\n2 = 예약 및 예약취소\n3 = 종료하기\n--------------------\n\n명령  입력 : ");
		scanf("%d", &menu);

		switch (menu) {

		case 1: // 좌석 정보 얻기
			// 시간측정시작
			if (clock_gettime(CLOCK_MONOTONIC, &start) == -1){
				perror("clock gettime");
				return EXIT_FAILURE;
			}
			m = -1;
			pthread_create(&send_thread, NULL, write_input, (void*)&m); // 서버에게 -1 이라는 요청값을 보냄
			pthread_join(send_thread, NULL);	
			pthread_create(&recv_thread, NULL, read_data, NULL); // 좌석 정보 얻는 쓰레드 수행
			pthread_join(recv_thread, NULL);
			
			// 시간측정종료
			if (clock_gettime(CLOCK_MONOTONIC, &stop) == -1){
				perror("clock gettime");
				return EXIT_FAILURE;
			}

			accum = (stop.tv_sec - start.tv_sec) + (double)(stop.tv_nsec - start.tv_nsec) / (double)BILLION;
			printf("걸린 시간 : %.9f\n", accum);
			break;

		case 2: // 반납 or 예약
			if (myseat > 0)
				printf("현재 예약한 좌석 : %d번\n", myseat);
			else
				printf("현재 예약한 좌석이 없습니다.\n");
			printf("좌석 번호를 입력해주세요 : ");
			scanf("%d", &n);

			// 시간측정시작
			if (clock_gettime(CLOCK_MONOTONIC, &start) == -1){
				perror("clock gettime");
				return EXIT_FAILURE;
			}

			if (flag == 0) { // 선점하고있는 좌석이 없으면
				pthread_mutex_lock(&mutex); // 뮤텍스 lock - 공유 변수인 좌석 값을 write 하기 때문에 mutex로 임계영역을 보호해야한다.

				pthread_create(&send_thread, NULL, write_input, (void*)&n); // send_thread를 통해 서버에게 선점할 좌석 값을 보냄
				pthread_join(send_thread, NULL);
				pthread_create(&rs_thread, NULL, wait_result, NULL);	// 공유메모리에 rsbuf->result값 저장될 때까지 기다림
				pthread_join(rs_thread, NULL);

				if (rsbuf->result == 0)
					printf("이미 예약된 좌석입니다. 다른 좌석을 선택하세요.\n");
				else{
					flag = 1;
					myseat = n;
					printf("%d번 좌석 예약 완료\n", myseat);
					pthread_create(&time_thread, NULL, time_on, NULL);
					pthread_detach(time_thread);
				}
				pthread_mutex_unlock(&mutex); // mutex unlock
			}
			else { // 이미 선점하고 있는 좌석이 있음
				if (myseat != n) // 다른 자리 예약 시도
					printf("이미 예약한 좌석이 있습니다. 좌석 예약 취소를 해주세요.\n");
				else { // 반납
					pthread_mutex_lock(&mutex); // mutex lock
					pthread_create(&send_thread, NULL, write_input, (void*)&n); // send_thread로 반납하고자 하는 좌석 번호를 보냄
					pthread_join(send_thread, NULL);
					pthread_create(&rs_thread, NULL, wait_result, NULL);
					pthread_join(rs_thread, NULL);

					if (rsbuf->result == 1) {
						printf("예약 취소 하였습니다.\n");
						flag = 0; // 반납 후, flag = 0, myseat = -1로 초기화
						myseat = -1;
						pthread_cancel(time_thread); // 좌석을 반납했으므로 타이머는 취소
					}
					else
						printf("예약 실패\n");
					pthread_mutex_unlock(&mutex); // mutex unlock
				}
			}

			// 시간측정종료
			if (clock_gettime(CLOCK_MONOTONIC, &stop) == -1){
				perror("clock gettime");
				return EXIT_FAILURE;
			}

			accum = (stop.tv_sec - start.tv_sec) + (double)(stop.tv_nsec - start.tv_nsec) / (double)BILLION;
			printf("걸린 시간 : %.9f\n", accum);			
			break;

		case 3: // 종료
			// 시간측정시작
			if (clock_gettime(CLOCK_MONOTONIC, &start) == -1){
				perror("clock gettime");
				return EXIT_FAILURE;
			}

			if (flag == 1) {
				pthread_mutex_lock(&mutex); // 종료전 반납을 위해 mutex lock
				pthread_create(&send_thread, NULL, write_input, (void*)&myseat); // 반납할 좌석 번호를 서버에게 보냄
				pthread_join(send_thread, NULL);
				pthread_create(&rs_thread, NULL, wait_result, NULL);
				pthread_join(rs_thread, NULL);

				if (rsbuf->result == 1) {
					flag = 0; // 반납 후, flag = 0, myseat = -1로 초기화
					myseat = -1;
					printf("예약을 취소하셨습니다.\n");
					printf("프로그램을 종료합니다\n");
				}
				else{
					printf("프로그램을 종료합니다\n");
				}
				pthread_mutex_unlock(&mutex); // mutex unlock
			}
			pthread_cancel(time_thread); // 타이머 쓰레드 취소
			// 시간측정종료
			if (clock_gettime(CLOCK_MONOTONIC, &stop) == -1){
				perror("clock gettime");
				return EXIT_FAILURE;
			}

			accum = (stop.tv_sec - start.tv_sec) + (double)(stop.tv_nsec - start.tv_nsec) / (double)BILLION;
			printf("걸린 시간 : %.9f\n", accum);
			pthread_exit(NULL); // 메인 쓰레드 exit
			break;

		default:
			printf("잘못 입력하셨습니다.\n");
			break;
		}
	}
	return 0;
}

/* 서버로부터 좌석 정보를 읽어옴 */
void* read_data()
{
	printf("산기시네마 좌석 정보입니다.\n");
	for (int i = 0; i < sizeof(rcvbuf->arr) / sizeof(int); i++) {
		int moduler = i / 10;
		switch (moduler) {
		case 0:
			printf("A열 %02d번 좌석 : [%d] ", i + 1, rcvbuf->arr[i]);
			break;
		case 1:
			printf("B열 %d번 좌석 : [%d] ", i + 1, rcvbuf->arr[i]);
			break;
		case 2:
			printf("C열 %d번 좌석 : [%d] ", i + 1, rcvbuf->arr[i]);
			break;
		case 3:
			printf("E열 %d번 좌석 : [%d] ", i + 1, rcvbuf->arr[i]);
			break;
		case 4:
			printf("E열 %d번 좌석 : [%d] ", i + 1, rcvbuf->arr[i]);
			break;
		}
		if (i % 10 == 9) {
			printf("\n");
		}
	}
	pthread_exit(NULL);
}

/* 서버에게 요청 값을 보냄 - 좌석 정보를 원하면 -1, 좌석 예약/반납을 원하면 1~50 */
void* write_input(void* data)
{
	sndbuf->data = *(int *)data; // 요청 좌석 값
	sndbuf->myseat = myseat; // 선점하고 있는 좌석 번호
	sndbuf->clnt_pid = clnt_pid; // 클라이언트 pid
	kill(serv_pid, SIGUSR2); // 서버에게 signal을 보내면 서버에서 input_read 함수 시행

	pthread_exit(NULL);
}

// 서버가 공유메모리에 데이터 다 쓰면 시그널에 의해 실행
void recv_result() {
	is_done = 1;
}

// 서버가 공유메모리에 데이터 다 쓸 때까지 기다림
void wait_result(){
	// is_done이 1이 될 때까지 반복
	while (is_done != 1){
		printf("wait...\n");
	}	
	is_done = 0;	// is_done 초기화
	pthread_exit(NULL);	// 쓰레드 종료
}

/* 선점하는 동안 돌아가는 타이머 */
void* time_on() {
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
void close_time() {
	printf("폐점 시간입니다. 안녕히 가세요\n");
	exit(1);
}
