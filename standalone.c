#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <time.h>
#include "mjson.h"


char servIP[40];
int sourcePortUDP, destPortUDP, destPortTCPHead, destPortTCPTail,
    tcpPort, UDPPayload, measureTime, numUDPPack, ttlUdp;
static double times[2];


/* Calculates Header Checksum */
unsigned short checksum(unsigned short* pts, int n)
{
	unsigned long sum;
	for(sum = 0; n > 0; n--)
	{
		sum += *pts++;
	}
	sum = (sum >> 16) + (sum &0xffff);
	sum += (sum >> 16);
	return (unsigned short)(~sum);
}


/* Void function that sets the fields for the IP Header */
void ipHeaderAttrs(struct iphdr* ipHeader, struct sockaddr_in addrSynHead, char* packet)
{
	ipHeader->ihl = 5;
	ipHeader->version = 4;
	ipHeader->tos = 0;
	ipHeader->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr);
	ipHeader->id = htonl(54321);
	ipHeader->frag_off = 0;
	ipHeader->ttl = 255;
	ipHeader->protocol = IPPROTO_TCP;
	ipHeader->check = 0; /* checksum is set to 0 intially before computing */
	ipHeader->saddr = inet_addr("127.0.0.1"); // Or set to client IP?
	ipHeader->daddr = addrSynHead.sin_addr.s_addr;

	ipHeader->check = checksum((unsigned short *) packet, ipHeader->tot_len);
}


void tcpHeaderAttrs(struct tcphdr* tcpHeader, int destPort)
{
	tcpHeader->source = htons(1234);
	tcpHeader->dest = htons(destPort);
	tcpHeader->seq = random();
	tcpHeader->ack_seq = 0;
	tcpHeader->th_flags = TH_SYN;
	tcpHeader->window = htons(65535);
	tcpHeader->check = 0;
	tcpHeader->urg = 0;
	tcpHeader->urg_ptr = 0;
	tcpHeader->th_off = 0;
}


void lowPacketTrain(struct sockaddr_in server_addr, int sock)
{

	clock_t start, end;

	/* ------- LOW ENTROPY PACKET TRAIN ------- */
	unsigned char lowEntropyData[UDPPayload]; // Creating low entropy with the size of payload
	memset(lowEntropyData, 0, UDPPayload);
	unsigned char lowEntropyPacket[UDPPayload + 2]; // +2 to add space for the packet ID in the beginning of the packet
	
	char* done = "done"; // char message to let the Server know it has sent the entire packet train
	start = clock();
	
	for(unsigned short int id = 0; id < numUDPPack; id++)
	{
		unsigned char idRight = id & 0xFF;	// Bit shift the last byte of the ID
		unsigned char idLeft = id >> 8;		// Bit shift the first byte of the ID
			
		// Stores the left in right bytes in the first two indicies of the packet
		lowEntropyPacket[0] = idLeft;	
		lowEntropyPacket[1] = idRight;
	
		strncpy(&lowEntropyPacket[2], lowEntropyData, UDPPayload);	
		if(sendto(sock, lowEntropyPacket, sizeof(lowEntropyPacket), 0, (struct sockaddr *)&server_addr,
		(socklen_t)sizeof(server_addr)) < 0)
		{
			perror("First UDP packet send error\n");
			exit(EXIT_FAILURE);
		}
		if(id == 0)
		{
			printf("[+]Successfully sent the first packet\n");
		}
		if(id == (numUDPPack - 1))
		{
			printf("[+]Successfully sent the last packet\n");
			end = clock();
		}
	}

	times[0] = (double)((end - start) * 1000 / CLOCKS_PER_SEC);
	
	if(sendto(sock, (const char*)done, strlen(done), 0, (struct sockaddr *)&server_addr,
	(socklen_t)sizeof(server_addr)) < 0)
	{
		perror("First UDP packet send error\n");
		exit(EXIT_FAILURE);
	}
	printf("[+]Sent done message to Server\n");
	
	// Will wait the Inter Measure Time before sending the High Entropy Packet Train
	sleep(measureTime);

}

void highPacketTrain(struct sockaddr_in server_addr, int sock)
{
	/* ------- HIGH ENTROPY PACKET CREATION AND SEND ------- */

	clock_t start, end;
	char* done = "done";
	unsigned char highEntropyData[UDPPayload];
    FILE *fp;
   	fp = fopen("/dev/urandom", "r");
   	if(fp == NULL)
   	{
   		perror("Cannot read from /dev/urandom\n");
  		exit(EXIT_FAILURE);
    }
    fread(&highEntropyData, 1, UDPPayload, fp); // reads from /dev/random and writes the random sequence of bytes to highEntropy
    fclose(fp);
	
    unsigned char highEntropyPacket[UDPPayload + 2];
	start = clock();	
	for(unsigned short int id = 0; id < numUDPPack; id++)
	{
		unsigned char idRight = id & 0xFF;	// Bit shift the last byte of the ID
		unsigned char idLeft = id >> 8;		// Bit shift the first byte of the ID
					
		// Stores the left in right bytes in the first two indicies of the packet
		highEntropyPacket[0] = idLeft;	
		highEntropyPacket[1] = idRight;
			
		strncpy(&highEntropyPacket[2], highEntropyData, UDPPayload);	
		if(sendto(sock, highEntropyPacket, sizeof(highEntropyPacket), 0, (struct sockaddr *)&server_addr,
		(socklen_t)sizeof(server_addr)) < 0)
		{
			perror("First UDP packet send error\n");
			exit(EXIT_FAILURE);
		}
					
		if(id == 0)
		{
			printf("[+]Successfully sent the first packet\n");
		}
		if(id == (numUDPPack - 1))
		{
			printf("[+]Successfully sent the last packet\n");
			end = clock();
		}
	}

	times[1] = (double)((end - start) * 1000 / CLOCKS_PER_SEC);
	if(sendto(sock, (const char*)done, strlen(done), 0, (struct sockaddr *)&server_addr,
	(socklen_t)sizeof(server_addr)) < 0)
	{
			perror("First UDP packet send error\n");
			exit(EXIT_FAILURE);
	}
	printf("[+]Sent done message to Server\n");
		
	close(sock);
	printf("[+]Successfully sent both packet trains\n");
}


int main(int argc, char* argv[])
{
	// Expects 2 arguemtns, the executbale and the config file.
	// Will exit program if configuration file is not include
	if(argc != 2)
	{
		perror("Configuration file missing\n");
		exit(EXIT_FAILURE);
	}

	// A JSON array struct that stores the values in the configuration file.
	// First argument looks for that existing string in the JSON file.
	// Stores the value of that key value pair in a global variable
	const struct json_attr_t json_attrs[] =
	{
	   	{"servIP", t_string, .addr.string = servIP, .len = sizeof(servIP)},
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

	char buffer[1024]; // To store the contents in the JSON file
	int length;
	FILE *fp;
	char* filename = argv[1];
	fp = fopen(filename, "r");

	// Goes through the file and stores each line in the buffer
	if(fp != NULL)
	{
		fseek(fp, 0, SEEK_END);
		length = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		fread(buffer, 1, length, fp);
		fclose(fp);
	}

	// Function in the API that matches the variables in the buffer to the
	// json_attrs struct to parse the values to the global variables that
	// will be used in the program
	if(json_read_object(buffer, json_attrs, NULL) < 0)
	{
		perror("Error parsing JSON file\n");
		exit(EXIT_FAILURE);
	}

	/* Setting default values if values in JSON file are invalid */
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

	
	/* ------- CREATING TCPHEAD, TCPTAIL, & UDP SOCKETS ------- */

	// TCP HEAD
	int tcpHeadSock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
	if(tcpHeadSock < 0)
	{
		perror("Unable to create socket for TCP HEAD\n");
		exit(EXIT_FAILURE);
	}
	struct sockaddr_in tcpHead;
	tcpHead.sin_family = AF_INET;
	tcpHead.sin_port = htons(destPortTCPHead);
	tcpHead.sin_addr.s_addr = inet_addr(servIP);
	int val = 1;
	if(setsockopt(tcpHeadSock, IPPROTO_IP, IP_HDRINCL, &val, sizeof(val)) < 0)
	{
		perror("setsockopt for tcpHeadSock error\n");
		exit(EXIT_FAILURE);
	}
	

	// TCP TAIL
	int tcpTailSock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
	if(tcpTailSock < 0)
	{
		perror("Unable to create socket for TCP HEAD\n");
		exit(EXIT_FAILURE);
	}
	struct sockaddr_in tcpTail;
	tcpTail.sin_family = AF_INET;
	tcpTail.sin_port = htons(destPortTCPTail);
	tcpTail.sin_addr.s_addr = inet_addr(servIP);
	val = 1;
	if(setsockopt(tcpTailSock, IPPROTO_IP, IP_HDRINCL, &val, sizeof(val)) < 0)
	{
		perror("setsockopt for tcpHeadSock error\n");
		exit(EXIT_FAILURE);
	}
	

	// UDP
	int UDPSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(UDPSock < 0)
	{
		perror("Unable to create socket for UDP\n");
		exit(EXIT_FAILURE);
	}
	struct sockaddr_in udpAddr;
	udpAddr.sin_family = AF_INET;
	udpAddr.sin_port = htons(destPortUDP);
	udpAddr.sin_addr.s_addr = inet_addr(servIP);

	// Setting DF flag using setsockopt()
	int valUDP = IP_PMTUDISC_DO;
	int sockopt = setsockopt(UDPSock, IPPROTO_IP, IP_MTU_DISCOVER, &valUDP, sizeof(val));
	if(sockopt < 0)
	{
		printf("Error setting DONT FRAG for UDPSock in setsockopt\n");
		exit(EXIT_FAILURE);
	}	

	// Setting TTL for packets in UDP Packet Train
	if(setsockopt(UDPSock, IPPROTO_IP, IP_TTL, &ttlUdp, sizeof(ttlUdp)) < 0)
	{
		perror("Error setting TTL for UDPSock in setsockopt\n");
		exit(EXIT_FAILURE);
	}

	char packet[1000];
	memset(packet, 0, sizeof(packet));	//zero out the packet buffer

	//IP header
	struct iphdr* ipHeader = (struct iphdr*) packet;

	//TCP Header
	struct tcphdr* tcpHeader = (struct tcphdr*) packet + sizeof(struct iphdr);

	 /* Sending TCP HEAD SYN Packet & LOW ENTROPY PACKET TRAIN */
	ipHeaderAttrs(ipHeader, tcpHead, packet);
	tcpHeaderAttrs(tcpHeader, destPortTCPHead);


	/* ------- SENDING TCP HEAD, PACKET TRAIN, THEN TCP TAIL ------- */
	if (sendto(tcpHeadSock, (char *) packet, ipHeader->tot_len, 0, (struct sockaddr *)&tcpHead, sizeof(tcpHead)) < 0)
	{
    	printf("error: could not send TCP Syn Head on raw socket\n");
	}
	printf("sent TCP Syn to port: %d\n", destPortTCPHead);

	lowPacketTrain(udpAddr, UDPSock);

	memset(packet, 0, sizeof(packet));
	if (sendto(tcpHeadSock, (char *) packet, ipHeader->tot_len, 0, (struct sockaddr *)&tcpTail, sizeof(tcpTail)) < 0)
	{
    	printf("error: could not send TCP Syn Tail on raw socket\n");
	}
	printf("sent TCP Syn to port: %d\n", destPortTCPHead);


	sleep(measureTime);

	
	if (sendto(tcpHeadSock, (char *) packet, ipHeader->tot_len, 0, (struct sockaddr *)&tcpHead, sizeof(tcpHead)) < 0)
	{
    	printf("error: could not send TCP Syn Head on raw socket\n");
	}
	printf("sent TCP Syn to port: %d\n", destPortTCPHead);

	highPacketTrain(udpAddr, UDPSock);

	memset(packet, 0, sizeof(packet));
	if (sendto(tcpHeadSock, (char *) packet, ipHeader->tot_len, 0, (struct sockaddr *)&tcpTail, sizeof(tcpTail)) < 0)
	{
    	printf("error: could not send TCP Syn Tail on raw socket\n");
	}
	printf("sent TCP Syn to port: %d\n", destPortTCPHead);

	double timeDifference = times[1] - times[0];

	printf("Low Entropy Packet Train Time: %fms\n", *times+0);
	printf("High Entropy Packet Train Time: %fms\n", *times+1);
	printf("Time DIfference: %fms\n", timeDifference);

	close(tcpHeadSock);
	shutdown(tcpHeadSock, SHUT_RDWR);
	
	return 0;
}
