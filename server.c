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
#include "mjson.h"

#define PORT 8221
#define IPADDR "127.0.0.1"
#define SIZE 1024

/* GLOBAL VARIABLES TO STORE CONFIG CONTENTS */
char servIP[40];
int sourcePortUDP, destPortUDP, destPortTCPHead, destPortTCPTail,
    UDPPayload, measureTime, numUDPPack, ttlUdp;


/* 1) CREATING TCP CONNECTION WITH CLIENT TO RECIEVE CONFIG FILE CONTENTS */
void tcpConnection(int tcpPort){

	/* Creates the socket */
	int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
	if(socket_desc < 0){
		perror("Error creating socket\n");
		exit(EXIT_FAILURE);
	}
	printf("[+]Socket created successfully\n");

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
	
	if(accept(socket_desc, (struct sockaddr*)&client_addr, &client_size) < 0){
		perror("Could not connect with client\n");
		exit(EXIT_FAILURE);
	}
	printf("[+]Client connected. IP: %s \t port: %i\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
	
	char buffer[SIZE];
	
	socklen_t len = 0;
	int n = recvfrom(client_sock, buffer, sizeof(buffer), MSG_WAITALL, (struct sockaddr *) &client_addr, &client_size); 
	    
	printf("%s test", buf);
	printf("\n");
	
	printf("[+]finished\n");
	close(client_sock);
	close(socket_desc);
		
}


double* udpConnection(char* serverIP, int port, int numPackets, int udpPayload){

	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sockfd < 0){
		printf("Error creating socket\n");
		exit(EXIT_FAILURE);
	}
	printf("[+]Socket created successfully\n");

	// Sets the IP header to DF
	int val = 1;
	int sockopt = setsockopt(sockfd, IPPROTO_IP, IPV6_DONTFRAG, &val, sizeof(val));
	if(sockopt < 0){
		printf("Error setsockopt\n");
		exit(EXIT_FAILURE);
	}

	/* For Linux if error occurs:
		int val = IP_PMTUDISC_DO;
	int sockopt = setsockopt(sock_desc, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val));
	if(sockopt < 0){
		printf("Error setsockopt\n");
		exit(EXIT_FAILURE);
	}
	*/
	
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
	int n = 0; // iteration counter
	int len = sizeof(client_addr);
	static double records[2]; // double array that stores the records of the times from the two packet trains

	/* intialized clock structs to record time */
	clock_t start_t, end_t;
	double lowEntropyTrain, highEntropyTrain; // stores the difference in times between first and last packet in train

	// Iterates to recieve all the packets from the first/low UDP packet train
	while(n < numPackets){
		start_t = clock();
		if(n == 0){
			if(recvfrom(sockfd, buffer, sizeof(buffer), MSG_WAITALL, (struct sockaddr *) &client_addr, &len) < 0){
				perror("Error recieving first LOW ENTROPY UDP packet: recvfrom()\n");
				exit(EXIT_FAILURE);
			}else{
				printf("[+] Successfully Recieved FIRST LOW ENTROPY packet\n");
				n++;
				continue;
			}
		}
		if(n == (numPackets-1)){
			if(recvfrom(sockfd, buffer, sizeof(buffer), MSG_WAITALL, (struct sockaddr *) &client_addr, &len) < 0){
				perror("Error recieving last LOW ENTROPY UDP packet: recvfrom()\n");
				exit(EXIT_FAILURE);
				
			}else{
				end_t = clock();
				lowEntropyTrain = (double)((end_t - start_t) * 1000 / CLOCKS_PER_SEC); // gets milliseconds
				printf("[+] Successfully Recieved LAST LOW ENTROPY packet\n");
				break;
			}
		}

		if(recvfrom(sockfd, buffer, sizeof(buffer), MSG_WAITALL, (struct sockaddr *) &client_addr, &len) < 0){
			perror("Error recieving UDP packet: recvfrom()\n");
			exit(EXIT_FAILURE);
		}else{
			n++;
		}	
	}

	records[0] = lowEntropyTrain;
	n = 0; // resets the iteration counter to 0 for the next packet train

	// Iterates to recieve all udp packets from second/high UDP packet train
	while(n < numPackets){
		start_t = clock();
		if(n == 0){
				if(recvfrom(sockfd, buffer, sizeof(buffer), MSG_WAITALL, (struct sockaddr *) &client_addr, &len) < 0){
					perror("Error recieving first HIGH ENTROPY UDP packet: recvfrom()\n");
					exit(EXIT_FAILURE);
				}else{
					printf("[+] Successfully Recieved FIRST HIGH ENTROPY packet\n");
					n++;
					continue;
				}
			}
			if(n == (numPackets-1)){
				if(recvfrom(sockfd, buffer, sizeof(buffer), MSG_WAITALL, (struct sockaddr *) &client_addr, &len) < 0){
					perror("Error recieving last HIGH ENTROPY UDP packet: recvfrom()\n");
					exit(EXIT_FAILURE);
					
				}else{
					end_t = clock();
					highEntropyTrain = (double)((end_t - start_t) * 1000 / CLOCKS_PER_SEC); // gets milliseconds
					printf("[+] Successfully Recieved LAST HIGH ENTROPY packet\n");
					break;
				}
			}
	
			if(recvfrom(sockfd, buffer, sizeof(buffer), MSG_WAITALL, (struct sockaddr *) &client_addr, &len) < 0){
				perror("Error recieving UDP packet: recvfrom()\n");
				exit(EXIT_FAILURE);
			}else{
				n++;
			}	
		}


	records[1] = highEntropyTrain;

	return records;
	
}


/* Second TCP connection takes in the two records found from the UDP connection,
calculates to see if compression is detected, then sends its findings to the client
*/



int main(int argc, char* argv[]){

	if(argc != 2){
		perror("Configuration file missing\n");
		exit(EXIT_FAILURE);
	}

	/* PARSING JSON FILE to get SOURCE PORT */
    int TCPPort;
    const struct json_attr_t json_attrs[] = {      
        {"TCPPort", t_integer, .addr.integer = &TCPPort},   
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
	

	tcpConnection(TCPPort);


	return 0;
}
