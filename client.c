#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "mjson.h"

#define PORT 8221
#define IPADDR "127.0.0.1"
#define SIZE 1024


void tcpConnection(char *serverIP, int tcpPort, char* fileContents){

	int sock_desc = socket(AF_INET, SOCK_STREAM, 0);
	if(sock_desc < 0){
		printf("Unable to create socket\n");
		exit(EXIT_FAILURE);
	}

	printf("[+]Socket created successfully\n");

	// setting port and IP
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(tcpPort);
	server_addr.sin_addr.s_addr = inet_addr(serverIP);

	if(connect(sock_desc, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0){
		printf("Client unable to connect to server\n");
		exit(1);
	}
	printf("[+]Connected to Server\n");

    /* Sending the contents of the JSON file to the server */
    if(sendto(sock_desc, fileContents, strlen(fileContents), '\0', (struct sockaddr *)
     &server_addr, sizeof(server_addr)) < 0){
     	perror("File not sent\n");
     	exit(EXIT_FAILURE);
     }
    else{
    	printf("[+]JSON content successfully sent\n"); 	
     }
	
	printf("[+]Closing the connection\n");
	close(sock_desc);
}


void udpConnection(char *serverIP, int sourcePort, int destinationPort, int udpPayload,
int interMeasureTime, int numPackets) {

	int sock_desc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); // may need to change to "IP_PROTOUDP"
	if(sock_desc < 0){
		printf("Unable to create socket\n");
		exit(EXIT_FAILURE);
	}

	/* Sets the IP header to DF
	int val = 1;
	int sockopt = setsockopt(sock_desc, IPPROTO_IP, IPV6_DONTFRAG, &val, sizeof(val));
	if(sockopt < 0){
		printf("Error setsockopt\n");
		exit(EXIT_FAILURE);
	}
	*/

	// For Linux if error occurs:
	int val = IP_PMTUDISC_DO;
	int sockopt = setsockopt(sock_desc, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val));
	if(sockopt < 0){
		printf("Error setsockopt\n");
		exit(EXIT_FAILURE);
	}
	
	

	// setting source and destination port and IP for client and server
	struct sockaddr_in client_addr, server_addr;

	memset(&client_addr, 0, sizeof(client_addr));
	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(sourcePort);
	client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(destinationPort);
	server_addr.sin_addr.s_addr = inet_addr(serverIP);


	/* use bind() if it error occurs
	if(bind(sock_desc, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0){
		perror("client could not bind\n");
		exit(EXIT_FAILURE);
	}
	*/

	// You store all the information into the buffer (packet) like ip header, udp header, packet id, and payload
	// it is up to the server to parse that information on their end

/* LOW ENTROPY PACKET CREATION AND SEND */

	unsigned char lowEntropy[udpPayload]; // default is 2000
	memset(lowEntropy, 0, sizeof(lowEntropy)); // sets the sequence of bytes to 0

// whatever i is, make that into the packet id using bit shifting to make it into 2 bytes (16 bits)
// unsigned short int is a postitive integer that is stored as 16 bits/2 bytes

	for(unsigned short int id = 0; id < numPackets; id++){
		// send
		if (id == 0){
			memset(lowEntropy, id, sizeof(id)); // sets the beginning of the payload (first 16 bits/2 bytes) to the packet ID
			if(sendto(sock_desc, lowEntropy, sizeof(lowEntropy), 0, (struct sockaddr *)&server_addr, (socklen_t)sizeof(server_addr)) < 0){
				perror("First UDP packet send error\n");
				exit(EXIT_FAILURE);
			}
			else{
				printf("[+]Successfully sent the first packet\n");
				continue;
			}		
		}
		if (id == (numPackets - 1)){
			memset(lowEntropy, id, sizeof(id));
			if(sendto(sock_desc, lowEntropy, sizeof(lowEntropy), 0, (struct sockaddr *)&server_addr, (socklen_t)sizeof(server_addr)) < 0){
				perror("Last UDP packet send error\n");
				exit(EXIT_FAILURE);
			}
			else{
				printf("[+]Successfully sent the last packet\n");
				continue;
			}
		}

		memset(lowEntropy, id, sizeof(id));
		if(sendto(sock_desc, lowEntropy, sizeof(lowEntropy), 0, (struct sockaddr *)&server_addr, (socklen_t)sizeof(server_addr)) < 0){
			perror("UDP packet send error\n");
			exit(EXIT_FAILURE);
		}
	}

	// Will wait the Inter Measure Time before sending the High Entropy Packet Train
	sleep(interMeasureTime);


/* HIGH ENTROPY PACKET CREATION AND SEND */

    unsigned char highEntropy[udpPayload];
    FILE *fp;
   	fp = fopen("/dev/urandom", "r");
   	// reads from /dev/random and writes the random sequence of bytes to highEntropy
    fread(&highEntropy, 1, udpPayload, fp); // reads from /dev/random and writes the random seq
    fclose(fp);
	
	for(unsigned short int id = 0; id < numPackets; id++){
			// send
			if (id == 0){
				memset(highEntropy, id, sizeof(id)); // sets the beginning of the payload (first 16 bits/2 bytes) to the packet ID
				if(sendto(sock_desc, highEntropy, sizeof(highEntropy), 0, (struct sockaddr *)&server_addr, (socklen_t)sizeof(server_addr)) < 0){
					perror("First UDP packet send error\n");
					exit(EXIT_FAILURE);
				}
				else{
					printf("[+]Successfully sent the first packet\n");
					continue;
				}		
			}
			if (id == (numPackets - 1)){
				memset(highEntropy, id, sizeof(id));
				if(sendto(sock_desc, highEntropy, sizeof(highEntropy), 0, (struct sockaddr *)&server_addr, (socklen_t)sizeof(server_addr)) < 0){
					perror("Last UDP packet send error\n");
					exit(EXIT_FAILURE);
				}
				else{
					printf("[+]Successfully sent the last packet\n");
					continue;
				}
			}
	
			memset(highEntropy, id, sizeof(id));
			if(sendto(sock_desc, highEntropy, sizeof(highEntropy), 0, (struct sockaddr *)&server_addr, (socklen_t)sizeof(server_addr)) < 0){
				perror("UDP packet send error\n");
				exit(EXIT_FAILURE);
		}
	
	}

	close(sock_desc);
	printf("[+]Successfully sent both packet trains\n");
}

int main(int argc, char* argv[]){

	// Checks if there is a config file attached 
	if(argc != 2){
		perror("Configuration file missing\n");
		exit(EXIT_FAILURE);
	}

	/* Parsing JSON file --> setting up configuration*/
    char servIP[40];
    int sourcePortUDP, destPortUDP, destPortTCPHead, destPortTCPTail,
    TCPPort, UDPPayload, measureTime, numUDPPack, ttlUdp;

    const struct json_attr_t json_attrs[] = {
        {"servIP", t_string, .addr.string = servIP,
			.len = sizeof(servIP)},
        {"sourcePortUDP", t_integer, .addr.integer = &sourcePortUDP},
        {"destPortUDP", t_integer, .addr.integer = &destPortUDP},
        {"destPortTCPHead", t_integer, .addr.integer = &destPortTCPHead},
        {"destPortTCPTail", t_integer, .addr.integer = &destPortTCPTail},
        {"TCPPort", t_integer, .addr.integer = &TCPPort},
        {"UDPPayload", t_integer, .addr.integer = &UDPPayload},
        {"measureTime", t_integer, .addr.integer = &measureTime},
        {"numUDPPack", t_integer, .addr.integer = &numUDPPack},
        {"ttlUdp", t_integer, .addr.integer = &ttlUdp},
        {NULL},
    };
    

	/*reading from file to array with buffer*/
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

    //printf("buffer: %s\n", buffer);


	/* Setting defaults to values if not present or "0" in JSON file */
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
    
	/*testing*/
    //printf("servIP = %s\nsourcePortUDP = %d\nnumUDPPack = %d\n", servIP, sourcePortUDP, numUDPPack);
	//printf("ttl = %d\n", ttlUdp);
	
 	/* Creates TCP connection with server that takes in the server IP address 
 	and TCP Port number. Sends the contexts in the config file to the server.*/
	printf("Pre-probing Phase:\n");
	tcpConnection(servIP, TCPPort, buffer);
	printf("\n");

	printf("Probing Phase:\n");
	udpConnection(servIP, sourcePortUDP, destPortUDP, UDPPayload, measureTime, numUDPPack);

	
	return 0;
}

