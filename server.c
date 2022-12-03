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


/* GLOBAL VARIABLES TO STORE CONFIG CONTENTS */
char servIP[40];
int sourcePortUDP, destPortUDP, destPortTCPHead, destPortTCPTail,
    tcpPort, UDPPayload, measureTime, numUDPPack, ttlUdp;
static double times[2];

/* Helper function that parses the contents in the buffer */
void parser(char* buffer)
{
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
        {"TCPPort", t_integer, .addr.integer = &tcpPort},
        {"UDPPayload", t_integer, .addr.integer = &UDPPayload},
        {"measureTime", t_integer, .addr.integer = &measureTime},
        {"numUDPPack", t_integer, .addr.integer = &numUDPPack},
        {"ttlUdp", t_integer, .addr.integer = &ttlUdp},
        {NULL},
    };

	// Function used from mjson.c file that takes in a buffer that stores the contents and
	// parses it with the array of json attributes. Returns -1 if an error occurs.
	if(json_read_object(buffer, json_attrs, NULL) < 0)
	{
		perror("Error getting from JSON file.\n");
		exit(EXIT_FAILURE);
    }

	// Sets the defaults for variables if values in buffer received are invalid
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

	// Prints the information after parsed
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
void tcpConnection(char* servIP, int tcpPort)
{

	// Creates the socket
	int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
	if(socket_desc < 0)
	{
		perror("Error creating socket\n");
		exit(EXIT_FAILURE);
	}
	printf("[+]Socket created successfully\n");

	// Used to allow the socket to reuse the same port when executing the program
	// multiple times
	int val = 1;
	int setsock = setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, (void*)&val, sizeof(val));
	if(setsock < 0)
	{
		perror("setsockopt error\n");
		exit(EXIT_FAILURE);
	}

	//Sets the TCP Port and the Server IP address for server
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(tcpPort);
	server_addr.sin_addr.s_addr = inet_addr(servIP);

	// Binds socket to address
	if(bind(socket_desc, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
	{
		perror("Server could not bind to port\n");
		exit(EXIT_FAILURE);
	}
	
	printf("[+]Server successfully binded to port\n");

	// Listens for incoming connection from Client
	if(listen(socket_desc, 1) < 0)
	{
		printf("Error while listening for client\n");
		exit(EXIT_FAILURE);
	}
	printf("[+]Listening for client\n");

	// struct used to store and represent information from the Client
	struct sockaddr_in client_addr;
	int client_size = sizeof(client_addr);
	int client_sock;

	// accepts the incoming request from the Client to the server socket
	client_sock = accept(socket_desc, (struct sockaddr*)&client_addr, &client_size);
	if(client_sock < 0)
	{
		perror("Could not connect with client\n");
		exit(EXIT_FAILURE);
	}
	printf("[+]Client connected. IP: %s \t port: %i\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
	printf("\n");

	// buffer to store the information sent from the Client
	char buffer[1024];

	// Receives the information from the Client and stores it in the buffer
	if(recv(client_sock, buffer, sizeof(buffer), 0) < 0)
	{ //(struct sockaddr *) &client_addr, &client_size) < 0){
		perror("Error recieving content from client\n");
		exit(EXIT_FAILURE);
	}

	// Prints the information sent from the Client after parsed
	printf("Info recieved: \n");
	parser(buffer);

	// closes the TCP connetion between Client and Server
	printf("[+]Closing connection with Client\n");
	close(client_sock);
	close(socket_desc);
}


/* 2. Receiving Packet Trains from Client and calculates if compression was detected using UDP Connection */
void udpConnection(char* serverIP, int port, int udpPayload)
{
	// Creates the socket
	int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sockfd < 0)
	{
		printf("Error creating socket\n");
		exit(EXIT_FAILURE);
	}
	printf("[+]Socket created successfully\n");

	// sockaddr_in structs to set the port and IP address of Server and to store
	// the inforamtion from the Client
	struct sockaddr_in server_addr, client_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	memset(&server_addr, 0, sizeof(client_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = inet_addr(serverIP);

	// Binds socket to address
	if(bind(sockfd, (const struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
	{
		printf("Server could not bind to port\n");
		exit(EXIT_FAILURE);
	}
	printf("[+]Server successfully binded to port\n");

	char buffer[udpPayload];	// Buffer that stores the contents received from Client
	int numReceived = 0;		// Counter that keeps track of how many packets were recieved
	int len = sizeof(client_addr);

	// intialized clock structs to record time
	clock_t start, end;

	// Recieves packets from the Low entropy packet train. Breaks loop once datagram
	// sent is the "done" message
	start = clock();	// Starts clock
	while(true)
	{
		// Recieves packet from Client and is stored in buffer
		if(recvfrom(sockfd, buffer, sizeof(buffer), MSG_WAITALL, (struct sockaddr *) &client_addr, &len) < 0)
		{
				perror("Error recieving UDP packet: recvfrom()\n");
				exit(EXIT_FAILURE);
		}

		// Checks to see if the buffer contains "done". If it does, it exits the while loop
		// and indicates that the Client fully sent the packet train
		if(strstr(buffer, "done"))
		{
			end = clock();	// Stops the clock
			break;
		}
		
		numReceived++;	// increases numPackets once a packet is received
	}

	// Calculates the time of compression of the packet train in miliseconds, stores it
	// in the double array, then prints the result
	times[0] = (double)((end - start) * 1000 / CLOCKS_PER_SEC);
	printf("Low Entropy Train --> Number of packets Recieved: %d\n", numReceived);

	numReceived = 0; // resets the counter of number of packets received

	// Does the same process for receiving the High Entropy packet train
	start = clock();
	while(true)
	{
		if(recvfrom(sockfd, buffer, sizeof(buffer), MSG_WAITALL, (struct sockaddr *) &client_addr, &len) < 0)
		{
			perror("Error recieving UDP packet: recvfrom()\n");
			exit(EXIT_FAILURE);
		}
	
		if(strstr(buffer, "done"))
		{
			end = clock();
			break;
		}
			
		numReceived++;
	}

	times[1] = (double)((end - start) * 1000 / CLOCKS_PER_SEC);
	printf("High Entropy Train --> Number of packets Recieved: %d\n", numReceived);			
	
	close(sockfd);	// cloeses the socket once Client has sent both packet trains
	
}


/* Second TCP connection takes in the two records found from the UDP connection,
calculates to see if compression is detected, then sends its findings to the client
*/

/* 3. TCP Connection with Client to send the Compression Detection findings */
void postProbing(int tcpPort, double *times)
{

	// Booleans that represent if compression is detected for both LE and HE trains
	bool lowCompressed = true;
	bool highCompressed = true;

	// Retrieves times from times array
	double lowEntropyTime = times[0];
	double highEntropyTime = times[1];

	printf("lowEntropyTime: %f\nhighEntropyTime: %f\n", lowEntropyTime, highEntropyTime);

	// Checks the time to see if they are under the threshold of 100 ms
	if(lowEntropyTime < 100)
	{
		lowCompressed = false;
	}

	if(highEntropyTime < 100)
	{
		highCompressed = false;
	}	

	// Writes the message of compression detection of both packet trains to send to client
	char compressionMessage[1024];
	if(lowCompressed)
	{
		strcpy(compressionMessage, "\tLow Entropy Train: Compression detected!\n");
	}
	else
	{
		strcpy(compressionMessage, "\tLow Entropy Train: No compression was detected!\n");
	}

	if(highCompressed)
	{
		strcat(compressionMessage, "\tHigh Entropy Train: Compression detected!\n");
	}
	else
	{
		strcat(compressionMessage, "\tHigh Entropy Train: No compression was detected!\n");
	}

	// Same process to create sock and connect to client
	
	int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
	if(socket_desc < 0)
	{
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
	
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(tcpPort);
	server_addr.sin_addr.s_addr = inet_addr(servIP);
		
	if(bind(socket_desc, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
	{
		perror("Server could not bind to port\n");
		exit(EXIT_FAILURE);
	}
		
	printf("[+]Server successfully binded to port\n");
		
	if(listen(socket_desc, 1) < 0)
	{
		printf("Error while listening for client\n");
		exit(EXIT_FAILURE);
	}
	printf("[+]Listening for client\n");
			
	struct sockaddr_in client_addr;
	int client_size = sizeof(client_addr);
	int client_sock;
		
	client_sock = accept(socket_desc, (struct sockaddr*)&client_addr, &client_size);
	if(client_sock < 0)
	{
		perror("Could not connect with client\n");
		exit(EXIT_FAILURE);
	}

	printf("[+]Client connected. IP: %s \t port: %i\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

	// sends the compession detection findings of packet trains to Client
	if(send(client_sock, compressionMessage, strlen(compressionMessage), 0) < 0)
	{
		perror("Error sending compression message to client\n");
		exit(EXIT_FAILURE);
	}
	printf("[+]Sent Compression findings to client\n");

	// Closes Server and Client socket after sending findings to Client
	printf("[+]Closing connection with Client\n");
	close(client_sock);
	close(socket_desc);
	
}


int main(int argc, char* argv[])
{
	// Exits program if there are not 2 arguments present: executable name and JSON file
	if(argc != 2)
	{
		perror("Configuration file missing\n");
		exit(EXIT_FAILURE);
	}

	// Parses JSON File to get Source Port */
    const struct json_attr_t json_attrs[] =
    {
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

	printf("Server IP: %s\n", servIP);
	printf("TCPport: %d\n", tcpPort);
	printf("\n");
	printf("Pre-probing Phase:\n");
	tcpConnection(servIP, tcpPort);
	printf("\n");

	printf("Probing Phase:\n");
	udpConnection(servIP, destPortUDP, UDPPayload);
	printf("\n");

	printf("Low Entropy Packet Train Compression Time: %fms\n", *times+0);
	printf("High Entropy Packet Train Compression Time: %fms\n", *times+1);
	printf("\n");

	printf("Post-probing Phase:\n");
	postProbing(tcpPort, times);
	printf("\n");
	
	return 0;
}
