#define SEND "유저 1"
#define RECV "유저 2"
//쓰레드에서 사용되는 함수와 로그 작성 함수는 모두 헤더파일에 정의되어 있다.
#include "chat.h"
int main(int argc, char** argv){
	
	//제공 기능 및 이용법에 대한 안내이다.
	printf("안녕하세요! %s %d님(이)가 체팅방에 입장하셨습니다.\n",SEND, getpid());
	printf("------------------------------------\n");
	printf("이용 방법 안내\n");
	printf("0.채팅 이용 가능 시간은 2분입니다.\n");
	printf("1.해당 대화는 모두 로그로 기록됩니다.\n");
	printf("2.체팅을 종료하고 싶으시면 \\q 를 입력해주세요.\n");
	printf("3.이모티콘을 보내고 싶으시면 \\e 를 입력해주세요.\n");
	printf("------------------------------------\n");
	
	struct mq_attr attr;
	pthread_t sendT, recvT, timeT;
	int ret, status = 0;
	//채팅 기록을 저장하기 위한 로그 파일을 만든다. 이때 파일이름은 pid가 된다.
	char namebuf[10];
	sprintf(namebuf, "%d.txt", getpid());
	
	//채팅 기록을 위한 상호배제를 위해 뮤텍스를 생성한다.
	pthread_mutex_init(&mutex, NULL);
	attr.mq_maxmsg = MAX_MSG;
	attr.mq_msgsize = MSG_SIZE;
	
	//채팅에 필요한 메시지 큐와 채팅 기록을 저장하기 위한 파일디스크립터를 생성한다.
	//유저1은 mq1을 전송용, mq2를 수신용으로 사용한다. 
	//유저2는 mq1을 수신용, mq2를 전송용으로 사용한다.
	mq1 = mq_open(MQ_1, O_CREAT | O_RDWR, 0666, &attr);
	mq2 = mq_open(MQ_2, O_CREAT | O_RDWR, 0666, &attr);
	fd = open(namebuf, O_WRONLY | O_CREAT, 0666);
	
	//메시지 큐 생성 에러를 체크한다.
	if ((mq1 == (mqd_t)-1) || (mq2 == (mqd_t)-1)){
		perror("open message queue error");
		exit(0);
	}
	//파일디스크립터 생성 에러를 체크한다.
	if (fd == -1){
		printf("cannot open file %s \n", namebuf);
		exit(0);
	}
	//수신을 위한 쓰레드를 생성하고 에러를 체크한다.
	ret = pthread_create(&sendT, NULL, send_thread, (void*)&mq1);
	if (ret < 0){
		perror("thread create error : ");
		exit(0);
	}
	//송신을 위한 쓰레드를 생성하고 에러를 체크한다.
	ret = pthread_create(&recvT, NULL, recv_thread, (void*)&mq2);
	if (ret < 0){
		perror("thread create error : ");
		exit(0);
	}
	//이용시간 체크에 대한 쓰레드를 생성하고 에러를 체크한다.
	ret = pthread_create(&timeT, NULL, time_thread, NULL);
	if (ret < 0){
		perror("thread create error : ");
		exit(0);
	}
	//송신 버퍼에 시작의 의미를 알리는 /s담아 메시지큐에 송신한다.
	strcpy(send_buf, "/s");
	mq_send(mq1, send_buf, strlen(send_buf), 0);
	
	//송신 쓰레드의 종료를 기다린다.
	pthread_join(sendT, (void**)&status);
	//수신 쓰레드의 종료를 기다린다.
	pthread_join(recvT, (void**)&status);
	//이용시간에 대한 쓰레드의 종료를 기다린다.
	pthread_join(timeT, NULL);
	
	return 0;
}
