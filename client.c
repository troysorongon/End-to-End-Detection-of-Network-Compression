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

// Global variables to store the values from JSON file
char servIP[40];
int sourcePortUDP, destPortUDP, destPortTCPHead, destPortTCPTail,
TCPPort, UDPPayload, measureTime, numUDPPack, ttlUdp;


/* 1. Creates TCP Connection with Server to send contents in JSON file */
void tcpConnection(char *serverIP, int tcpPort, char* fileContents)
{
	// Creates the socket
	int sock_desc = socket(AF_INET, SOCK_STREAM, 0);
	if(sock_desc < 0)
	{
		printf("Unable to create socket\n");
		exit(EXIT_FAILURE);
	}
	printf("[+]Socket created successfully\n");

	// Used to allow the socket to reuse the same port when executing the program
	// multiple times
	int val = 1;
	int setsock = setsockopt(sock_desc, SOL_SOCKET, SO_REUSEADDR, (void*)&val, sizeof(val));
	if(setsock < 0)
	{
		perror("setsockopt error\n");
		exit(EXIT_FAILURE);
	}

	// Sets the port number and IP address for the server
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(tcpPort);
	server_addr.sin_addr.s_addr = inet_addr(serverIP);

	// connects to the server
	if(connect(sock_desc, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0)
	{
		printf("Client unable to connect to server\n");
		exit(1);
	}
	printf("[+]Connected to Server\n");

    // Sending the contents of the JSON file to the server
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
	close(sock_desc);	// closes socket once JSON file contents have been sent
}

/* 2. Sending Low and High Entropy Packet Trains to Server using UDP Connection */
void udpConnection(char *serverIP, int sourcePort, int destinationPort, int udpPayload,
int numPackets, int interMeasureTime)
{
	// Creates socket
	int sock_desc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sock_desc < 0){
		printf("Unable to create socket\n");
		exit(EXIT_FAILURE);
	}
	printf("[+]Socket created successfully\n");
	
	// Sets DF flag for packets in both packet trains
	int val = IP_PMTUDISC_DO;
	int sockopt = setsockopt(sock_desc, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val));
	if(sockopt < 0)
	{
		printf("Error setting DF Flag for UDP packets\n");
		exit(EXIT_FAILURE);
	}
	printf("[+]Set DF flag for UDP Packets\n");

	// setting source and destination port and IP for client and server
	struct sockaddr_in client_addr, server_addr;

	memset(&client_addr, 0, sizeof(client_addr));
	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(sourcePort);
	client_addr.sin_addr.s_addr = htonl(INADDR_ANY);	// Would be set to the IP address of the client VM

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(destinationPort);
	server_addr.sin_addr.s_addr = inet_addr(serverIP);

	// ------- LOW ENTROPY PACKET CREATION AND SEND --------
	unsigned char lowEntropyData[udpPayload]; // Creating Low Entropy data with the size of payload
	memset(lowEntropyData, 0, udpPayload);	// Clears the memory of the buffer to make sure it is empty
	unsigned char lowEntropyPacket[udpPayload + 2]; // +2 to add 2 bytes of space for the packet ID in the beginning of packet

	char* done = "done"; // char message to let the Server know it has sent the entire packet train

	for(unsigned short int id = 0; id < numPackets; id++)	// Sends numPackets
	{
		unsigned char idRight = id & 0xFF;	// Bit shift the last byte of the ID
		unsigned char idLeft = id >> 8;		// Bit shift the first byte of the ID
		
		// Stores the left in right bytes in the first two indicies of the packet
		lowEntropyPacket[0] = idLeft;	
		lowEntropyPacket[1] = idRight;

		strncpy(&lowEntropyPacket[2], lowEntropyData, udpPayload);	// Appends the LE data after the packet ID in packet

		// Sends the packet to the Server
		if(sendto(sock_desc, lowEntropyPacket, sizeof(lowEntropyPacket), 0, (struct sockaddr *)&server_addr,
		(socklen_t)sizeof(server_addr)) < 0)
		{
			perror("First UDP packet send error\n");
			exit(EXIT_FAILURE);
		}
		if(id == 0)
		{
			printf("[+]Successfully sent the first packet of LE Train\n");	// Prints to console that the first packet was sent
		}
		if(id == (numPackets - 1))
		{
			printf("[+]Successfully sent the last packet of LE Train\n");	// Prints to console that the last packet was sent
		}
	}

	// Sends "done" message to Server to indate that it finished sending the packet train
	if(sendto(sock_desc, (const char*)done, strlen(done), 0, (struct sockaddr *)&server_addr,
	(socklen_t)sizeof(server_addr)) < 0)
	{
		perror("First UDP packet send error\n");
		exit(EXIT_FAILURE);
	}
	printf("[+]Sent done message to Server\n");

	printf("\n");
	printf("Waiting Inter-Meausrement Time\n");
	printf("\n");

	// Will wait the Inter Measure Time before sending the High Entropy Packet Train
	sleep(interMeasureTime);

	// ------- HIGH ENTROPY PACKET CREATION AND SEND -------

    unsigned char highEntropyData[udpPayload];	// Used to store the High Entropy Data from /dev/urandom
    FILE *fp;
   	fp = fopen("/dev/urandom", "r");	// Opens the file and reads from it
   	if(fp == NULL)
   	{
   		printf("Cannot read from /dev/urandom\n");
   		exit(EXIT_FAILURE);
   	}
    fread(&highEntropyData, 1, udpPayload, fp); // reads from /dev/random and writes the random sequence of bytes to highEntropy
    fclose(fp);	// closes the file

    unsigned char highEntropyPacket[udpPayload + 2];	

	// Same process for sending packet train as Low Entropy train
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
			printf("[+]Successfully sent the first packet of HE Train\n");
		}
		if(id == (numPackets - 1))
		{
			printf("[+]Successfully sent the last packet of HE Train\n");
		}
	}

	if(sendto(sock_desc, (const char*)done, strlen(done), 0, (struct sockaddr *)&server_addr,
	(socklen_t)sizeof(server_addr)) < 0)
	{
			perror("First UDP packet send error\n");
			exit(EXIT_FAILURE);
	}
	printf("[+]Sent done message to Server\n");
	
	printf("[+]Successfully sent both packet trains\n");
	close(sock_desc);	// closes socket
}

/* 3. Receives the Compression Detection findings from Server through TCP connection */
void postProbing(char *serverIP, int tcpPort)
{
	int sock_desc = socket(AF_INET, SOCK_STREAM, 0);
	if(sock_desc < 0)
	{
		printf("Unable to create socket\n");
		exit(EXIT_FAILURE);
	}

	printf("[+]Socket created successfully\n");

	// Allows socket to reuse port after the first TCP connection and UDP connection
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

	char buffer[1024];	// buffer to store the contents received from Server

    // Receives Compression Cetection findings from the Server
	if(recv(sock_desc, buffer, sizeof(buffer), 0) < 0)
	{
     	perror("File not sent\n");
     	exit(EXIT_FAILURE);
     }

    printf("[+]Server Findings: \n");	
    printf("%s", buffer); 	// prints findings
	
	printf("[+]Closing the connection\n");
	close(sock_desc);
}

int main(int argc, char* argv[])
{
	// Checks if JSON file is attached along with the executable when calling program.
	// Exits program if not
	if(argc != 2)
	{
		perror("Configuration file missing\n");
		exit(EXIT_FAILURE);
	}

	// Creates a array of attributes that are used to assign the values of
	// the keys in the JSON file to variables declared in the program
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

	char buffer[1024];	// Buffer to store information in JSON file
	int length;
	FILE *fp;
	char *filename = argv[1];	// Filename is the second argument --> argv[1]
	fp = fopen(filename, "r");
		
	if(fp != NULL)
	{
		fseek(fp, 0, SEEK_END);	// Goes to the last position in the file
		length = ftell(fp);		// Gets the length of the file
		fseek(fp, 0, SEEK_SET);	// Sets the position to the beginning of the file again
		fread(buffer, 1, length, fp);	// reads each byte in the file to the buffer up to the length of the file
		fclose(fp);	// closes the file
	}
	
	// Parses the file and stores the values of keys into global variables
	if(json_read_object(buffer, json_attrs, NULL) < 0)
	{
		perror("Error getting from JSON file.\n");
		exit(EXIT_FAILURE);
	}

	//  Sets defaults to variables if values are invalid in JSON file */
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
	
 	// 1. Pre-probing Phase
	printf("Pre-probing Phase:\n");
	tcpConnection(servIP, TCPPort, buffer);
	printf("\n");

	// 2. Probing Phase
	printf("Probing Phase:\n");
	udpConnection(servIP, sourcePortUDP, destPortUDP, UDPPayload, numUDPPack, measureTime);
	printf("\n");

	// 3. Post-Probing Phase
	printf("Post-Probing Phase: \n");
	postProbing(servIP, TCPPort);
	
	return 0;
}
