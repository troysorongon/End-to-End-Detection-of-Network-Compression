#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <stdbool.h>

#include "mjson.h"

#define PORT 8221
#define IPADDR "127.0.0.1"
#define SIZE 1024
#define TEST 1

/* GLOBAL VARIABLES TO STORE CONFIG CONTENTS */
char servIP[40];
int sourcePortUDP, destPortUDP, destPortTCPHead, destPortTCPTail,
    tcpPort, UDPPayload, measureTime, numUDPPack, ttlUdp;
static double times[2];

/* Helper function that parses the contents in the buffer */
void parser(char* buffer){
	const struct json_attr_t json_attrs[] = {
        {"servIP", t_string, .addr.string = servIP,
			.len = sizeof(servIP)},
        {"sourcePortUDP", t_integer, .addr.integer = &sourcePortUDP},
        {"destPortUDP", t_integer, .addr.integer = &destPortUDP},
        {"destPortTCPHead", t_integer, .addr.integer = &destPortTCPHead},
        {"destPortTCPTail", t_integer, .addr.integer = &destPortTCPTail},
        {"TCPPort", t_integer, .addr.integer = &tcpPort},
        {"UDPPayload", t_integer, .addr.integer = &UDPPayload},
        {"measureTime", t_integer, .addr.integer = &measureTime},
        {"numUDPPack", t_integer, .addr.integer = &numUDPPack},
        {"ttlUdp", t_integer, .addr.integer = &ttlUdp},
        {NULL},
    };
	
	if(json_read_object(buffer, json_attrs, NULL) < 0){
		perror("Error getting from JSON file.\n");
		exit(EXIT_FAILURE);
    }

    if(UDPPayload <= 0 || UDPPayload > 65527){
    	UDPPayload = 1000;
    }

    if(measureTime <= 0){
    	measureTime = 5;
    }

    if(numUDPPack <= 0){
    	numUDPPack = 6000;
    }

    if(ttlUdp <= 0){
    	ttlUdp = 255;
    }

    printf("\tServer IP: %s\n", servIP);
    printf("\tUDP Source Port: %d\n", sourcePortUDP);
    printf("\tUDP Destination Port: %d\n", destPortUDP);
    printf("\tDestination Port TCP Head: %d\n", destPortTCPHead);
    printf("\tDestination Port TCP Tail: %d\n", destPortTCPTail);
    printf("\tTCP Port: %d\n", tcpPort);
    printf("\tUDP Payload: %d\n", UDPPayload);
    printf("\tInter-Measurement Time: %d\n", measureTime);
    printf("\tUDP Packets: %d\n", numUDPPack);
    printf("\tTime-to-Live: %d\n", ttlUdp);

    printf("[+]Successfully parsed message from client\n");
}

/* 1) CREATING TCP CONNECTION WITH CLIENT TO RECIEVE CONFIG FILE CONTENTS */
void tcpConnection(char* servIP, int tcpPort){

	/* Creates the socket */
	int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
	if(socket_desc < 0){
		perror("Error creating socket\n");
		exit(EXIT_FAILURE);
	}
	printf("[+]Socket created successfully\n");

	int val = 1;
	int setsock = setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, (void*)&val, sizeof(val));
	if(setsock < 0){
		perror("setsockopt error\n");
		exit(EXIT_FAILURE);
	}

	/* Creates the server address struct */
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(tcpPort);
	server_addr.sin_addr.s_addr = inet_addr(servIP);
	
	if(bind(socket_desc, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
		perror("Server could not bind to port\n");
		exit(EXIT_FAILURE);
	}
	
	printf("[+]Server successfully binded to port\n");
	
	if(listen(socket_desc, 1) < 0){
		printf("Error while listening for client\n");
		exit(EXIT_FAILURE);
	}
	printf("[+]Listening for client\n");
		
	struct sockaddr_in client_addr;
	int client_size = sizeof(client_addr);
	int client_sock;
	
	client_sock = accept(socket_desc, (struct sockaddr*)&client_addr, &client_size);
	if(client_sock < 0){
		perror("Could not connect with client\n");
		exit(EXIT_FAILURE);
	}
	printf("[+]Client connected. IP: %s \t port: %i\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
	printf("\n");
	
	char buffer[1024];
	
	if(recv(client_sock, buffer, sizeof(buffer), 0) < 0){ //(struct sockaddr *) &client_addr, &client_size) < 0){
		perror("Error recieving content from client\n");
		exit(EXIT_FAILURE);
	}
	    
	printf("Info recieved: \n");
	
	parser(buffer);
	
	printf("[+]Closing connection with Client\n");
	close(client_sock);
	close(socket_desc);
}


void udpConnection(char* serverIP, int port, int udpPayload){

	int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sockfd < 0){
		printf("Error creating socket\n");
		exit(EXIT_FAILURE);
	}
	printf("[+]Socket created successfully\n");

	struct sockaddr_in server_addr, client_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	memset(&server_addr, 0, sizeof(client_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = inet_addr(serverIP);
	
	if(bind(sockfd, (const struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
		printf("Server could not bind to port\n");
		exit(EXIT_FAILURE);
	}
	printf("[+]Server successfully binded to port\n");

	char buffer[udpPayload];
	int numRecieved = 0;
	int len = sizeof(client_addr);
	

	/* intialized clock structs to record time */
	clock_t start, end;

	/*  Recieves all packets from the low and high entropy packet train. Breaks loop once last packet of high entropy 
	packet train is recieved */
	start = clock();
	while(true){
		if(recvfrom(sockfd, buffer, sizeof(buffer), MSG_WAITALL, (struct sockaddr *) &client_addr, &len) < 0){
				perror("Error recieving UDP packet: recvfrom()\n");
				exit(EXIT_FAILURE);
		}

		if(strstr(buffer, "done")){
			end = clock();
			break;
		}
		
		numRecieved++;
	}

	times[0] = (double)((end - start) * 1000 / CLOCKS_PER_SEC);
	printf("Low Entropy Train --> Number of packets Recieved: %d\n", numRecieved);

	numRecieved = 0;

	start = clock();
	while(true){
		if(recvfrom(sockfd, buffer, sizeof(buffer), MSG_WAITALL, (struct sockaddr *) &client_addr, &len) < 0){
			perror("Error recieving UDP packet: recvfrom()\n");
			exit(EXIT_FAILURE);
		}
	
		if(strstr(buffer, "done")){
			end = clock();
			break;
		}
			
		
		numRecieved++;
	}

	times[1] = (double)((end - start) * 1000 / CLOCKS_PER_SEC);
	printf("High Entropy Train --> Number of packets Recieved: %d\n", numRecieved);			
	
	close(sockfd);
	
}


/* Second TCP connection takes in the two records found from the UDP connection,
calculates to see if compression is detected, then sends its findings to the client
*/


void postProbing(int tcpPort, double *times){

	/* Calculating Compression Detection */
	bool lowCompressed = true;
	bool highCompressed = true;
	double lowEntropyTime = *times + 0;
	double highEntropyTime = *times + 1;

	printf("lowEntropyTime: %f\n highEntropyTime: %f\n", lowEntropyTime, highEntropyTime);

	/* Checks the time to see if they are under the threshold of 100 ms */
	if(lowEntropyTime < 100){
		lowCompressed = false;
	}

	if(highEntropyTime < 100){
		highCompressed = false;
	}	

	/* Writes the message of compression detection of both packet trains to send to client */
	char compressionMessage[SIZE];
	if(lowCompressed){
		strcpy(compressionMessage, "\tLow Entropy Train: Compression detected!\n");
	}
	else{
		strcpy(compressionMessage, "\tLow Entropy Train: No compression was detected!\n");
	}

	if(highCompressed){
		strcat(compressionMessage, "\tHigh Entropy Train: Compression detected!\n");
	}
	else{
		strcat(compressionMessage, "\tHigh Entropy Train: No compression was detected!\n");
	}
	
	int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
	if(socket_desc < 0){
		perror("Error creating socket\n");
		exit(EXIT_FAILURE);
	}
	printf("[+]Socket created successfully\n");


	int val = 1;
	int setsock = setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, (void*)&val, sizeof(val));
	if(setsock < 0){
		perror("setsockopt error\n");
		exit(EXIT_FAILURE);
	}

	
	/* Creates the server address struct */
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(tcpPort);
	server_addr.sin_addr.s_addr = inet_addr(IPADDR);
		
	if(bind(socket_desc, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
		perror("Server could not bind to port\n");
		exit(EXIT_FAILURE);
	}
		
	printf("[+]Server successfully binded to port\n");
		
	if(listen(socket_desc, 1) < 0){
		printf("Error while listening for client\n");
		exit(EXIT_FAILURE);
	}
	printf("[+]Listening for client\n");
			
	struct sockaddr_in client_addr;
	int client_size = sizeof(client_addr);
	int client_sock;
		
	client_sock = accept(socket_desc, (struct sockaddr*)&client_addr, &client_size);
	if(client_sock < 0){
		perror("Could not connect with client\n");
		exit(EXIT_FAILURE);
	}

	printf("[+]Client connected. IP: %s \t port: %i\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
	printf("\n");

	if(send(client_sock, compressionMessage, strlen(compressionMessage), 0) < 0){
		perror("Error sending compression message to client\n");
		exit(EXIT_FAILURE);
	}
	printf("[+]Sent Compression findings to client\n");

	printf("[+]Closing connection with Client\n");
	close(client_sock);
	close(socket_desc);
	
}


int main(int argc, char* argv[]){

	if(argc != 2){
		perror("Configuration file missing\n");
		exit(EXIT_FAILURE);
	}

	/* PARSING JSON FILE to get SOURCE PORT */
   // int TCPPort;
    const struct json_attr_t json_attrs[] = {      
		{"servIP", t_string, .addr.string = servIP,
			.len = sizeof(servIP)},
		{"sourcePortUDP", t_integer, .addr.integer = &sourcePortUDP},
		{"destPortUDP", t_integer, .addr.integer = &destPortUDP},
		{"destPortTCPHead", t_integer, .addr.integer = &destPortTCPHead},
		{"destPortTCPTail", t_integer, .addr.integer = &destPortTCPTail},
		{"TCPPort", t_integer, .addr.integer = &tcpPort},
		{"UDPPayload", t_integer, .addr.integer = &UDPPayload},
		{"measureTime", t_integer, .addr.integer = &measureTime},
		{"numUDPPack", t_integer, .addr.integer = &numUDPPack},
		{"ttlUdp", t_integer, .addr.integer = &ttlUdp}, 
        {NULL},
    };

	char buffer[SIZE];
	int length;
	FILE *fp;
	char *filename = argv[1];
	fp = fopen(filename, "r");
	
	if(fp != NULL){
		fseek(fp, 0, SEEK_END);
		length = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		fread(buffer, 1, length, fp);
		fclose(fp);
	}
	
	if(json_read_object(buffer, json_attrs, NULL) < 0){
		perror("Error getting from JSON file.\n");
		exit(EXIT_FAILURE);
	}


	printf("Pre-probing Phase:\n");
	printf("TCPport: %d\n", tcpPort);
	tcpConnection(servIP, tcpPort);
	printf("\n");

	printf("Probing Phase:\n");
	udpConnection(servIP, destPortUDP, UDPPayload);
	printf("\n");
	
	printf("Post-probing Phase:\n");
	postProbing(tcpPort, times);
	
	return 0;
}

