// Gerald Abut
// ICS 451 Assignment 12: Learning Switch Simulation
// 5/4/2015

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>	// for ntohs, htons, inet_aton, inet_ntoa
#include <sys/types.h>	// for getpid
#include <unistd.h>		// for getpid
#include <sys/time.h>	// for gettimeofday

#define BUFF_SIZE 			100
#define MAX_ROUTE 			5
#define PRINT 				0	// false = 0, true = 1


typedef struct Ethernet_Frame {
	u_int16_t   eth_type;               // 2 bytes
	u_int16_t   payload_size;          	// store the size to let other programs know how big the payload is
	//u_int8_t    *payload;            	// variable value ranging from 0-1500. this must be malloced
	u_int8_t	payload[1500];
	
    u_int8_t	local_mac_addr[6];		// 6 bytes or 48 bits
    u_int8_t	remote_mac_addr[6];		// 6 bytes or 48 bits
    
	u_int16_t	src_port;
} Ethernet_Frame;


int main (int argc, char **argv) {
    int local_udp_port;
    int remote_udp_port;
    int i = 0;  // counter
    int n = 0;	// counter
    char ipv6_localhostDottedFormat[INET6_ADDRSTRLEN];
    char * localhost_ipv6 = "::1";			// LOOP BACK ADDRESS
    u_int8_t localhost_ipv6_addr[16];		// in network byte order
    struct Ethernet_Frame eth_frame_packet;
    
    // get local udp port number
    local_udp_port = (int) strtol(argv[1], (char **) NULL, 10);	// base ten
    if (PRINT) {printf("local udp port:\t\t\t\t%d\n", local_udp_port);}
    
    // get remote udp port number
    remote_udp_port = (int) strtol(argv[2], (char**)NULL, 10); // base ten
    if (PRINT) {printf("remote udp port:\t\t\t%d\n", remote_udp_port); }
    
    eth_frame_packet.payload_size = (int)strtol(argv[3], (char**)NULL, 10);   // base ten
    if (PRINT) {printf("packet size:\t\t\t\t%d\n", eth_frame_packet.payload_size);}
    
	// RESERVE PAYLOAD SIZE
    //eth_frame_packet.payload = (u_int8_t *)malloc(sizeof(u_int8_t) * eth_frame_packet.payload_size);
    for (i = 0; i < eth_frame_packet.payload_size; i++) {
       eth_frame_packet.payload[i] = 0x00;	// initialize payload to zero
    }
    eth_frame_packet.payload[i] = '\0';
    
    
    // determine size to check for correctness
    i = 0;  // reset counter
    while (i < eth_frame_packet.payload_size) {
		if (PRINT) {printf("%d", eth_frame_packet.payload[i]);}
        i++;
    }
    if (PRINT) {printf("\n");}
    if (PRINT) {printf("payload size:\t\t\t\t%d\n", i);}


    // GET LOCAL MAC ADDRESS
    i = 0;  // reset counter
    n = 0;	// reset counter
    while (n < 6) {	// 6 bytes 
		char x[2];
		x[0] = argv[4][i];
		i++;
		x[1] = argv[4][i];
		i++;
		x[2] = '\0';
		char num = (char) strtol(x, (char**)NULL, 10); // base ten
		//printf("%d\n", num);
		eth_frame_packet.local_mac_addr[n] = num;
		n++;
		i++;
    }
	// CONVERT LOCAL MAC ADDRESS BACK TO DOTTED FORMAT
	char local_mac_Str[12];
	sprintf(local_mac_Str, "%d:%d:%d:%d:%d:%d", eth_frame_packet.local_mac_addr[0] & 0xff, eth_frame_packet.local_mac_addr[1] & 0xff, eth_frame_packet.local_mac_addr[2] & 0xff, eth_frame_packet.local_mac_addr[3] & 0xff, eth_frame_packet.local_mac_addr[4] & 0xff, eth_frame_packet.local_mac_addr[5] & 0xff);
	if (PRINT) {printf("Local MAC address string:\t\t%s\n", local_mac_Str);}

	// GET REMOTE MAC ADDRESS
    i = 0;  // reset counter
    n = 0;	// reset counter
    while (n < 6) {	// 6 bytes 
		char x[2];
		x[0] = argv[5][i];
		i++;
		x[1] = argv[5][i];
		i++;
		x[2] = '\0';
		char num = (char) strtol(x, (char**)NULL, 10); // base ten
		//printf("%d\n", num);
		eth_frame_packet.remote_mac_addr[n] = num;
		n++;
		i++;
    }
    
    // CONVERT REMOTE MAC ADDRESS BACK TO DOTTED FORMAT (debugging)
	char remote_mac_Str[12];
	sprintf(remote_mac_Str, "%d:%d:%d:%d:%d:%d", eth_frame_packet.remote_mac_addr[0] & 0xff, eth_frame_packet.remote_mac_addr[1] & 0xff, eth_frame_packet.remote_mac_addr[2] & 0xff, eth_frame_packet.remote_mac_addr[3] & 0xff, eth_frame_packet.remote_mac_addr[4] & 0xff, eth_frame_packet.remote_mac_addr[5] & 0xff);
	if (PRINT) {printf("Remote MAC address string:\t\t%s\n", remote_mac_Str);}
    
    
    // INITIALIZE ETHERNET TYPE
    eth_frame_packet.eth_type = 0x8888;	// decimal 34952
    if (PRINT) {printf("ethernet type:\t\t\t\t%#x\n", eth_frame_packet.eth_type);}
    
    // CONVERT LOCAL HOST INTO BYTES
	if (inet_pton (AF_INET6, localhost_ipv6, localhost_ipv6_addr) != 1) {	// localhost address 128 bits
        printf ("usage:\t%s udp-port ipv6-source ipv6-dest\n", argv [0]);
        printf ("\tsource address %s is not a valid IPv6 address\n", localhost_ipv6);
        return 1;
    }
    
    // CONVERT BYTES TO LOCAL HOST AGAIN (for testing)
	if (inet_ntop(AF_INET6, localhost_ipv6_addr, ipv6_localhostDottedFormat, INET6_ADDRSTRLEN) == NULL) {
        perror("inet_ntop\n");
        exit(EXIT_FAILURE);
    }
    
    // LET EVERY THREAD KNOW YOUR SOURCE PORT
    eth_frame_packet.src_port = local_udp_port;
        
    
    //********************************* SENDING ************************************************************
    
    // ESTABLISH REMOTE UDP CONNECTION USING IPV6 ADDRESS TO SEND MESSAGE
    int af_remote = AF_INET6;	// address family
    int sockfd_remote = 0;
    struct sockaddr_in6 sin6_remote;
    bzero(&sin6_remote, sizeof(sin6_remote));	// zero out
    sin6_remote.sin6_family = af_remote;
    sin6_remote.sin6_port = htons(remote_udp_port);
    
    // CONVERSION FROM IPV6 STRING TO BINARY ALREADY HAPPENED ABOVE, SIMPLY COPY TO STRUCT
    memcpy(sin6_remote.sin6_addr.s6_addr, localhost_ipv6_addr, sizeof(localhost_ipv6_addr));
    
    // CREATE SOCKET FOR IPv6
    if((sockfd_remote = socket(af_remote, SOCK_DGRAM, 0)) < 0){
        printf("Error: Could not open socket\n");
        exit(1);
    }
    if (PRINT) {printf("Success: Sending socket was created for IPv6 address\n");}
    
    // CONNECT
    if (connect (sockfd_remote, (struct sockaddr *)&sin6_remote, sizeof(sin6_remote)) == -1){
        printf ("Error: Could not establish connection\n");
        exit(1);
    }
    if (PRINT) {printf ("Success: Connect to remote port %d\n", remote_udp_port);}
    
   
	// SEND ETHERNET FRAME PACKET
    int len = sizeof(eth_frame_packet);
    char buff[len];
    memcpy(buff, &eth_frame_packet, len);
    
    int err = 0;
    if ((err = sendto (sockfd_remote, buff, len, 0, (struct sockaddr *)&sin6_remote, sizeof(sin6_remote))) == -1){
        printf ("Error: Did not send file to IP [%s] with error code [%d]\n", ipv6_localhostDottedFormat, err);
        exit(1);
    }

    // CONVERT LOCAL AND REMOTE MAC ADDRESS BACK TO DOTTED FORMAT (debugging)
    //printf("Sending ethernet frame packet using local host IP %s ", ipv6_localhostDottedFormat);
	printf("sending %d bytes", eth_frame_packet.payload_size);

	char src[12];
	sprintf(src, "%d:%d:%d:%d:%d:%d", eth_frame_packet.local_mac_addr[0] & 0xff, eth_frame_packet.local_mac_addr[1] & 0xff, eth_frame_packet.local_mac_addr[2] & 0xff, eth_frame_packet.local_mac_addr[3] & 0xff, eth_frame_packet.local_mac_addr[4] & 0xff,eth_frame_packet.local_mac_addr[5] & 0xff);
	//printf("from local MAC %s ", src);
	printf(" %s", src);

	char dest[12];
	sprintf(dest, "%d:%d:%d:%d:%d:%d", eth_frame_packet.remote_mac_addr[0] & 0xff, eth_frame_packet.remote_mac_addr[1] & 0xff,eth_frame_packet.remote_mac_addr[2] & 0xff,eth_frame_packet.remote_mac_addr[3] & 0xff, eth_frame_packet.remote_mac_addr[4] & 0xff,eth_frame_packet.remote_mac_addr[5] & 0xff);
	//printf("to remote MAC %s\n", dest);
	printf(" to %s\n", dest);

    
    //printf("Sending ethernet frame packet using local host IP %s from local mac address %s to remote mac address %s\n", ipv6_localhostDottedFormat, src, dest);
    memset(buff,'\0', len);	// reset buff
    
    
    //************************** AFTER SENDING MESSAGE LISTEN FOR INCOMING MESSAGES ***********************
    
    // CREATE A CONNECTION FOR LISTENING
    int af = AF_INET6;	// address family
    int sockfd = 0;
    int port = local_udp_port;
    struct sockaddr_in6 sin6_receiver, sin6_sender;
    bzero(&sin6_receiver, sizeof(sin6_receiver));	// zero out
    sin6_receiver.sin6_family = af;
    sin6_receiver.sin6_port = htons(port);
    
    // CONVERT IPV6 LOOPBACK ADDRESS STRING TO NETWORK BINARY TO STORE IN STRUCT
    if (inet_pton (AF_INET6, localhost_ipv6, &sin6_receiver.sin6_addr) != 1) {
        printf ("usage: %s udp-port ipv6-source ipv6-dest\n", argv [0]);
        printf ("       source address %s is not a valid IPv6 address", argv [2]);
        return 1;
    }
    
    err = 0;
    
    // CREATE SOCKET FOR IPv6
    if((sockfd = socket(af, SOCK_DGRAM, 0)) < 0){
        printf("Error: Could not open socket with error code %d\n", sockfd);
        exit(1);
    }
    if (PRINT) {printf("Success: Socket created for IPv6 address\n");}
    
    // BIND TO PORT
    if ((err = bind(sockfd, (struct sockaddr *)&sin6_receiver, sizeof(sin6_receiver))) == -1) {
        printf("Error: Could not bind to port %d with error code %d\n", port, err);
        exit(1);
    }
    if (PRINT) {printf("Success: Binded to port: %d\n", port);}
    
   
    // KEEP LISTENING FOR A GIVEN ROUTE AND DETERMINE WHICH ROUTE TO GO IN THE LOOKUP TABLEz
    char packet[len];
    int recv_len = 0;
    struct Ethernet_Frame * h;	// = (Ethernet_Frame *)malloc(sizeof(Ethernet_Frame));
    while (1) {	// KEEP LISTENING
        
        printf("Host is listening to port %d...\n", port);
        fflush(stdout);
        
        // RECEIVE PACKET FROM SENDER (40 bytes)
        int sin6_size = sizeof(sin6_sender);
        if ((recv_len = recvfrom(sockfd, packet, sizeof(packet),0, (struct sockaddr *)&sin6_sender, (socklen_t*)&sin6_size)) < 0) {
            printf("Error: Did not receive response (IPv6 address) with error code %d\n", recv_len);
            continue;	// keep looping
        }
        
        //printf("Received  packet with length [%3d] from Sender with IP [%s] at port [%d]\n", strlen(recv_buff), packet, ntohs(sin6_sender.sin6_port));
        //printf("Data obtained from Sender: %s\n", recv_buff);
        //printf("Buff length: %d\n", strlen(recv_buff));
        
        //*************************************************************************************************
        h = (Ethernet_Frame *)packet;
		
		if (PRINT) {
			printf("Ethernet Frame Packet Content:\n");
			printf("\tEthernet Type:\t%d\n", h->eth_type);
			printf("\tPayload Size:\t%d\n", h->payload_size);
			//printf("\tLocal Mac:\t%s\n", h->local_mac_addr);
			//printf("\tRemote Mac:\t%s\n", h->remote_mac_addr);
		}
		
		
		// CONVERT LOCAL AND REMOTE MAC ADDRESS BACK TO DOTTED FORMAT (debugging)
		printf("got %d bytes", h->payload_size);
		char local_mac_str[12];
		sprintf(local_mac_str, "%d:%d:%d:%d:%d:%d", h->local_mac_addr[0] & 0xff, h->local_mac_addr[1] & 0xff,h->local_mac_addr[2] & 0xff, h->local_mac_addr[3] & 0xff, h->local_mac_addr[4] & 0xff,h->local_mac_addr[5] & 0xff);
		if (PRINT) {printf("\tLocal MAC:\t%s\n", local_mac_str);}
		printf(" %s to", local_mac_str);
		
		char remote_mac_str[12];
		sprintf(remote_mac_str, "%d:%d:%d:%d:%d:%d", h->remote_mac_addr[0] & 0xff, h->remote_mac_addr[1] & 0xff,h->remote_mac_addr[2] & 0xff, h->remote_mac_addr[3] & 0xff, h->remote_mac_addr[4] & 0xff,h->remote_mac_addr[5] & 0xff);
		if (PRINT) {printf("\tRemote MAC:\t%s\n", remote_mac_str);}
        printf(" %s\n", remote_mac_str);
		if (PRINT) {printf("\tPayload:\n");}
		int p = 0;
		for (p = 0; p < h->payload_size; p++) {
			if ((p % 50) == 0) {
				if (PRINT) {printf("\n");}	// new line
			}
			if (PRINT) {printf("%d", h->payload[p]);}
		}
		if (PRINT) {printf("\n");}	// new line
    
        
    
        //int err = 0;
        // JUST SEND THE ORIGINAL PACKET TO SO THAT SENDER KNOWNS IT WAS RECEIVED
        //if ((err = sendto (sockfd_remote, packet, sizeof(packet), 0, (struct sockaddr *)&sin6_remote, sizeof(sin6_remote))) == -1) {
        //    printf("Could not send back package to confirm that it has been received\n");
        //    exit(1);
        //}
        
    }	// end of while loop for listening
    
    free(eth_frame_packet.payload);
    return 0;
}
