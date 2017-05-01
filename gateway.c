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
#include "messages.h"

struct node{
	int port;
	char address[50];
	int nclients;
	struct node *next;
};

int run = 1;
void sigint_handler(int n){
	run = 0;
}

/* função que escolhe servidor com menos carga */
struct node *pick_server(struct node *servers){
	struct node *s1,*s2;
	int clients;

	if(!servers)//servers vazio
		return NULL;

	s1 = servers;
	s2 = servers;
	while(s1->next){
		if(s2->nclients > s1->next->nclients){
			s2 = s1->next;
		}
		s1 = s1->next;
	}

	return s2;
}
struct node *addserver(struct node *start, struct node *to_add){
	/* função que adiciona servidor à lista */
	to_add->next = start;
	return to_add;
}

struct node *search_server(struct node *servers, struct sockaddr_in *rmt_addr){
	struct node *s;
	s = servers;
	while(s->next != NULL){
		if(s->next->port == rmt_addr->sin_port)
			if(strcmp(s->address, inet_ntoa(rmt_addr->sin_addr))==0)
				return s;
	}
	return NULL;
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
	struct node *servers=NULL, *tmp_node;

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
	

/****** READY TO TALK TO CLIENT AND SERVER ******/
	while(run){
		addr_len = sizeof(rmt_addr);
		recvfrom(s, &gw_msg, sizeof(gw_msg), 0, (struct sockaddr *) &rmt_addr, &addr_len);
		if(gw_msg.type == 1){ //contacted by peer
			tmp_node = (struct node *) malloc(sizeof(struct node));
			tmp_node->port = gw_msg.port;
		        strcpy(tmp_node->address, inet_ntoa(rmt_addr.sin_addr) );
			tmp_node->nclients = 0;
			tmp_node->next = NULL;
			servers = addserver(servers, tmp_node);
			printf("New server available - addr=%s, port =%d\n", tmp_node->address, tmp_node->port);
		}
		else if(gw_msg.type == 0){ //contacted by client
			printf("Contacted by client: %s\n", inet_ntoa(rmt_addr.sin_addr));
			if( (tmp_node = pick_server(servers)) != NULL){
				printf("Server available with port %d\n", tmp_node->port);
				gw_msg.type = 1; //notify server is available
				strcpy(gw_msg.address, tmp_node->address);
				gw_msg.port = tmp_node->port;
				tmp_node->nclients = tmp_node->nclients + 1;
				sendto(s, &gw_msg, sizeof(gw_msg), 0, (const struct sockaddr*) &rmt_addr, sizeof(rmt_addr));
			}
			else{
				printf("No servers available\n");
				gw_msg.type = 0; // notify no server available
				sendto(s, NULL, 0, 0, (const struct sockaddr*) &rmt_addr, sizeof(rmt_addr));
			}

		}
		else if(gw_msg.type == 2){
			tmp_node = search_server(servers, &rmt_addr);
			tmp_node->nclients = tmp_node->nclients + 1;
			
		}
		else if(gw_msg.type == 3){
			tmp_node = search_server(servers, &rmt_addr);
			tmp_node->nclients = tmp_node->nclients - 1;
		}
	}
	
	printf("Closing socket\n");
	close(s);

	exit(EXIT_SUCCESS);
}
