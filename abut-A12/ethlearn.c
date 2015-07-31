// Gerald Abut
// ICS 451 Assignment 12: Learning Switch Simulation
// 5/4/2015

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>	// for htohs, htons, inet_aton, inet_ntoa
#include <inttypes.h>	// for PRIu32 print format
#include <pthread.h>    // for thread functions, may need to be explicitly link with -lpthread command
#include <signal.h>		// sigaction
#include <errno.h>		// to check if sigaction changed this value
#include <unistd.h>		// alarm

#define BUFF_SIZE 		100
#define MAX_ROUTE 		6 	// REALLY ONLY 5 (N-1)
#define PRINT			0	// 0 = FALSE, 1 = TRUE
#define HEADER_SIZE 	40
#define WAIT_TIME		10
#define MAX_WAIT		10
#define MAX_PORT_SIZE 	65535
#define MAX_PAYLOAD 	1500
#define	MAX_PORTS		100


// SHARED PORTS
typedef struct Port {
    u_int16_t	local_port;
    u_int16_t	remote_port;
} Port;
// SHARED PORT ARRAY
typedef struct Ports {
    struct Port port[MAX_PORTS];	// 100 ports max
    int size;
} Ports;

// ETHERNET FRAME PACKET
typedef struct Ethernet_Frame {
    u_int16_t   eth_type;               // 2 bytes
    u_int16_t   payload_size;          	// store the size to let other programs know how big the payload is
    //u_int8_t    *payload;            	// variable value ranging from 0-1500. this must be malloced
    u_int8_t	payload[1500];
    
    u_int8_t	local_mac_addr[6];		// 6 bytes or 48 bits
    u_int8_t	remote_mac_addr[6];		// 6 bytes or 48 bits
    u_int16_t	src_port;
} Ethernet_Frame;

// SHARED MAC ADDRESS AND PORT MAPPING
typedef struct  Mac_Addr{
    u_int16_t 	src_port;
    u_int8_t	src[6];
    //u_int8_t	dst[6];
} Mac_Addr;

// SHARED FORWARDING TABLE
typedef struct ForwardingTable {
    //u_int16_t 	port;
    //u_int8_t	mac_addr[6];
    struct Mac_Addr	entry[10000];
    int size;
    int transaction;
} ForwardingTable;

// SHARED LOCKS
typedef struct Lock {
    pthread_mutex_t mutex;
    pthread_cond_t 	cond;
    int 			key;
} Lock;

// THREAD ARGUMENTS
struct FunctionArgs {
    struct ForwardingTable 	*forwarding_table;	// can contain at least 10000 entries
    struct Lock 			*lock;
    struct Ports			*ports;				// every thread knows all the ports	
    int 					id;					// thread id to identify which thread is working
} FunctionArgs;

// GET KEY
void * getKey(struct FunctionArgs * funcArgs) {
    
    pthread_mutex_lock(&(funcArgs->lock->mutex));
    while (funcArgs->lock->key == 0) {
        printf("Thread %d doesn't see a key for a lock and sleeps\n", funcArgs->id);
        pthread_cond_wait(&(funcArgs->lock->cond),&(funcArgs->lock->mutex));
    }
    (funcArgs->lock->key)--;
    pthread_mutex_unlock(&(funcArgs->lock->mutex));
    
    // if obtained key use bathroom
    printf("Thread %d acquires lock\n", funcArgs->id);
    return  0;
}

// RETURN KEY
void * returnKey(struct FunctionArgs * funcArgs) {
    pthread_mutex_lock(&(funcArgs->lock->mutex));
    if (funcArgs->lock->key < 1) {
        funcArgs->lock->key++;
        printf("Thread %d releases lock\n", funcArgs->id);
        pthread_cond_signal(&(funcArgs->lock->cond));    // wake up a sleeping thread
    } else {
        printf("Error: Too many locks\n");       // should not go here
        exit(1);
    }
    pthread_mutex_unlock(&(funcArgs->lock->mutex));
    return 0;
}

// PRINT BUFFER
void print_buffer(char * buffer, int num_bytes) {
    int i = 0;
    for (i = 0; i < num_bytes; i++) {
        printf("%x ", (*buffer & 0xff)); // turn off unnecessary bits with 0xff mask
        buffer++;
    }
    printf("\n");   // newline
}

/*
 // SIGACTION ERROR PRINT
 void handler(int signum) {
	printf("Error: Did not receive response within %d seconds\n", WAIT_TIME);	// check status of "errno"
 }
 
 // SET ALARM
 void setAlarm(int time) {
	// SET ALARM
	struct sigaction *action = (struct sigaction *)malloc(sizeof(struct sigaction));
	action->sa_handler = handler;
	sigaction(SIGALRM, action, NULL);
	alarm(time);	// alarm in seconds
	free(action);
 }*/


void * listenThread(void *args) {
    struct FunctionArgs *funcArgs =(struct FunctionArgs *)args;
    
    // CREATE A CONNECTION FOR LISTENING
    int af = AF_INET6;	// address family
    int sockfd = 0;
    int port = 0;
    
    // assign a local port
    port = funcArgs->ports->port[funcArgs->id].local_port;
    
    struct sockaddr_in6 sin6_receiver, sin6_sender;
    bzero(&sin6_receiver, sizeof(sin6_receiver));	// zero out
    sin6_receiver.sin6_family = af;
    sin6_receiver.sin6_port = htons(port);
    
    // CONVERT IPV6 LOOPBACK ADDRESS STRING TO NETWORK BINARY TO STORE IN STRUCT
    char * ipv6_loopBackAddr = "::1";	// LOOP BACK ADDRESS
    if (inet_pton (AF_INET6, ipv6_loopBackAddr, &sin6_receiver.sin6_addr) != 1) {
        //printf ("usage: %s udp-port ipv6-source ipv6-dest\n", argv [0]);
        //printf ("       source address %s is not a valid IPv6 address", argv [2]);
        printf("Error: Cannot connect to IPv4 loop back address\n");
        exit (1);
    }
    
    int err = 0;
    
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
    if (PRINT) {printf("Listening to port: %d\n", port);}
    
    // KEEP LISTENING FOR A GIVEN ROUTE AND DETERMINE WHICH ROUTE TO GO IN THE LOOKUP TABLE
    int len = sizeof(struct Ethernet_Frame);
    char packet[len];
    int recv_len = 0;
    Ethernet_Frame * h = (Ethernet_Frame *)malloc(len);
    
    while (1) {	// KEEP LISTENING
        printf("Listening on port: %d...\n", port);
        fflush(stdout);
        
        
        // RECEIVE ETHERNET FRAME PACKET FROM SENDER        
        int sin6_size = sizeof(sin6_sender);
        if ((recv_len = recvfrom(sockfd, packet, len, 0, (struct sockaddr *)&sin6_sender, (socklen_t*)&sin6_size)) < 0) {
            printf("Error: Did not receive response (IPv6 address) with error code %d\n", recv_len);
            continue;	// keep looping
        }

        
        // SHOW ETHERNET PACKET CONTENT
		//Ethernet_Frame * h = (Ethernet_Frame *)malloc(len);
        h = (Ethernet_Frame *)packet;
       
		printf("Ethernet Frame Packet Content:\n");
		printf("\tEthernet Type:\t%#x\n", h->eth_type);
		//printf("\tPayload Size:\t%d\n", h->payload_size);
       
        
        // CONVERT LOCAL AND REMOTE MAC ADDRESS BACK TO DOTTED FORMAT FOR PRINTING
        char local_mac_str[12];
        sprintf(local_mac_str, "%d:%d:%d:%d:%d:%d", h->local_mac_addr[0] & 0xff, h->local_mac_addr[1] & 0xff,h->local_mac_addr[2] & 0xff, h->local_mac_addr[3] & 0xff, h->local_mac_addr[4] & 0xff,h->local_mac_addr[5] & 0xff);
		printf("\tLocal MAC:\t%s\n", local_mac_str);
        char remote_mac_str[12];
        sprintf(remote_mac_str, "%d:%d:%d:%d:%d:%d", h->remote_mac_addr[0] & 0xff, h->remote_mac_addr[1] & 0xff,h->remote_mac_addr[2] & 0xff, h->remote_mac_addr[3] & 0xff, h->remote_mac_addr[4] & 0xff,h->remote_mac_addr[5] & 0xff);
		printf("\tRemote MAC:\t%s\n", remote_mac_str);
        
		printf("\tPayload:\n");
        printf("\t");
        int p = 0;
        for (p = 0; p < h->payload_size; p++) {
            if ((p % 50) == 0) {
				printf("\n");	// new line
				printf("\t");	// tab
            }
			printf("%d", h->payload[p]);
        }
		printf("\n");	// new line
     

        // GET THE KEY TO ACQUIRE A LOCK
        getKey(funcArgs);
	
        int f = 0;
        int x = 0;
        int found_index = 0;
        int outgoingPort = 0;
        
		funcArgs->forwarding_table->transaction += 1;	// keep track of transactions
        
		// TABLE LOOK UP
        if (funcArgs->forwarding_table->size == 0) {	// if there is no entry in the table
			
			// AT THIS POINT THERE IS NO OTHER DESTINATION TO SEND TO SO DROP THE PACKET
			char remote_mac_str[12];
			sprintf(remote_mac_str, "%d:%d:%d:%d:%d:%d", h->remote_mac_addr[0] & 0xff, h->remote_mac_addr[1] & 0xff,h->remote_mac_addr[2] & 0xff, h->remote_mac_addr[3] & 0xff, h->remote_mac_addr[4] & 0xff,h->remote_mac_addr[5] & 0xff);
			printf("Initial MAC destination %s not learned yet. Dropping packet.\n",  remote_mac_str);
			memset(remote_mac_str, '\0', sizeof(remote_mac_str));	// reset buff

						
			// NO DESTINATION FOUND BUT STILL SAVE THE SOURCE MAC AND PORT (INTERFACE) PAIR	
			int j = 0;
			for (j = 0; j < 6; j++) {
				funcArgs->forwarding_table->entry[0].src[j] = h->local_mac_addr[j];
				//funcArgs->forwarding_table->entry[0].dst[j] = h->remote_mac_addr[j];	// also extract the destination to find
			}
			if (PRINT) {printf("Added first entry to table:\n");}
			
			// get source port
			funcArgs->forwarding_table->entry[0].src_port = h->src_port;
			if (PRINT) {printf("\tSource port:\t%d\n",funcArgs->forwarding_table->entry[0].src_port);}
			
			// get source mac address
			char local_mac_str[12];
			sprintf(local_mac_str, "%d:%d:%d:%d:%d:%d", funcArgs->forwarding_table->entry[0].src[0] & 0xff, funcArgs->forwarding_table->entry[0].src[1] & 0xff, funcArgs->forwarding_table->entry[0].src[2] & 0xff, funcArgs->forwarding_table->entry[0].src[3] & 0xff, funcArgs->forwarding_table->entry[0].src[4] & 0xff, funcArgs->forwarding_table->entry[0].src[5] & 0xff);
			if (PRINT) {printf("\tSource Mac:\t%s\n", local_mac_str);}
			memset(local_mac_str, '\0', sizeof(local_mac_str));	// reset buff
			
			(funcArgs->forwarding_table->size)++;	// increment size of table 	
			
			
			// BROAD CAST TO ALL OTHER PORTS
			int listeningPort = funcArgs->ports->port[funcArgs->id].local_port;
			int z = 0;	// reset  counter
			for (z = 0; z < funcArgs->ports->size; z++) {
				int op = funcArgs->ports->port[z].remote_port;
				//int p2 = funcArgs->ports[z].remote_port

				if (op != outgoingPort || op != listeningPort) {
					// ESTABLISH REMOTE UDP CONNECTION USING IPV6 ADDRESS TO SEND MESSAGE
					 int af_remote = AF_INET6;	// address family
					 int sockfd_remote = 0;
					 struct sockaddr_in6 sin6_remote;
					 bzero(&sin6_remote, sizeof(sin6_remote));	// zero out
					 sin6_remote.sin6_family = af_remote;
					 sin6_remote.sin6_port = htons(op);
					 
					 
					 // CONVERSION FROM IPV6 STRING TO BINARY ALREADY HAPPENED ABOVE, SIMPLY COPY TO STRUCT
					 char * ipv6_loopBackAddr = "::1";	// LOOP BACK ADDRESS
					 if (inet_pton (AF_INET6, ipv6_loopBackAddr, &sin6_remote.sin6_addr) != 1) {
						printf("Error: Could not establish loopback address ::1\n");
						exit (1);
					 }
					 
					 // CREATE SOCKET FOR IPv6
					 if((sockfd_remote = socket(af_remote, SOCK_DGRAM, 0)) < 0){
						printf("Error: Could not open socket\n");
						exit(1);
					 }
					 if (PRINT) {printf("Success: Sending socket created for IPv6 address\n");}
					 
					 // CONNECT
					 if (connect (sockfd_remote, (struct sockaddr *)&sin6_remote, sizeof(sin6_remote)) == -1){
						printf ("Error: Could not establish connection\n");
						exit(1);
					 }
					 if (PRINT) {printf("Success: Connected to remote_port %d\n", funcArgs->forwarding_table->entry[z].src_port);}
					 //if (PRINT) {printf ("Success: Connect to remote port %d on interface %s\n", remote_port, funcArgs->interface[z].interface);}
					 
					 
					 // JUST SEND THE ORIGINAL PACKET
					 int err = 0;
					 if ((err = sendto (sockfd_remote, packet, sizeof(packet), 0, (struct sockaddr *)&sin6_remote, sizeof(sin6_remote))) == -1) { 		
							char src_buff[12];
							sprintf(src_buff, "%d:%d:%d:%d:%d:%d", funcArgs->forwarding_table->entry[z].src[0] & 0xff, funcArgs->forwarding_table->entry[z].src[1] & 0xff, funcArgs->forwarding_table->entry[z].src[2] & 0xff, funcArgs->forwarding_table->entry[z].src[3] & 0xff, funcArgs->forwarding_table->entry[z].src[4] & 0xff, funcArgs->forwarding_table->entry[z].src[5] & 0xff);			
							printf ("Error: Could NOT send file to MAC %s with error code [%d]\n", src_buff, err);
							exit(1);
					 }
					printf("%d bytes ", h->payload_size);
					char src_mac_buff[12];
					sprintf(src_mac_buff, "%d:%d:%d:%d:%d:%d", h->local_mac_addr[0] & 0xff, h->local_mac_addr[1] & 0xff, h->local_mac_addr[2] & 0xff, h->local_mac_addr[3] & 0xff, h->local_mac_addr[4] & 0xff, h->local_mac_addr[5] & 0xff);
					printf("%s to ", src_mac_buff);
					char remote_mac_buff[12];
					sprintf(remote_mac_buff, "%d:%d:%d:%d:%d:%d", h->remote_mac_addr[0] & 0xff, h->remote_mac_addr[1] & 0xff,h->remote_mac_addr[2] & 0xff, h->remote_mac_addr[3] & 0xff, h->remote_mac_addr[4] & 0xff,h->remote_mac_addr[5] & 0xff);
					printf("%s, port %d, broadcasting\n", remote_mac_buff, op);
					
					memset(src_mac_buff, '\0', sizeof(src_mac_buff));	// reset buff
					memset(remote_mac_buff, '\0', sizeof(remote_mac_buff));	// reset buff
				} else {
					// interface are equal so drop the packet
					printf("%d bytes ", h->payload_size);
					char src_mac_buff[12];
					sprintf(src_mac_buff, "%d:%d:%d:%d:%d:%d", h->local_mac_addr[0] & 0xff, h->local_mac_addr[1] & 0xff, h->local_mac_addr[2] & 0xff, h->local_mac_addr[3] & 0xff, h->local_mac_addr[4] & 0xff, h->local_mac_addr[5] & 0xff);
					printf("%s to ", src_mac_buff);
					char remote_mac_buff[12];
					sprintf(remote_mac_buff, "%d:%d:%d:%d:%d:%d", h->remote_mac_addr[0] & 0xff, h->remote_mac_addr[1] & 0xff,h->remote_mac_addr[2] & 0xff, h->remote_mac_addr[3] & 0xff, h->remote_mac_addr[4] & 0xff,h->remote_mac_addr[5] & 0xff);
					printf("%s, port %d (same), dropping\n", remote_mac_buff, op);
					
					memset(src_mac_buff, '\0', sizeof(src_mac_buff));	// reset buff
					memset(remote_mac_buff, '\0', sizeof(remote_mac_buff));	// reset buf;	
				}	
			}	
		} else {
			
			// FIND THE MAC DESTINATION ENTRY IN THE TABLE (THE MAC SOURCES THAT HAVE PORT PAIRS)
			for (f = 0; f < (funcArgs->forwarding_table->size)-1; f++) {
				for (x = 0; x < 6; x++) {
					//printf("comparing [%d][%d]\n", funcArgs->forwarding_table->entry[f].src[x], h->remote_mac_addr[x]);
					if (funcArgs->forwarding_table->entry[f].src[x] == h->remote_mac_addr[x]) {
						found_index = f;	// found the destination to send packet to in table
					} else {
						found_index = -1;	// reset
						break;
					}
				}
			}	
   
			if (found_index == -1) {	// entry not found so drop packet
				char remote_mac_str[12];
				sprintf(remote_mac_str, "%d:%d:%d:%d:%d:%d", h->remote_mac_addr[0] & 0xff, h->remote_mac_addr[1] & 0xff,h->remote_mac_addr[2] & 0xff, h->remote_mac_addr[3] & 0xff, h->remote_mac_addr[4] & 0xff,h->remote_mac_addr[5] & 0xff);
				printf("MAC destination %s not learned yet. Broadcasting.\n",  remote_mac_str);
				memset(remote_mac_str, '\0', sizeof(remote_mac_str));	// reset buff
				

				// RECORD THE SOURCE MAC (AT THIS POINT YOU DID NOT FIND DESTINATION ON THE TABLE BUT YOU LEARNED THE SOURCE MAC)		
				int index = funcArgs->forwarding_table->size;
				int j = 0;
				for (j = 0; j < 6; j++) {
					funcArgs->forwarding_table->entry[index].src[j] = h->local_mac_addr[j];
				}
				funcArgs->forwarding_table->entry[index].src_port = h->src_port;
				if (PRINT) {printf("Added source MAC and port to table:\n");}
				
				// get source port
				if (PRINT) {printf("\tSource port:\t\t%d\n",funcArgs->forwarding_table->entry[index].src_port);}
				
				// get source mac address
				char local_mac_str[12];
				sprintf(local_mac_str, "%d:%d:%d:%d:%d:%d", funcArgs->forwarding_table->entry[index].src[0] & 0xff, funcArgs->forwarding_table->entry[index].src[1] & 0xff, funcArgs->forwarding_table->entry[index].src[2] & 0xff, funcArgs->forwarding_table->entry[index].src[3] & 0xff, funcArgs->forwarding_table->entry[index].src[4] & 0xff, funcArgs->forwarding_table->entry[index].src[5] & 0xff);			
				if (PRINT) {printf("\tSource Mac:\t\t%s\n", local_mac_str);}
				memset(local_mac_str, '\0', sizeof(local_mac_str));	// reset buff
								
								
				(funcArgs->forwarding_table->size)++;	// increment size
					
				// BROAD CAST TO ALL OTHER PORTS
				int listeningPort = funcArgs->ports->port[funcArgs->id].local_port;
				int z = 0;	
				for (z = 0; z < funcArgs->forwarding_table->size; z++) {					
					int op = funcArgs->forwarding_table->entry[z].src_port;
					//int op = funcArgs->ports->port[z].remote_port;
					if ((op != listeningPort) && (op != h->src_port)) {
						// ESTABLISH REMOTE UDP CONNECTION USING IPV6 ADDRESS TO SEND MESSAGE
						 int af_remote = AF_INET6;	// address family
						 int sockfd_remote = 0;
						 struct sockaddr_in6 sin6_remote;
						 bzero(&sin6_remote, sizeof(sin6_remote));	// zero out
						 sin6_remote.sin6_family = af_remote;
						 sin6_remote.sin6_port = htons(op);
						 
						 
						 // CONVERSION FROM IPV6 STRING TO BINARY ALREADY HAPPENED ABOVE, SIMPLY COPY TO STRUCT
						 char * ipv6_loopBackAddr = "::1";	// LOOP BACK ADDRESS
						 if (inet_pton (AF_INET6, ipv6_loopBackAddr, &sin6_remote.sin6_addr) != 1) {
						 printf("Error: Could not establish loopback address ::1\n");
						 exit (1);
						 }
						 
						 // CREATE SOCKET FOR IPv6
						 if((sockfd_remote = socket(af_remote, SOCK_DGRAM, 0)) < 0){
							printf("Error: Could not open socket\n");
							exit(1);
						 }
						 if (PRINT) {printf("Success: Sending socket created for IPv6 address\n");}
						 
						 // CONNECT
						 if (connect (sockfd_remote, (struct sockaddr *)&sin6_remote, sizeof(sin6_remote)) == -1){
							printf ("Error: Could not establish connection\n");
							exit(1);
						 }
						 if (PRINT) {printf("Success: Connected to remote_port %d\n", funcArgs->forwarding_table->entry[z].src_port);}
						 //if (PRINT) {printf ("Success: Connect to remote port %d on interface %s\n", remote_port, funcArgs->interface[z].interface);}
						 
						 
						 // JUST SEND THE ORIGINAL PACKET
						 int err = 0;
						 if ((err = sendto (sockfd_remote, packet, sizeof(packet), 0, (struct sockaddr *)&sin6_remote, sizeof(sin6_remote))) == -1) { 		
								char src_buff[12];
								sprintf(src_buff, "%d:%d:%d:%d:%d:%d", funcArgs->forwarding_table->entry[z].src[0] & 0xff, funcArgs->forwarding_table->entry[z].src[1] & 0xff, funcArgs->forwarding_table->entry[z].src[2] & 0xff, funcArgs->forwarding_table->entry[z].src[3] & 0xff, funcArgs->forwarding_table->entry[z].src[4] & 0xff, funcArgs->forwarding_table->entry[z].src[5] & 0xff);			
								printf ("Error: Could NOT send file to MAC %s with error code [%d]\n", src_buff, err);
								exit(1);
						 }
						printf("%d bytes ", h->payload_size);
						char src_mac_buff[12];
						sprintf(src_mac_buff, "%d:%d:%d:%d:%d:%d", h->local_mac_addr[0] & 0xff, h->local_mac_addr[1] & 0xff, h->local_mac_addr[2] & 0xff, h->local_mac_addr[3] & 0xff, h->local_mac_addr[4] & 0xff, h->local_mac_addr[5] & 0xff);
						printf("%s to ", src_mac_buff);
						char remote_mac_buff[12];
						sprintf(remote_mac_buff, "%d:%d:%d:%d:%d:%d", h->remote_mac_addr[0] & 0xff, h->remote_mac_addr[1] & 0xff,h->remote_mac_addr[2] & 0xff, h->remote_mac_addr[3] & 0xff, h->remote_mac_addr[4] & 0xff,h->remote_mac_addr[5] & 0xff);
						printf("%s, port %d, broadcasting\n", remote_mac_buff, op);
						
						memset(src_mac_buff, '\0', sizeof(src_mac_buff));	// reset buff
						memset(remote_mac_buff, '\0', sizeof(remote_mac_buff));	// reset buff	

					} else {
						printf("%d bytes ", h->payload_size);
						char src_mac_buff[12];
						sprintf(src_mac_buff, "%d:%d:%d:%d:%d:%d", h->local_mac_addr[0] & 0xff, h->local_mac_addr[1] & 0xff, h->local_mac_addr[2] & 0xff, h->local_mac_addr[3] & 0xff, h->local_mac_addr[4] & 0xff, h->local_mac_addr[5] & 0xff);
						printf("%s to ", src_mac_buff);
						char remote_mac_buff[12];
						sprintf(remote_mac_buff, "%d:%d:%d:%d:%d:%d", h->remote_mac_addr[0] & 0xff, h->remote_mac_addr[1] & 0xff,h->remote_mac_addr[2] & 0xff, h->remote_mac_addr[3] & 0xff, h->remote_mac_addr[4] & 0xff,h->remote_mac_addr[5] & 0xff);
						printf("%s, port %d (same), dropping\n", remote_mac_buff, op);
						
						memset(src_mac_buff, '\0', sizeof(src_mac_buff));	// reset buff
						memset(remote_mac_buff, '\0', sizeof(remote_mac_buff));	// reset buff	
					}
				}
			
			} else {
				// RECORD THE PORT NUMBER OF THE DESTINATION MAC THAT WAS FOUND (THIS IS THE PORT YOU SEND THE PACKET TO)
				outgoingPort = funcArgs->forwarding_table->entry[found_index].src_port;
				char remote_mac_str[12];
				sprintf(remote_mac_str, "%d:%d:%d:%d:%d:%d", funcArgs->forwarding_table->entry[found_index].src[0] & 0xff, funcArgs->forwarding_table->entry[found_index].src[1] & 0xff, funcArgs->forwarding_table->entry[found_index].src[2] & 0xff, funcArgs->forwarding_table->entry[found_index].src[3] & 0xff, funcArgs->forwarding_table->entry[found_index].src[4] & 0xff, funcArgs->forwarding_table->entry[found_index].src[5] & 0xff);
				printf("Found Mac destination %s to port %d on table\n", remote_mac_str, outgoingPort);
				memset(remote_mac_str, '\0', sizeof(remote_mac_str));	// reset buff
				
				
		
				//********************************************************************************************
				// FIND THE SOURCE MAC ENTRY IN THE TABLE
				f 			= 0;	// reset
				x 			= 0;	// reset
				found_index = 0;	// reset
				for (f = 0; f < (funcArgs->forwarding_table->size); f++) {
					for (x = 0; x < 6; x++) {
						//printf("comparing [%d][%d]\n", funcArgs->forwarding_table->entry[f].src[x], h->local_mac_addr[x]);
						if (funcArgs->forwarding_table->entry[f].src[x] == h->local_mac_addr[x]) {
							found_index = f;	// found the destination to send packet to in table
						} else {
							found_index = -1;	// reset
							break;
						}
					}
				}
				
				// IF FOUND SOURCE MAC, UPDATE THE PORT NUMBER
				// IF SOURCE MAC NOT FOUND, ADD SOURCE MAC AND PORT NUMBER PAIR TO TABLE    
				if (found_index == -1) {	// entry not found so add it to table			
					int index = funcArgs->forwarding_table->size;
					int j = 0;
					for (j = 0; j < 6; j++) {
						funcArgs->forwarding_table->entry[index].src[j] = h->local_mac_addr[j];
						//funcArgs->forwarding_table->entry[index].dst[j] = h->remote_mac_addr[j];	// also extract the destination to find
					}
					if (PRINT) {printf("Added new source Mac entry to table:\n");}
					
					// get source port
					funcArgs->forwarding_table->entry[index].src_port = h->src_port;
					if (PRINT) {printf("\tSource port:\t\t%d\n",funcArgs->forwarding_table->entry[index].src_port);}
					
					// get source mac address
					char local_mac_str[12];
					sprintf(local_mac_str, "%d:%d:%d:%d:%d:%d", funcArgs->forwarding_table->entry[index].src[0] & 0xff, funcArgs->forwarding_table->entry[index].src[1] & 0xff, funcArgs->forwarding_table->entry[index].src[2] & 0xff, funcArgs->forwarding_table->entry[index].src[3] & 0xff, funcArgs->forwarding_table->entry[index].src[4] & 0xff, funcArgs->forwarding_table->entry[index].src[5] & 0xff);			
					if (PRINT) {printf("\tSource Mac:\t\t%s\n", local_mac_str);}
					memset(local_mac_str, '\0', sizeof(local_mac_str));	// reset buff
					
					(funcArgs->forwarding_table->size)++;	// increment size of table 
				} else {
					
					// SOURCE MAC ENTRY IS FOUND SO UPDATE IT'S SOURCE PORT
					int index = found_index;
					if (PRINT) {
						printf("Source Mac entry found for transaction %d\n", funcArgs->forwarding_table->transaction);
						printf("Updating source port %d to", funcArgs->forwarding_table->entry[index].src_port);
					}
					funcArgs->forwarding_table->entry[index].src_port = h->src_port;	// change source port for Source Mac
					
					if (PRINT) {
						printf(" %d\n", funcArgs->forwarding_table->entry[index].src_port);
						printf("Port\tSource Mac\n");
						printf("%d", funcArgs->forwarding_table->entry[index].src_port);
					}

					char src_buff[12];
					sprintf(src_buff, "%d:%d:%d:%d:%d:%d", funcArgs->forwarding_table->entry[index].src[0] & 0xff, funcArgs->forwarding_table->entry[index].src[1] & 0xff, funcArgs->forwarding_table->entry[index].src[2] & 0xff, funcArgs->forwarding_table->entry[index].src[3] & 0xff, funcArgs->forwarding_table->entry[index].src[4] & 0xff, funcArgs->forwarding_table->entry[index].src[5] & 0xff);			
					if (PRINT) {printf("\t%s\n", src_buff);}
					memset(src_buff, '\0', sizeof(src_buff));	// reset buff

		  
				}
				//********************************************************************************
				
				
				
				// SEND PACKET TO THE INTERFACE THAT IS PAIRED WITH THE MAC ADDRESS THAT WAS FOUND
				// ESTABLISH REMOTE UDP CONNECTION USING IPV6 ADDRESS TO SEND MESSAGE
				 int af_remote = AF_INET6;	// address family
				 int sockfd_remote = 0;
				 struct sockaddr_in6 sin6_remote;
				 bzero(&sin6_remote, sizeof(sin6_remote));	// zero out
				 sin6_remote.sin6_family = af_remote;
				 sin6_remote.sin6_port = htons(outgoingPort);
				 
				 
				 // CONVERSION FROM IPV6 STRING TO BINARY ALREADY HAPPENED ABOVE, SIMPLY COPY TO STRUCT
				 char * ipv6_loopBackAddr = "::1";	// LOOP BACK ADDRESS
				 if (inet_pton (AF_INET6, ipv6_loopBackAddr, &sin6_remote.sin6_addr) != 1) {
					printf("Error: Could not establish loopback address ::1\n");
					exit (1);
				 }
				 
				 // CREATE SOCKET FOR IPv6
				 if((sockfd_remote = socket(af_remote, SOCK_DGRAM, 0)) < 0){
					printf("Error: Could not open socket\n");
					exit(1);
				 }
				 if (PRINT) {printf("Success: Sending socket created for IPv6 address\n");}
				 
				 // CONNECT
				 if (connect (sockfd_remote, (struct sockaddr *)&sin6_remote, sizeof(sin6_remote)) == -1){
					printf ("Error: Could not establish connection\n");
					exit(1);
				 }
				 if (PRINT) {printf("Success: Connected to remote_port %d\n", outgoingPort);}
				 //if (PRINT) {printf ("Success: Connect to remote port %d on interface %s\n", remote_port, funcArgs->interface[z].interface);}
				 
				 
				 // JUST SEND THE ORIGINAL PACKET
				 int err = 0;
				 if ((err = sendto (sockfd_remote, packet, sizeof(packet), 0, (struct sockaddr *)&sin6_remote, sizeof(sin6_remote))) == -1) { 		
					//char src_buff[12];
					//sprintf(src_buff, "%d:%d:%d:%d:%d:%d", funcArgs->forwarding_table->entry[z].src[0] & 0xff, funcArgs->forwarding_table->entry[z].src[1] & 0xff, funcArgs->forwarding_table->entry[z].src[2] & 0xff, funcArgs->forwarding_table->entry[z].src[3] & 0xff, funcArgs->forwarding_table->entry[z].src[4] & 0xff, funcArgs->forwarding_table->entry[z].src[5] & 0xff);			
					//printf ("Error: Could NOT send file to MAC %s with error code [%d]\n", src_buff, err);
					printf ("Error: Could NOT send packet\n");
					exit(1);
				 }
				
				//**********************************************************************************************
				printf("%d bytes ", h->payload_size);
				char src_mac_buff[12];
				sprintf(src_mac_buff, "%d:%d:%d:%d:%d:%d", h->local_mac_addr[0] & 0xff, h->local_mac_addr[1] & 0xff, h->local_mac_addr[2] & 0xff, h->local_mac_addr[3] & 0xff, h->local_mac_addr[4] & 0xff, h->local_mac_addr[5] & 0xff);
				printf("%s to ", src_mac_buff);
				char remote_mac_buff[12];
				sprintf(remote_mac_buff, "%d:%d:%d:%d:%d:%d", h->remote_mac_addr[0] & 0xff, h->remote_mac_addr[1] & 0xff,h->remote_mac_addr[2] & 0xff, h->remote_mac_addr[3] & 0xff, h->remote_mac_addr[4] & 0xff,h->remote_mac_addr[5] & 0xff);
				printf("%s, port %d, forwarding to port %d\n", remote_mac_buff, funcArgs->ports->port[funcArgs->id].local_port, outgoingPort);
				
				
				memset(src_mac_buff, '\0', sizeof(src_mac_buff));	// reset buff
				memset(remote_mac_buff, '\0', sizeof(remote_mac_buff));	// reset buff
				//**********************************************************************************************
				
				// BROAD CAST TO ALL OTHER PORTS
				int listeningPort = funcArgs->ports->port[funcArgs->id].local_port;
				int z = 0;	
				for (z = 0; z < funcArgs->forwarding_table->size; z++) {					
					int op = funcArgs->forwarding_table->entry[z].src_port;
					//int op = funcArgs->ports->port[z].remote_port;

					if ((op != listeningPort) && (op != h->src_port)) {
						// ESTABLISH REMOTE UDP CONNECTION USING IPV6 ADDRESS TO SEND MESSAGE
						 int af_remote = AF_INET6;	// address family
						 int sockfd_remote = 0;
						 struct sockaddr_in6 sin6_remote;
						 bzero(&sin6_remote, sizeof(sin6_remote));	// zero out
						 sin6_remote.sin6_family = af_remote;
						 sin6_remote.sin6_port = htons(op);
						 
						 
						 // CONVERSION FROM IPV6 STRING TO BINARY ALREADY HAPPENED ABOVE, SIMPLY COPY TO STRUCT
						 char * ipv6_loopBackAddr = "::1";	// LOOP BACK ADDRESS
						 if (inet_pton (AF_INET6, ipv6_loopBackAddr, &sin6_remote.sin6_addr) != 1) {
						 printf("Error: Could not establish loopback address ::1\n");
						 exit (1);
						 }
						 
						 // CREATE SOCKET FOR IPv6
						 if((sockfd_remote = socket(af_remote, SOCK_DGRAM, 0)) < 0){
							printf("Error: Could not open socket\n");
							exit(1);
						 }
						 if (PRINT) {printf("Success: Sending socket created for IPv6 address\n");}
						 
						 // CONNECT
						 if (connect (sockfd_remote, (struct sockaddr *)&sin6_remote, sizeof(sin6_remote)) == -1){
							printf ("Error: Could not establish connection\n");
							exit(1);
						 }
						 if (PRINT) {printf("Success: Connected to remote_port %d\n", funcArgs->forwarding_table->entry[z].src_port);}
						 //if (PRINT) {printf ("Success: Connect to remote port %d on interface %s\n", remote_port, funcArgs->interface[z].interface);}
						 
						 
						 // JUST SEND THE ORIGINAL PACKET
						 int err = 0;
						 if ((err = sendto (sockfd_remote, packet, sizeof(packet), 0, (struct sockaddr *)&sin6_remote, sizeof(sin6_remote))) == -1) { 		
								char src_buff[12];
								sprintf(src_buff, "%d:%d:%d:%d:%d:%d", funcArgs->forwarding_table->entry[z].src[0] & 0xff, funcArgs->forwarding_table->entry[z].src[1] & 0xff, funcArgs->forwarding_table->entry[z].src[2] & 0xff, funcArgs->forwarding_table->entry[z].src[3] & 0xff, funcArgs->forwarding_table->entry[z].src[4] & 0xff, funcArgs->forwarding_table->entry[z].src[5] & 0xff);			
								printf ("Error: Could NOT send file to MAC %s with error code [%d]\n", src_buff, err);
								exit(1);
						 }
						 											
						printf("%d bytes ", h->payload_size);
						char src_mac_buff[12];
						sprintf(src_mac_buff, "%d:%d:%d:%d:%d:%d", h->local_mac_addr[0] & 0xff, h->local_mac_addr[1] & 0xff, h->local_mac_addr[2] & 0xff, h->local_mac_addr[3] & 0xff, h->local_mac_addr[4] & 0xff, h->local_mac_addr[5] & 0xff);
						printf("%s to ", src_mac_buff);
						char remote_mac_buff[12];
						sprintf(remote_mac_buff, "%d:%d:%d:%d:%d:%d", h->remote_mac_addr[0] & 0xff, h->remote_mac_addr[1] & 0xff,h->remote_mac_addr[2] & 0xff, h->remote_mac_addr[3] & 0xff, h->remote_mac_addr[4] & 0xff,h->remote_mac_addr[5] & 0xff);
						printf("%s, port %d, broadcasting\n", remote_mac_buff, op);
						
						memset(src_mac_buff, '\0', sizeof(src_mac_buff));	// reset buff
						memset(remote_mac_buff, '\0', sizeof(remote_mac_buff));	// reset buff		
															
										
					} else {
						// interface are equal so drop the packet
						printf("%d bytes ", h->payload_size);
						char src_mac_buff[12];
						sprintf(src_mac_buff, "%d:%d:%d:%d:%d:%d", h->local_mac_addr[0] & 0xff, h->local_mac_addr[1] & 0xff, h->local_mac_addr[2] & 0xff, h->local_mac_addr[3] & 0xff, h->local_mac_addr[4] & 0xff, h->local_mac_addr[5] & 0xff);
						printf("%s to ", src_mac_buff);
						char remote_mac_buff[12];
						sprintf(remote_mac_buff, "%d:%d:%d:%d:%d:%d", h->remote_mac_addr[0] & 0xff, h->remote_mac_addr[1] & 0xff,h->remote_mac_addr[2] & 0xff, h->remote_mac_addr[3] & 0xff, h->remote_mac_addr[4] & 0xff,h->remote_mac_addr[5] & 0xff);
						printf("%s, port %d (same), dropping\n", remote_mac_buff, op);
						
						memset(src_mac_buff, '\0', sizeof(src_mac_buff));	// reset buff
						memset(remote_mac_buff, '\0', sizeof(remote_mac_buff));	// reset buff
					}
;
					
					

				}	
			}

		}
		
		// RETURN THE KEY SO OTHER THREADS CAN USE IT
        returnKey(funcArgs);
		
		
		// PRINT OUT ALL TABLE ENTRIES
		int z = 0;
		printf("All Table Entries:\nPort\tSource Mac\n");
		for (z = 0; z < funcArgs->forwarding_table->size; z++) {
			int index = z;
			printf("%d", funcArgs->forwarding_table->entry[index].src_port);

			char src_buff[12];
			sprintf(src_buff, "%d:%d:%d:%d:%d:%d", funcArgs->forwarding_table->entry[index].src[0] & 0xff, funcArgs->forwarding_table->entry[index].src[1] & 0xff, funcArgs->forwarding_table->entry[index].src[2] & 0xff, funcArgs->forwarding_table->entry[index].src[3] & 0xff, funcArgs->forwarding_table->entry[index].src[4] & 0xff, funcArgs->forwarding_table->entry[index].src[5] & 0xff);			
			printf("\t%s\n", src_buff);
		}
				         
		// reset buffers and free memory
		memset(local_mac_str, '\0', sizeof(local_mac_str));
		memset(remote_mac_str, '\0', sizeof(remote_mac_str));
		memset(packet, '\0', sizeof(packet));
    }// END OF LISTENING LOOP
}


// MAIN
int main (int argc, char ** argv) {
    
    // PARSE PORTS FROM COMMANDLINE WHICH AS THE FORM localUDPPort/remoteUDPPort
    int j = 0;
    int numInterface = argc-1;	// number of localUDPPort/remoteUDPPort pairings (the interface)
    struct Ports * ports = (struct Ports *)malloc(sizeof(struct Ports));
    ports->size = 0;
    for (j = 0; j < numInterface; j++) {
        int i = 0;		// counter
        int found_slash = 0;
        int remote_port_index = 0;
        char localUDPPort_buff[MAX_PORT_SIZE];
        char remoteUDPPort_buff[MAX_PORT_SIZE];
        while (argv[j+1][i] != '\0') {
            if (argv[j+1][i] == '/') {	// slash found
                found_slash = 1;
                localUDPPort_buff[i] = '\0';	// null terminate
            }
            
            if (found_slash != 1) {		// local port
                localUDPPort_buff[i] = argv[j+1][i];
            }
            
            if (found_slash == 1) {		// remote port
                if (argv[j+1][i] == '/') {
                    i++;
                }
                remoteUDPPort_buff[remote_port_index] = argv[j+1][i];
                remote_port_index++;
            }
            i++;
        }
        remoteUDPPort_buff[remote_port_index] = '\0';	// null terminate
        //if (PRINT) {printf("local port %d:\t%s\n", j, localUDPPort_buff);}
        //if (PRINT) {printf("remote port %d:\t%s\n", j, remoteUDPPort_buff);}
        ports->port[j].local_port = (int)strtol(localUDPPort_buff, (char**)NULL, 10);
        ports->port[j].remote_port = (int)strtol(remoteUDPPort_buff, (char**)NULL, 10);
        ports->size++;
    }
    
    
    if (PRINT) {printf("Testing interface...\n");}
    j = 0;
    for (j = 0; j< numInterface; j++) {
        if (PRINT) {printf("local\tport\t%d:\t%d\n", j, ports->port[j].local_port);}
        if (PRINT) {printf("remote\tport\t%d:\t%d\n", j, ports->port[j].remote_port);}
    }
    
    
    // CREATE A SHARED FORWARDING TABLE
    struct ForwardingTable *forwarding_table = (struct ForwardingTable *)malloc(sizeof(struct ForwardingTable));
    forwarding_table->size = 0;			// keep track of table size	
    forwarding_table->transaction = -1;	// keep track of transactions (first transaction will be 0)
    
    // START LISTENING THREADS WITH LOCKS
    int t = 0;
    int rc = 0;
    pthread_t thread[numInterface];
    struct Lock *lock = (struct Lock *)malloc(sizeof(struct Lock));
    lock->key = 1;	// initialize number of keys
    pthread_mutex_init(&(lock->mutex), NULL);
    pthread_cond_init(&(lock->cond), NULL);
    
    for (t = 0; t < numInterface; t++) {
        struct FunctionArgs *funcArgs = (struct FunctionArgs *)malloc(sizeof(struct FunctionArgs));
        
        funcArgs->id = t;								// thread id
        funcArgs->lock = lock;							// all threads will point to the same lock struct (all share a single lock)
        funcArgs->ports = ports;						// all threads will point to the same port struct (all share the same ports)
        funcArgs->ports->size = numInterface;			
        funcArgs->forwarding_table = forwarding_table;	// all threads will point to the same forwarding table (all share the same table)
        if ((rc = pthread_create(&thread[t], NULL, listenThread, (void*)funcArgs)) != 0) { // default attribute
            printf("ERROR: Race condition %d\n", rc);
            exit(1);
        }
    }
    
    
    // JOIN ALL THREADS
    for (t = 0; t < numInterface; t++) {
        // Joining with child threads
        if ((rc = pthread_join(thread[t], NULL)) != 0) {
            printf("Error while joining with child thread #%d\n",rc);
            exit(1);
        }
    }
    
    return 0;
}
