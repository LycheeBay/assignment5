#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lib/tdns/tdns-c.h"

/* A few macros that might be useful */
/* Feel free to add macros you want */
#define DNS_PORT 53
#define BUFFER_SIZE 2048 



int main() {
    /* A few variable declarations that might be useful */
    /* You can add anything you want */
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    /* PART1 TODO: Implement a DNS nameserver for the utexas.edu zone */
    
    /* 1. Create an **UDP** socket */
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    /* 2. Initialize server address (INADDR_ANY, DNS_PORT) */
    /* Then bind the socket to it */

    memset(&server_addr, 0, sizeof(server_addr));
    memset(&client_addr, 0, sizeof(client_addr));

    // Filling server information
    server_addr.sin_family = AF_INET; // IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(DNS_PORT);

    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    /* 3. Initialize a server context using TDNSInit() */
    /* This context will be used for future TDNS library function calls */

    struct TDNSServerContext *ctx = TDNSInit();

    /* 4. Create the cs.utexas.edu zone using TDNSCreateZone() */
    /* Add an IP address for cs.utexas.edu domain using TDNSAddRecord() */
    /* Add an IP address for aquila.cs.utexas.edu domain using TDNSAddRecord() */

    TDNSCreateZone(ctx, "cs.utexas.edu");
    TDNSAddRecord(ctx, "cs.utexas.edu", "", "50.0.0.10", NULL);
    TDNSAddRecord(ctx, "cs.utexas.edu", "aquila", "50.0.0.20", NULL);

    /* 5. Receive a message continuously and parse it using TDNSParseMsg() */

    while (1) {
        socklen_t len = sizeof(client_addr); // len is value/result
        printf("Before recvfrom\n");
        ssize_t n = recvfrom(sockfd, buffer, BUFFER_SIZE, MSG_WAITALL, (struct sockaddr *)&client_addr, &len);
        //buffer[n] = '\0'; // Null-terminate the received message
        printf("After recvfrom\n");

        // Parse the received message
        struct TDNSParseResult *parsed = malloc(sizeof(struct TDNSParseResult));;
        if (parsed == NULL) {
            perror("malloc failed");
            exit(EXIT_FAILURE);
        }

        printf("Before TDNSParseMSG\n");
        TDNSParseMsg(buffer, n, parsed);
        printf("After TDNSParseMSG\n");


        struct TDNSFindResult *res = malloc(sizeof(struct TDNSFindResult)); 
        if (res == NULL) {
            perror("malloc failed");
            exit(EXIT_FAILURE);
        }

        if (parsed->qtype == 1 || parsed->qtype == 2 || parsed->qtype == 28) {  // A, NS, AAAA
            printf("Before TDNSFind\n");
            TDNSFind(ctx, parsed, res);
            printf("After TDNSFind\n");
            //sendto(sockfd, res, sizeof(res), 0);
            sendto(sockfd, res->serialized, res->len, 0, (struct sockaddr *)&client_addr, client_len);
            //return 0;
        }

        free(parsed);
        free(res);
    }

    /* 6. If it is a query for A, AAAA, NS DNS record */
    /* find the corresponding record using TDNSFind() and send the response back */
    /* Otherwise, just ignore it. */

    return 0;
}

    /* 4. Create the cs.utexas.edu zone using TDNSCreateZone() */
    /* Add an IP address for cs.utexas.edu domain using TDNSAddRecord() */
    /* Add an IP address for aquila.cs.utexas.edu domain using TDNSAddRecord() */