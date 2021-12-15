#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_SIZE 1024

void msgHandle(int);
void msgRecv(int);

int main(){
	int serverfd , response;
	struct sockaddr_in serv_addr;
	pthread_t t;
	char send_msg[MAX_SIZE];

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
	serv_addr.sin_port = htons(8080);

	serverfd = socket(AF_INET , SOCK_STREAM , 0);
	response = connect(serverfd , (struct sockaddr*) &serv_addr , sizeof(serv_addr));

	pthread_create(&t , NULL , (void*) &msgHandle , (void*) serverfd);
	memset(send_msg , '\0' , MAX_SIZE);
	while(1){
		fflush(stdout);
		fgets(send_msg , MAX_SIZE , stdin);
		send(serverfd , send_msg , sizeof(send_msg) , 0);
		if(strncmp(send_msg , "/q" , 2) == 0){
			break;
		}
		memset(send_msg , '\0' , MAX_SIZE);
	}

	pthread_join(t , NULL);
	close(serverfd);
	return 0;
}

void msgHandle(int connfd){
	int ret;
	char recv_msg[MAX_SIZE];
	char send_msg[MAX_SIZE];
	char check[] = "check";

	while(1){
		memset(recv_msg , '\0' , MAX_SIZE);

		fflush(stdout);

		ret = recv(connfd , recv_msg , MAX_SIZE , 0);

		if(ret > 0){
			printf("%s" , recv_msg);
			if(strncmp(recv_msg , "bye" , 3) == 0){
				break;
			}
		}
	}
}
