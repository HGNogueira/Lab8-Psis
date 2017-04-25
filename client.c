#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <signal.h>
#include "storyserver.h"

int run = 1;

void sigint_handler(int n){
	run = 0;
}

int main(){
	int s, scl, s_gw ,err;
	struct sockaddr_in srv_addr;
	struct sockaddr_in gw_addr;
	char scanned[MESSAGE_LEN];
	FILE *f;
	char fread_buff[50];
	int port;
	struct sigaction act_INT;
	socklen_t size_addr;
	message rmsg;
	message smsg;
	message_gw gw_msg;

	/****** SIGNAL MANAGEMENT ******/
	act_INT.sa_handler = sigint_handler;
	sigemptyset(&act_INT.sa_mask);
	act_INT.sa_flags=0;
	sigaction(SIGINT, &act_INT, NULL);
	/****** SIGNAL MANAGEMENT ******/

		
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
	
	if( (s_gw=socket(AF_INET, SOCK_DGRAM, 0)) == -1){
		perror("socket error");
		exit(EXIT_FAILURE);
	}
	gw_msg.type = 0;

	if( (sendto(s_gw, &gw_msg, sizeof(gw_msg), 0,(struct sockaddr *) &gw_addr, sizeof(gw_addr)) )==-1){
		perror("GW contact");
		exit(EXIT_FAILURE);
	}
	/****** GATEWAY CONTACT ESTABLISHED *****/
	
	size_addr = sizeof(struct sockaddr_in);
	recvfrom(s_gw, &gw_msg, sizeof(gw_msg), 0, (struct sockaddr *) &gw_addr, &size_addr);
	if(gw_msg.type == 0){
		printf("No server available, try later\n");
		exit(EXIT_SUCCESS);
	}



	if(  (s = socket(AF_INET, SOCK_STREAM, 0))==-1 ){
		perror("socket");
		exit(EXIT_FAILURE);
	}

	srv_addr.sin_family = AF_INET;
	srv_addr.sin_port = htons(gw_msg.port);
	inet_aton(gw_msg.address, &srv_addr.sin_addr);

	if( (scl = connect(s, (const struct sockaddr *) &srv_addr, sizeof(struct sockaddr_in))) == -1){
		perror("accept");
		exit(EXIT_FAILURE);
	}
	
	while(run){
		fgets(scanned, MESSAGE_LEN, stdin);
		strcpy(smsg.buffer, scanned);
		send(s, (void *) &smsg, sizeof(message), 0);
		recv(s, &rmsg, sizeof(rmsg), 0);
		printf("sent %s --- received %s\n", smsg.buffer, rmsg.buffer);
	}
	close(scl);
	close(s);
	
	exit(EXIT_SUCCESS);
}
