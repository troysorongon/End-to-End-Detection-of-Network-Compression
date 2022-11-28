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

void tcpConnection(char *serverIP, int tcpPort, char* fileContents)
{
	int sock_desc = socket(AF_INET, SOCK_STREAM, 0);
	if(sock_desc < 0)
	{
		printf("Unable to create socket\n");
		exit(EXIT_FAILURE);
	}

	printf("[+]Socket created successfully\n");

	int val = 1;
	int setsock = setsockopt(sock_desc, SOL_SOCKET, SO_REUSEADDR, (void*)&val, sizeof(val));
	if(setsock < 0)
	{
		perror("setsockopt error\n");
		exit(EXIT_FAILURE);
	}

	// setting port and IP
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(tcpPort);
	server_addr.sin_addr.s_addr = inet_addr(serverIP);

	if(connect(sock_desc, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0)
	{
		printf("Client unable to connect to server\n");
		exit(1);
	}
	printf("[+]Connected to Server\n");

    /* Sending the contents of the JSON file to the server */
    if(sendto(sock_desc, fileContents, strlen(fileContents), '\0', (struct sockaddr *)
     &server_addr, sizeof(server_addr)) < 0)
    {
     	perror("File not sent\n");
     	exit(EXIT_FAILURE);
    }
    else
    {
    	printf("[+]JSON content successfully sent\n"); 	
     }
	
	printf("[+]Closing the connection\n");
	close(sock_desc);
}


void udpConnection(char *serverIP, int sourcePort, int destinationPort, int udpPayload,
int numPackets, int interMeasureTime)
{
	int sock_desc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); // may need to change to "IP_PROTOUDP"
	if(sock_desc < 0){
		printf("Unable to create socket\n");
		exit(EXIT_FAILURE);
	}

	int val = IP_PMTUDISC_DO;
	int sockopt = setsockopt(sock_desc, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val));
	if(sockopt < 0)
	{
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

	/*------- LOW ENTROPY PACKET CREATION AND SEND -------- */
	unsigned char lowEntropyData[udpPayload]; // Creating low entropy with the size of payload
	memset(lowEntropyData, 0, udpPayload);
	unsigned char lowEntropyPacket[udpPayload + 2]; // +2 to add space for the packet ID in the beginning of the packet

	char* done = "done"; // char message to let the Server know it has sent the entire packet train

	for(unsigned short int id = 0; id < numPackets; id++)
	{
		unsigned char idRight = id & 0xFF;	// Bit shift the last byte of the ID
		unsigned char idLeft = id >> 8;		// Bit shift the first byte of the ID
		
		// Stores the left in right bytes in the first two indicies of the packet
		lowEntropyPacket[0] = idLeft;	
		lowEntropyPacket[1] = idRight;

		strncpy(&lowEntropyPacket[2], lowEntropyData, udpPayload);	
		if(sendto(sock_desc, lowEntropyPacket, sizeof(lowEntropyPacket), 0, (struct sockaddr *)&server_addr,
		(socklen_t)sizeof(server_addr)) < 0)
		{
			perror("First UDP packet send error\n");
			exit(EXIT_FAILURE);
		}
		if(id == 0)
		{
			printf("[+]Successfully sent the first packet\n");
		}
		if(id == (numPackets - 1))
		{
			printf("[+]Successfully sent the last packet\n");
		}
	}

	if(sendto(sock_desc, (const char*)done, strlen(done), 0, (struct sockaddr *)&server_addr,
	(socklen_t)sizeof(server_addr)) < 0)
	{
		perror("First UDP packet send error\n");
		exit(EXIT_FAILURE);
	}
	printf("[+]Sent done message to Server\n");

	// Will wait the Inter Measure Time before sending the High Entropy Packet Train
	sleep(interMeasureTime);

	/* ------- HIGH ENTROPY PACKET CREATION AND SEND ------- */
    unsigned char highEntropyData[udpPayload];
    FILE *fp;
   	fp = fopen("/dev/urandom", "r");
    fread(&highEntropyData, 1, udpPayload, fp); // reads from /dev/random and writes the random sequence of bytes to highEntropy
    fclose(fp);

    unsigned char highEntropyPacket[udpPayload + 2];
	
	for(unsigned short int id = 0; id < numPackets; id++)
	{
		unsigned char idRight = id & 0xFF;	// Bit shift the last byte of the ID
		unsigned char idLeft = id >> 8;		// Bit shift the first byte of the ID
				
		// Stores the left in right bytes in the first two indicies of the packet
		highEntropyPacket[0] = idLeft;	
		highEntropyPacket[1] = idRight;
		
		strncpy(&highEntropyPacket[2], highEntropyData, udpPayload);	
		if(sendto(sock_desc, highEntropyPacket, sizeof(highEntropyPacket), 0, (struct sockaddr *)&server_addr,
		(socklen_t)sizeof(server_addr)) < 0)
		{
			perror("First UDP packet send error\n");
			exit(EXIT_FAILURE);
		}
				
		if(id == 0)
		{
			printf("[+]Successfully sent the first packet\n");
		}
		if(id == (numPackets - 1))
		{
			printf("[+]Successfully sent the last packet\n");
		}
	}

	if(sendto(sock_desc, (const char*)done, strlen(done), 0, (struct sockaddr *)&server_addr,
	(socklen_t)sizeof(server_addr)) < 0)
	{
			perror("First UDP packet send error\n");
			exit(EXIT_FAILURE);
	}
	printf("[+]Sent done message to Server\n");
	
	close(sock_desc);
	printf("[+]Successfully sent both packet trains\n");
}


void postProbing(char *serverIP, int tcpPort)
{
	int sock_desc = socket(AF_INET, SOCK_STREAM, 0);
	if(sock_desc < 0)
	{
		printf("Unable to create socket\n");
		exit(EXIT_FAILURE);
	}

	printf("[+]Socket created successfully\n");

	int val = 1;
	int setsock = setsockopt(sock_desc, SOL_SOCKET, SO_REUSEADDR, (void*)&val, sizeof(val));
	if(setsock < 0)
	{
		perror("setsockopt error\n");
		exit(EXIT_FAILURE);
	}


	// setting port and IP
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(tcpPort);
	server_addr.sin_addr.s_addr = inet_addr(serverIP);

	if(connect(sock_desc, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0)
	{
		printf("Client unable to connect to server\n");
		exit(1);
	}
	printf("[+]Connected to Server\n");

	char buffer[1024];
    /* Sending the contents of the JSON file to the server */
	if(recv(sock_desc, buffer, sizeof(buffer), 0) < 0)
	{
     	perror("File not sent\n");
     	exit(EXIT_FAILURE);
     }

    printf("[+]Server Findings: \n");
    printf("%s", buffer); 	

	
	printf("[+]Closing the connection\n");
	close(sock_desc);
}


int main(int argc, char* argv[])
{
	// Checks if there is a config file attached
	if(argc != 2)
	{
		perror("Configuration file missing\n");
		exit(EXIT_FAILURE);
	}

	/* Parsing JSON file --> setting up configuration*/
    char servIP[40];
    int sourcePortUDP, destPortUDP, destPortTCPHead, destPortTCPTail,
    TCPPort, UDPPayload, measureTime, numUDPPack, ttlUdp;

    const struct json_attr_t json_attrs[] =
    {
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
	char buffer[1024];
	int length;
	FILE *fp;
	char *filename = argv[1];
	fp = fopen(filename, "r");

	if(fp != NULL)
	{
		fseek(fp, 0, SEEK_END);	// Goes to the last position of the file
		length = ftell(fp);	// Gets the total number of bytes in file
		fseek(fp, 0, SEEK_SET);	//	Goes back to the beginning of the file
		fread(buffer, 1, length, fp);	
		fclose(fp);
	}

    if(json_read_object(buffer, json_attrs, NULL) < 0)
    {
		perror("Error getting from JSON file.\n");
		exit(EXIT_FAILURE);
    }

	/* Setting defaults to values if not present or "0" in JSON file */
    if(UDPPayload <= 0 || UDPPayload > 65527)
    {
    	UDPPayload = 1000;
    }
    if(measureTime <= 0)
    {
    	measureTime = 5;
    }
    if(numUDPPack <= 0)
    {
    	numUDPPack = 6000;
    }
    if(ttlUdp <= 0)
    {
    	ttlUdp = 255;
    }
	
 	/* Creates TCP connection with server that takes in the server IP address
 	and TCP Port number. Sends the contexts in the config file to the server.*/
	printf("Pre-probing Phase:\n");
	tcpConnection(servIP, TCPPort, buffer);
	printf("\n");

	printf("Probing Phase:\n");
	udpConnection(servIP, sourcePortUDP, destPortUDP, UDPPayload, numUDPPack, measureTime);
	printf("\n");

	printf("Post-Probing Phase: \n");
	postProbing(servIP, TCPPort);
	
	return 0;
}
