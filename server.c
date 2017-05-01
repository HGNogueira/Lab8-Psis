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
#include <pthread.h>
#include "messages.h"

#define S_PORT 3001


struct pthread_node{
	pthread_t thread_id;
	int scl;
	struct pthread_node *next;
};

int run = 1, s_gw; //s_gw socket que comunica com gateway (partilhada entre threads)
struct sockaddr_in gw_addr;
message_gw gw_msg;

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

void *c_interact(void *thread_scl){
	int scl = (int) *((int *) thread_scl);
	int err;
	message smsg;
	message rmsg;

	while(1){
		if( (err = recv(scl, &rmsg, sizeof(message), 0)) == -1){
			perror("recv error");
			pthread_exit(NULL);
		}
		else if(err == 0){ //client disconnected
			printf("Client disconnected from this server\n");
			gw_msg.type = 3; //decrementar numero de clientes
			if( (sendto(s_gw, &gw_msg, sizeof(gw_msg), 0,(const struct sockaddr *) &gw_addr, sizeof(gw_addr)) )==-1){
				perror("GW contact");
			}
			close(scl);
			/* tell the gateway that we have lost 1 client */
			return(NULL);
		}
		printf("New receive\n");
		strcpy( smsg.buffer, rmsg.buffer);
		convert_toupper(smsg.buffer);
		printf("Turned %s into %s\n", rmsg.buffer, smsg.buffer);
		send(scl, &smsg, sizeof(smsg), 0);
	}
}

int main(){
	int s, scl, err;
	FILE *f;
	struct sockaddr_in srv_addr;
	struct sockaddr_in clt_addr;
	socklen_t clt_addr_len;
	char fread_buff[50];
	int port;
	struct sigaction act_INT, act_SOCK;
	struct pthread_node *thread_list, *thread_head;  //lista de threads

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
	srv_addr.sin_port = htons(S_PORT + getpid());
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
	gw_msg.port = S_PORT + getpid();
	printf("My port is %d\n", gw_msg.port);
	
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
	thread_list = (struct pthread_node *) malloc(sizeof(struct pthread_node));
	thread_head = thread_list;
	while(run){
		if( (thread_list->scl = accept(s, (struct sockaddr *) &clt_addr, &clt_addr_len)) != -1){
			gw_msg.type = 2; //incremento no número de clientes
			if( (sendto(s_gw, &gw_msg, sizeof(gw_msg), 0,(const struct sockaddr *) &gw_addr, sizeof(gw_addr)) )==-1){
				perror("GW contact");
			}
			if( pthread_create(&(thread_list->thread_id) , NULL, c_interact, &(thread_list->scl)) != 0)
				printf("Error creating a new thread\n");

			thread_list->next = (struct pthread_node *) malloc(sizeof(struct pthread_node));
			thread_list = thread_list->next;
		} else{
			perror("accept");
			//exit(EXIT_FAILURE):  //não sair para não interromper restantes threads?

		}
	}
	
	/* close thread_list */
	
	close(s);
	exit(EXIT_SUCCESS);
}
