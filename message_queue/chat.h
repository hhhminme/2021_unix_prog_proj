#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <mqueue.h>
#include <pthread.h>
#include <time.h>

#define MQ_1 "/mq1"
#define MQ_2 "/mq2"
#define MSG_SIZE 256
#define MAX_MSG 5
#define MAX_LOG 512

pthread_mutex_t mutex;
int fd;
mqd_t mq1, mq2;
char send_buf[MSG_SIZE];
char recv_buf[MSG_SIZE];
char log_buf[MAX_LOG];
//시간을 확인하기 위한 변수이다.
char* ptr = '\0';
time_t ltime;
struct tm *today;
//이용 유저를 체크하기 위한 변수이나 실질적으로 사용되지 않는다.
short in_user = 0;
//이모티콘 구별에 사용되는 변수이다.
int emoji_num;

//채팅 로그를 작성하기 위한 함수이다. 두 유저가 피일디스크립터를 공유하므로 상호배제를 실시한다.
void logg_f(char* str, char* user){
	pthread_mutex_lock(&mutex);
	
	sprintf(log_buf, "----------[%s] chatting log file ---------- \n",user);
	//현재 채팅 시간을 구한다.
	time(&ltime);
	today = localtime(&ltime);
	ptr = asctime(today);
	ptr[strlen(ptr)-1] = '\0';
	//로그 버퍼에 담긴 시각, 유저명, 내용을 기록한다.
	sprintf(log_buf, "[%s] %s : %s \n", ptr, user, str);
	write(fd, log_buf, strlen(log_buf));
	//해당 내용을 기록하고 난 후 로그 버퍼를 초기화한다.
	memset(log_buf, '\0', sizeof(log_buf));
	pthread_mutex_unlock(&mutex);
}
//이용 시각을 체크하기위한 함수이다. 체팅 제한시간은 2분으로 두었고 이용시간이 만료되면 뮤텍스를 없애고 메시지큐를 닫고 프로그램이 종료된다.
void *time_thread(void* args){
	sleep(120);
	printf("체팅 시간이 만료되었습니다. 감사합니다. \n");

	pthread_mutex_destroy(&mutex);
	close(fd);
	mq_close(mq1);
	mq_close(mq2);
	mq_unlink(MQ_1);
	mq_unlink(MQ_2);
	exit(0);
}
//송신을 위한 함수이다. 
void *send_thread(void* args){
	while(1){
		//메시지를 입력받기 전 송신버퍼를 초기화한다.
		memset(send_buf, '\0', sizeof(send_buf));
		printf("메시지를 입력해주세요 > ");
		//사용자로부터 키보드로 입력받은 메시지를 송신버퍼에 저장한다.
		fgets(send_buf, sizeof(send_buf), stdin);
		send_buf[strlen(send_buf)-1] = '\0';

		if (mq_send(*(mqd_t*)args, send_buf, strlen(send_buf), 0) == -1){
			perror("mq_send()");
		}
		else {
			//송신에 오류가 없다면 해당 내용을 채팅 로그에 기록한다. 이때 SEND에는 보낸 유저가 담긴다.
			logg_f(send_buf, SEND);
			// /q를 입력하면 프로그램이 종료된다.
			if (strcmp(send_buf, "/q") == 0){
				printf("체팅이 종료되었습니다\n");
				pthread_mutex_destroy(&mutex);
				close(fd);
				mq_close(mq1);
				mq_close(mq2);
				if (in_user){
					mq_unlink(MQ_1);
					mq_unlink(MQ_2);
				}
				exit(0);
			}
			// /e를 입력하면 세가지 이모티콘 중 하나를 골라 전송할 수 있다.
			if (strcmp(send_buf, "/e") == 0){
				printf("보내고 싶은 이모티콘 숫자를 선택해주세요.\n");
				printf("--------------------------------\n");
				printf("1. 하트\n");
				printf("2. 웃음\n");
				printf("3. 눈물\n");
				printf("--------------------------------\n");
				fgets(send_buf, sizeof(send_buf), stdin);
				send_buf[strlen(send_buf)-1] = '\0';
				if(strcmp(send_buf,"1") == 0){
					memset(send_buf, '\0', sizeof(send_buf));
					//송신버퍼에 하트 문자열을 담아 상대방에게 전송한다. 전송받은 유저는 해당하는 이모티콘을 화면에 출력한다.
					strcpy(send_buf,"하트");
					printf("[%s]%s : %s\n",ptr, SEND, send_buf);
					if (mq_send(*(mqd_t*)args, send_buf, strlen(send_buf), 0) == -1){
						perror("mq_send()");
					}
				}
				else if(strcmp(send_buf,"2") == 0){
				//송신버퍼에 웃음 문자열을 담아 상대방에게 전송한다. 전송받은 유저는 해당하는 이모티콘을 화면에 출력한다.
					memset(send_buf, '\0', sizeof(send_buf));
					strcpy(send_buf,"웃음");
					printf("[%s]%s : %s\n",ptr, SEND, send_buf);
					if (mq_send(*(mqd_t*)args, send_buf, strlen(send_buf), 0) == -1){
						perror("mq_send()");
					}
				}
				else if(strcmp(send_buf,"3") == 0){
				//송신버퍼에 눈물 문자열을 담아 상대방에게 전송한다. 전송받은 유저는 해당하는 이모티콘을 화면에 출력한다.
					memset(send_buf, '\0', sizeof(send_buf));
					strcpy(send_buf,"눈물");
					printf("[%s]%s : %s\n",ptr, SEND, send_buf);
					if (mq_send(*(mqd_t*)args, send_buf, strlen(send_buf), 0) == -1){
						perror("mq_send()");
					}
				}
			}
			//송신버퍼의 내역을 시간과 함께 보낸유저의 화면에 출력한다.
			else{
				printf("[%s]%s : %s\n",ptr, SEND, send_buf);
			}
		}
	}
}
//수신을 담당하는 함수이다.
void* recv_thread(void* args){
	while(1){
		//수신 버퍼에 있는 내용이 다 비워질때까지 해당 함수는 실행된다.
		while(mq_receive(*(mqd_t*)args, recv_buf, sizeof(recv_buf), 0) > 0){
			//수신 받은 내역은 채팅 로그에 기록된다. 이때 RECV에는 메시지를 받은 유저가 담긴다.
			logg_f(recv_buf, RECV);
			//프로그램 시작 시 수신 버퍼의 내역을 비운다. 해당 내역을 비우지 않을 경우 기존 프로그램에서 버퍼에 담긴 메시지가 화면에 출력된다.
			if (strcmp(recv_buf, "/s") == 0){
				//in_user = 0;
				memset(recv_buf, '\0', sizeof(recv_buf));
				break;
			}
			// 송신측에서 /q를 입력하여 종료했을때 받는 유저에게 종료되었음을 알린다. 
			else if (strcmp(recv_buf, "/q") == 0){
				//in_user = 1;
				printf("\n다른 유저가 체팅을 종료하였습니다.\n");
				memset(recv_buf, '\0', sizeof(recv_buf));
				break;
			}
			//하트 이모티콘을 받았을 때에 대한 내역을 출력한다.
			else if (strcmp(recv_buf, "하트") == 0){
				printf("ღ'ᴗ'ღ I LOVE YOU~!");
				printf("\n[%s]%s : %s\n",ptr, RECV, recv_buf);
				memset(recv_buf, '\0', sizeof(recv_buf));
			}
			//웃음 이모티콘을 받았을 때에 대한 내역을 출력한다.
			else if (strcmp(recv_buf, "웃음") == 0){
				printf("(๑･̑◡･̑๑) AWAYS BE HAPPY!!");
				printf("\n[%s]%s : %s\n",ptr, RECV, recv_buf);
				memset(recv_buf, '\0', sizeof(recv_buf));
				
			}
			//눈물 이모티콘을 받았을 때에 대한 내역을 출력한다.
			else if (strcmp(recv_buf, "눈물") == 0){
				printf("( Ĭ ^ Ĭ )  DON'T WORRY!! CHEER UP!");
				printf("\n[%s]%s : %s\n",ptr, RECV, recv_buf);
				memset(recv_buf, '\0', sizeof(recv_buf));
			}
			//시간과 함께 수신 유저와 내역을 화면에 출력한다.
			else{
				printf("\n[%s]%s : %s\n",ptr, RECV, recv_buf);
				memset(recv_buf, '\0', sizeof(recv_buf));
			}
		}
	}
}


