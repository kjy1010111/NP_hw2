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
#include <ctype.h>

#define MAX_SIZE 1024
#define MAX_PLAYER 16

int connfd[MAX_PLAYER];
char player_name[MAX_PLAYER][MAX_SIZE];
int play_with[MAX_PLAYER];

void serverInit();
int findEmptyConn();
void playerList(int);
void serverLogin(int);
void serverHandle(int);
void serverInstr(int);
int loginCheck(char* , char*);
void serverShutdown(int);
void gamePlay(int , int);
void printBoard(char* , char*);
int isWin(char*);
int b_isEmpty(char*);

int main(int argc, char **argv){
	int listenfd , index;
	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	socklen_t c_len;
	pthread_t server_cmd , server_sd;

	serverInit();

	listenfd = socket(AF_INET , SOCK_STREAM , 0);

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(8080);

	bind(listenfd , (struct sockaddr *) &server_addr , sizeof(server_addr));
	listen(listenfd , 32);

	printf("Server start\n");

	pthread_create(&server_sd , NULL , (void*) &serverShutdown , (void*) listenfd);

	while(1){
		c_len = sizeof(client_addr);
		index = findEmptyConn();
		connfd[index] = accept(listenfd , (struct sockaddr*) &client_addr , &c_len);

		printf("Client: %d connected\n" , index);

		pthread_create(malloc(sizeof(pthread_t)) , NULL , (void*) &serverHandle , (void*) index);
	}
}

void serverInit(){
	for(int i = 0;i < MAX_PLAYER;i++){
		connfd[i] = -1;
		memset(player_name[i] , '\0' , MAX_SIZE);
		play_with[i] = -1;
	}
}

int findEmptyConn(){
	for(int i = 0;i < MAX_PLAYER;i++){
		if(connfd[i] == -1)
			return i;
	}

	return -1;
}

void serverLogin(int index){
	char acc_msg[] = "Please login in first\nPlease enter your account name:\n";
	char pwd_msg[] = "Please enter your password:\n";
	char login_succ[] = "\nLogin success!\nEnter to continue\n";
	char login_fail[] = "\nLogin failed!\nPlease try again\nEnter to continue\n";
	char acc_input[MAX_SIZE] , pwd_input[MAX_SIZE] , tmp[MAX_SIZE];

	int ret;

	while(1){
		send(connfd[index] , acc_msg , sizeof(acc_msg) , 0);
		ret = recv(connfd[index] , acc_input , MAX_SIZE , 0);
		for(int i = 0;i < MAX_SIZE;i++){
			if((i == (MAX_SIZE - 1)) || (acc_input[i] == '\n')){
				acc_input[i] = '\0';
				break;
			}
		}
		send(connfd[index] , pwd_msg , sizeof(pwd_msg) , 0);
		ret = recv(connfd[index] , pwd_input , MAX_SIZE , 0);
		for(int i = 0;i < MAX_SIZE;i++){
			if((i == (MAX_SIZE - 1)) || (pwd_input[i] == '\n')){
				pwd_input[i] = '\0';
				break;
			}
		}

		if(loginCheck(acc_input , pwd_input)){
			send(connfd[index] , login_succ , sizeof(login_succ) , 0);
			ret = recv(connfd[index] , tmp , MAX_SIZE , 0);
			strcpy(player_name[index] , acc_input);
			break;
		}
		else{
			send(connfd[index] , login_fail , sizeof(login_fail) , 0);
			ret = recv(connfd[index] , tmp , MAX_SIZE , 0);
		}
	}

	printf("\nClient id: %d\nname: %s\nLogin success\n\n" , index , player_name[index]);
}

void playerList(int index){
	int ret;
	char send_tmp[MAX_SIZE] = "==========================================\n";
	char b_line[] = "==========================================\n> ";
	char output[] = "id\t\tuser_name\tgame_state\n";

	strcat(send_tmp , output);

	for(int i = 0;i < MAX_PLAYER;i++){
		if(connfd[i] != -1){
			int g_state;
			char tmp[MAX_SIZE];
			sprintf(tmp , "%d\t\t%s\t\t" , i , player_name[i]);
			if(play_with[i] == -1){
				strcat(tmp , "in Lobby\n");
			}
			else{
				strcat(tmp , "in Game\n");
			}

			strcat(send_tmp , tmp);
		}
	}

	strcat(send_tmp , b_line);
	send(connfd[index] , send_tmp , sizeof(send_tmp) , 0);
}

void serverInstr(int index){
	char instrs[] = "list\t(/l)\tlist online user\nrequest\t(/r id)\trequest player\nlogout\t(/q)\tlogout and quit server\n> ";
	char response[MAX_SIZE] , tmp[MAX_SIZE];
	int ret;

	send(connfd[index] , instrs , sizeof(instrs) , 0);

	while(1){
		if(play_with[index] != -1){
			continue;
		}
		memset(response , '\0' , MAX_SIZE);
		fflush(stdout);
		ret = recv(connfd[index] , response , MAX_SIZE , 0);
		for(int i = 0;i < MAX_SIZE;i++){
			if((response[i] == '\n') || (i == MAX_SIZE - 1)){
				response[i] = '\0';
				break;
			}
		}

		printf("user: %s> %s\n" , player_name[index] , response);

		if(strncmp(response , "/l" , 2) == 0){
			playerList(index);
		}
		else if(strncmp(response , "/q" , 2) == 0){
			char bye[] = "bye\n";

			printf("\nClient id: %d\nname: %s\nLogout success\n\n" , index , player_name[index]);
			send(connfd[index] , bye , sizeof(bye) , 0);
			memset(player_name[index] , '\0' , MAX_SIZE);
			connfd[index] = -1;
			break;
		}
		else if(strncmp(response , "/r" , 2) == 0){
			int id = -1 , n;
			char recv_id[MAX_SIZE];
			char req_fail[] = "Request failed\n> ";
			char req_cancel[] = "Request canceled\n> ";

			for(n = 2;n < MAX_SIZE;n++){
				if(response[n] == ' '){
					continue;
				}
				if(!isdigit(response[n])){
					n = -1;
					send(connfd[index] , req_fail , sizeof(req_fail) , 0);
					break;
				}
				else{
					break;
				}
			}

			if(n == -1 || n >= MAX_SIZE || !isdigit(response[n])){
				continue;
			}

			int shift_n = n;

			for(int i = n;i < MAX_SIZE;i++){
				if(isdigit(response[i])){
					recv_id[i - shift_n] = response[i];
				}
				else if(response[i] == '\0'){
					recv_id[i - shift_n] = response[i];
					id = atoi(recv_id);
					break;
				}
				else{
					id = -1;
					send(connfd[index] , req_fail , sizeof(req_fail) , 0);
					break;
				}
			}

			if(id != -1){
				if((id >= MAX_PLAYER) || (connfd[id] == -1)){
					char no_user[MAX_SIZE];
					sprintf(no_user , "No user's id is %d\n> " , id);
					send(connfd[index] , no_user , sizeof(no_user) , 0);
					continue;
				}
				if(id == index){
					char self_req[] = "Can't send request to yourself\n> ";
					send(connfd[index] , self_req , sizeof(self_req) , 0);
					continue;
				}
				if(play_with[id] != -1){
					char playing_msg[] = "User is playing\n> ";
					send(connfd[index] , playing_msg , sizeof(playing_msg) , 0);
					continue;
				}
				char req_confirm[MAX_SIZE];
				sprintf(req_confirm , "Send request to %s(y/n)> " , player_name[id]);
				send(connfd[index] , req_confirm , sizeof(req_confirm) , 0);
				memset(response , '\0' , MAX_SIZE);
				ret = recv(connfd[index] , response , MAX_SIZE , 0);

				if((strncmp(response , "y" , 1) == 0) || (strncmp(response , "Y" , 1) == 0)){
					char req[MAX_SIZE];
					play_with[id] = index;
					play_with[index] = id;
					sprintf(req , "\nuser: %s want to play with you\n" , player_name[index]);
					send(connfd[id] , req , sizeof(req) , 0);
					sprintf(req , "Accept?(y/n)> ");
					send(connfd[id] , req , sizeof(req) , 0);
					ret = recv(connfd[id] , response , MAX_SIZE , 0);

					if((strncmp(response , "y" , 1) == 0) || (strncmp(response , "Y" , 1) == 0)){
						char msg[MAX_SIZE];
						sprintf(msg , "Playing with %s\nEnter to start\n" , player_name[index]);
						send(connfd[id] , msg , sizeof(msg) , 0);
						sprintf(msg , "Playing with %s\nEnter to start\n" , player_name[id]);
						send(connfd[index] , msg , sizeof(msg) , 0);

						gamePlay(index , id);

						play_with[index] = -1;
						play_with[id] = -1;
					}
					else{
						char msg[] = "Request rejected\n> ";
						play_with[index] = -1;
						play_with[id] = -1;
						send(connfd[id] , msg , sizeof(msg) , 0);
						send(connfd[index] , msg , sizeof(msg) , 0);
					}
				}
				else if((strncmp(response , "n" , 1) == 0) || (strncmp(response , "N" , 1) == 0)){
					send(connfd[index] , req_cancel , sizeof(req_cancel) , 0);
				}
				else{
					send(connfd[index] , req_fail , sizeof(req_fail) , 0);
				}
			}
			else{
				send(connfd[index] , req_fail , sizeof(req_fail) , 0);
			}
		}
		else{
			char msg[] = "> ";
			send(connfd[index] , msg , sizeof(msg) , 0);
		}
	}
}

void printBoard(char *b , char *p){
	char tmp[MAX_SIZE] , fin[MAX_SIZE];
	memset(tmp , '\0' , MAX_SIZE);
	memset(fin , '\0' , MAX_SIZE);

	sprintf(tmp , "-------------\n");
	strcat(fin , tmp);
	memset(tmp , '\0' , MAX_SIZE);
	sprintf(tmp , "| %c | %c | %c |\n" , b[0] , b[1] , b[2]);
	strcat(fin , tmp);
	memset(tmp , '\0' , MAX_SIZE);
	sprintf(tmp , "----+---+----\n");
	strcat(fin , tmp);
	memset(tmp , '\0' , MAX_SIZE);
	sprintf(tmp , "| %c | %c | %c |\n" , b[3] , b[4] , b[5]);
	strcat(fin , tmp);
	memset(tmp , '\0' , MAX_SIZE);
	sprintf(tmp , "----+---+----\n");
	strcat(fin , tmp);
	memset(tmp , '\0' , MAX_SIZE);
	sprintf(tmp , "| %c | %c | %c |\n" , b[6] , b[7] , b[8]);
	strcat(fin , tmp);
	sprintf(tmp , "-------------\n");
	strcat(fin , tmp);
	memset(tmp , '\0' , MAX_SIZE);

	strcpy(p , fin);
	return;
}

int isWin(char *b){
	if((b[0] != ' ') && (b[0] == b[1]) && (b[1] == b[2]))
		return 1;
	if((b[3] != ' ') && (b[3] == b[4]) && (b[4] == b[5]))
		return 1;
	if((b[6] != ' ') && (b[6] == b[7]) && (b[7] == b[8]))
		return 1;
	if((b[0] != ' ') && (b[0] == b[3]) && (b[3] == b[6]))
		return 1;
	if((b[1] != ' ') && (b[1] == b[4]) && (b[4] == b[7]))
		return 1;
	if((b[2] != ' ') && (b[2] == b[5]) && (b[5] == b[8]))
		return 1;
	if((b[0] != ' ') && (b[0] == b[4]) && (b[4] == b[8]))
		return 1;
	if((b[2] != ' ') && (b[2] == b[4]) && (b[4] == b[6]))
		return 1;

	return 0;
}

int b_isEmpty(char *b){
	for(int i = 0;i < 10;i++){
		if(b[i] == ' ')
			return 1;
	}
	return 0;
}

void gamePlay(int a , int b){
	int ret , tmp , turn = 1;
	char board[10] , init_b[10];
	char recv_msg[MAX_SIZE] , send_msg[MAX_SIZE] , print_b[MAX_SIZE] , print_init[MAX_SIZE];
	for(int i = 0;i < 10;i++){
		board[i] = ' ';
		init_b[i] = i + '1';
	}
	board[9] = '\0';
	init_b[9] = '\0';

	printBoard(init_b , print_init);

	ret = recv(connfd[a] , recv_msg , MAX_SIZE , 0);
	ret = recv(connfd[b] , recv_msg , MAX_SIZE , 0);

	memset(send_msg , '\0' , MAX_SIZE);
	sprintf(send_msg , "%syou're 'O' you play first\n" , print_init);
	send(connfd[b] , send_msg , MAX_SIZE , 0);
	memset(send_msg , '\0' , MAX_SIZE);
	sprintf(send_msg , "%syou're 'X' you play after\n" , print_init);
	send(connfd[a] , send_msg , MAX_SIZE , 0);

	while(1){
		char choose[] = "\nYour turn\nChoose a number(1-9)> ";
		if(turn == 1){
			send(connfd[b] , choose , sizeof(choose) , 0);
			ret = recv(connfd[b] , recv_msg , MAX_SIZE , 0);
			int place;
			sscanf(recv_msg , "%d" , &place);

			if(board[place - 1] != ' '){
				char msg[] = "You can't place here\nChoose again\n> ";
				send(connfd[b] , msg , sizeof(msg) , 0);
				continue;
			}

			board[place - 1] = 'O';
			printBoard(board , print_b);

			if(isWin(board)){
				memset(send_msg , '\0' , MAX_SIZE);
				sprintf(send_msg , "%s\nGAME OVER!!!\nYou win!\n" , print_b);
				send(connfd[b] , send_msg , sizeof(send_msg) , 0);
				memset(send_msg , '\0' , MAX_SIZE);
				sprintf(send_msg , "%s\nGAME OVER!!!\nYou lose...\n" , print_b);
				send(connfd[a] , send_msg , sizeof(send_msg) , 0);

				break;
			}
			if(!b_isEmpty(board)){
				memset(send_msg , '\0' , MAX_SIZE);
				sprintf(send_msg , "%s\nGAME OVER!!!\nTie!\n" , print_b);
				send(connfd[b] , send_msg , sizeof(send_msg) , 0);
				memset(send_msg , '\0' , MAX_SIZE);
				sprintf(send_msg , "%s\nGAME OVER!!!\nTie!\n" , print_b);
				send(connfd[a] , send_msg , sizeof(send_msg) , 0);

				break;
			}

			send(connfd[b] , print_b , sizeof(print_b) , 0);
			send(connfd[a] , print_b , sizeof(print_b) , 0);
			turn = 0;
		}
		else{
			send(connfd[a] , choose , sizeof(choose) , 0);
			ret = recv(connfd[a] , recv_msg , MAX_SIZE , 0);
			int place;
			sscanf(recv_msg , "%d" , &place);

			if(board[place - 1] != ' '){
				char msg[] = "You can't place here\nChoose again\n> ";
				send(connfd[a] , msg , sizeof(msg) , 0);
				continue;
			}

			board[place - 1] = 'X';
			printBoard(board , print_b);

			if(isWin(board)){
				memset(send_msg , '\0' , MAX_SIZE);
				sprintf(send_msg , "%s\nGAME OVER!!!\nYou win!!!\n" , print_b);
				send(connfd[a] , send_msg , sizeof(send_msg) , 0);
				memset(send_msg , '\0' , MAX_SIZE);
				sprintf(send_msg , "%s\nGAME OVER!!!\nYou lose...\n" , print_b);
				send(connfd[b] , send_msg , sizeof(send_msg) , 0);

				break;
			}
			if(!b_isEmpty(board)){
				memset(send_msg , '\0' , MAX_SIZE);
				sprintf(send_msg , "%s\nGAME OVER!!!\nTie!\n" , print_b);
				send(connfd[b] , send_msg , sizeof(send_msg) , 0);
				memset(send_msg , '\0' , MAX_SIZE);
				sprintf(send_msg , "%s\nGAME OVER!!!\nTie!\n" , print_b);
				send(connfd[a] , send_msg , sizeof(send_msg) , 0);

				break;
			}

			send(connfd[b] , print_b , sizeof(print_b) , 0);
			send(connfd[a] , print_b , sizeof(print_b) , 0);
			turn = 1;
		}
	}

	send(connfd[a] , "> " , 2 , 0);
	send(connfd[b] , "> " , 2 , 0);
}

void serverHandle(int index){
	serverLogin(index);

	serverInstr(index);

	pthread_exit(NULL);
}

int loginCheck(char* acc , char* pwd){
	FILE *f = fopen("user.txt" , "r");
	char user_tmp[MAX_SIZE] , pwd_tmp[MAX_SIZE];

	if(f == NULL){
		printf("no user file\n");
		exit(-1);
	}

	while(feof(f) == 0){
		fscanf(f , "%s%s" , user_tmp , pwd_tmp);
		if((strncmp(acc , user_tmp , sizeof(acc)) == 0) && (strncmp(pwd , pwd_tmp , sizeof(pwd)) == 0)){
			return 1;
		}
	}

	return 0;
}

void serverShutdown(int connfd){
	char input[100];

	while(1){
		scanf("%s" , input);

		if(strncmp(input , "/q" , 2) == 0){
			close(connfd);
			exit(0);
		}
	}
}
