#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>

#include "storyserver.h"

int run = 1;

void sigint_handler(int n){
	run = 0;
}

void convert_toupper(char *string){
	int i = 0;
	while(string[i] != '\0'){
		string[i] = toupper(string[i]);
		i++;
	}
}
	

int main(){
	int s, scl, s_gw, err;
	FILE *f;
	struct sockaddr_in srv_addr;
	struct sockaddr_in clt_addr;
	struct sockaddr_in gw_addr;
	socklen_t clt_addr_len;
	message smsg;
	message rmsg;
	message_gw gw_msg;
	char fread_buff[50];
	int port;
	struct sigaction act_INT, act_SOCK;

/****** SIGNAL MANAGEMENT ******/
	act_INT.sa_handler = sigint_handler;
	sigemptyset(&act_INT.sa_mask);
	act_INT.sa_flags=0;
	sigaction(SIGINT, &act_INT, NULL);

	act_SOCK.sa_handler = sigint_handler;
	sigemptyset(&act_SOCK.sa_mask);
	act_SOCK.sa_flags=0;
	sigaction(SIGPIPE, &act_SOCK, NULL); //Quando cliente fecha scl, podemos controlar comportamento do servidor
/****** SIGNAL MANAGEMENT ******/

/****** PREPARE SOCK_STREAM ******/
	if(  (s = socket(AF_INET, SOCK_STREAM, 0))==-1 ){
		perror("socket");
		exit(EXIT_FAILURE);
	}
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_port = htons(SRV1_PORT + getpid());
	srv_addr.sin_addr.s_addr = INADDR_ANY;
	
	if(  (err = bind(s, (const struct sockaddr *) &srv_addr, sizeof(struct sockaddr_in)))== -1){
		perror("bind");
		exit(EXIT_FAILURE);
	}
	
	if( (err = listen(s, 10)) == -1){
		perror("listen");
		exit(EXIT_FAILURE);
	}
	printf("Ready to accept\n");
/****** SOCK_STREAM PREPARED *******/
	
/****** CONTACT GATEWAY ******/
	gw_addr.sin_family = AF_INET;

	if( (f = fopen("./ifconfig.txt", "r"))== NULL){
		perror("fopen");
		exit(EXIT_FAILURE);
	}
	fgets(fread_buff, 50, f);//lê porto
	sscanf(fread_buff, "%d", &port);
	printf("Gateway port: %d\n", port);
	gw_addr.sin_port = htons(port);

	fgets(fread_buff, 50, f);//lê endereço
	printf("Gateway address: %s\n", fread_buff);
	inet_aton(fread_buff, &gw_addr.sin_addr);
	fclose(f);
	
	gw_msg.type = 1;
	gw_msg.port = SRV1_PORT + getpid();
	
	if( (s_gw=socket(AF_INET, SOCK_DGRAM, 0)) == -1){
		perror("socket error");
		exit(EXIT_FAILURE);
	}
	if( (sendto(s_gw, &gw_msg, sizeof(gw_msg), 0,(const struct sockaddr *) &gw_addr, sizeof(gw_addr)) )==-1){
		perror("GW contact");
		exit(EXIT_FAILURE);
	}
/****** GATEWAY CONTACT ESTABLISHED *****/


/****** READY TO RECEIVE MULTIPLE CONNECTIONS ******/
	clt_addr_len = sizeof(struct sockaddr_in);
	if( (scl = accept(s, (struct sockaddr *) &clt_addr, &clt_addr_len)) == -1){
		perror("accept");
		exit(EXIT_FAILURE);
	}
	while(run){
		while(run){ // para com SIGINT ou SIGPIPE (falha na socket)
			if( recv(scl, &rmsg, sizeof(message), 0) == -1){
				perror("recv error");
				close(s);
				exit(EXIT_FAILURE);
			}
			if(rmsg.buffer[0]== 'A'){//condição de fim de interação (por mudar)
				printf("Ending interaction\n");
				close(scl);
				close(s);
				exit(EXIT_SUCCESS);
			}
			printf("New receive from %s\n",inet_ntoa(clt_addr.sin_addr));
			strcpy( smsg.buffer, rmsg.buffer);
			convert_toupper(smsg.buffer);
			printf("Turned %s into %s\n", rmsg.buffer, smsg.buffer);
			send(scl, &smsg, sizeof(smsg), 0);
			}
	}
	
	printf("Closing socket\n");
	close(s);
	close(scl);

	exit(EXIT_SUCCESS);
}
