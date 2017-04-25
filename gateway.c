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

struct node{
	int port;
	char address[50];
	struct node *next;
};

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
	int s, err;
	FILE *f;
	struct sockaddr_in gw_addr;
	struct sockaddr_in rmt_addr;
	char address[20];
	socklen_t addr_len;
	message       msg;
	message_gw gw_msg;
	char fread_buff[50], client_send_buffer[50];
	int port, available;
	struct sigaction act_INT, act_SOCK;
	struct node *servers, *servers_head, *servers_start;

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

/****** INITIALIZE SOCKET ******/
	gw_addr.sin_family = AF_INET;

	if( (f = fopen("./ifconfig.txt", "r"))== NULL){
		perror("fopen");
		exit(EXIT_FAILURE);
	}
	fgets(fread_buff, 50, f);//lê porto
	sscanf(fread_buff, "%d", &port);
	printf("Gateway port: %d\n", port);
	gw_addr.sin_port = htons(port);
	gw_addr.sin_addr.s_addr = INADDR_ANY;
	fclose(f);
	
	if( (s=socket(AF_INET, SOCK_DGRAM, 0)) == -1){
		perror("socket error");
		exit(EXIT_FAILURE);
	}
	
	if( (bind(s, (const struct sockaddr *) &gw_addr, sizeof(gw_addr))) == -1){
		perror("bind error");
		exit(EXIT_FAILURE);
	}

/****** DGRAM SOCKET INITIALIZED ******/
	
	servers = (struct node*) malloc(sizeof(struct node));
	servers_head = servers;
	servers_start = servers;
	servers->port = 0;

/****** READY TO TALK TO CLIENT AND SERVER ******/
	while(run){
		addr_len = sizeof(rmt_addr);
		recvfrom(s, &gw_msg, sizeof(gw_msg), 0, (struct sockaddr *) &rmt_addr, &addr_len);
		if(gw_msg.type == 1){
			servers->port = gw_msg.port;
		        strcpy(servers->address, inet_ntoa(rmt_addr.sin_addr) );
			servers->next = (struct node *) malloc(sizeof(struct node));
			printf("New server available - addr=%s, port =%d\n", servers->address, servers->port);
			servers = servers->next;
			servers->port = 0;
		}
		else{
			printf("Contacted by client: %s\n", inet_ntoa(rmt_addr.sin_addr));
			if(servers_head->port){
				printf("Server available with port %d\n", servers_head->port);
				gw_msg.type = 1; //notify server is available
				strcpy(gw_msg.address, servers_head->address);
				gw_msg.port = servers_head->port;
				servers_head = servers_head->next;
				sendto(s, &gw_msg, sizeof(gw_msg), 0, (const struct sockaddr*) &rmt_addr, sizeof(rmt_addr));
			}
			else{
				printf("No servers available\n");
				gw_msg.type = 0; // notify no server available
				sendto(s, NULL, 0, 0, (const struct sockaddr*) &rmt_addr, sizeof(rmt_addr));
			}

		}
	}
	
	printf("Closing socket\n");
	close(s);

	exit(EXIT_SUCCESS);
}
