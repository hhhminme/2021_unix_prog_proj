고급프로그래밍 설계과제물

프로젝트명: 열람식 좌석 예약 시스템
팀명: 도비(Doby)
팀원: 2017154028 이상현, 2017154020 양소영, 2015150016 배선영
제출일: 2019년 12월 2일


[프로젝트 설명]

여러 명의 클라이언트가 서버에 접속하여 열람실의 좌석을 예매할 수있는 프로그램으로
Synchronize tool을 이용해 좌석에 대한  경쟁상태가 일어나지 않도록 구현함.
같은 프로그램을 3개의 IPC(shared memory, message passing, pipe)를 이용하여 구현함
으로써 IPC기법에 따른 성능차이를 비교하고자 함. 


[ 소스코드]

1. Share Memory
	소스파일- shm_server.c shm_client.c share_ex.h
	실행파일- shm_server shm_client

2. Message Passing
	소스파일- msg_server.c msg_client.c msg.h
	실행파일- msg_server msg_client

3. Pipe
	소스파일- pipe_server.c pipe_client.c pipe.h
	실행파일- pipe_server pipe_client


[ 컴파일 ]

 'make' 명령을 통해 컴파일 


[ 실행방법 ]

 서버: './(ipc기법)_server' 명령 실행 
 클라이언트: './(ipc기법)_client' 명령 실행
 

