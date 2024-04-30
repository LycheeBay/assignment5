#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lib/tdns/tdns-c.h"

/* DNS header structure */
struct dnsheader {
        uint16_t        id;         /* query identification number */
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
                        /* fields in third byte */
        unsigned        qr: 1;          /* response flag */
        unsigned        opcode: 4;      /* purpose of message */
        unsigned        aa: 1;          /* authoritative answer */
        unsigned        tc: 1;          /* truncated message */
        unsigned        rd: 1;          /* recursion desired */
                        /* fields in fourth byte */
        unsigned        ra: 1;          /* recursion available */
        unsigned        unused :1;      /* unused bits (MBZ as of 4.9.3a3) */
        unsigned        ad: 1;          /* authentic data from named */
        unsigned        cd: 1;          /* checking disabled by resolver */
        unsigned        rcode :4;       /* response code */
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ 
                        /* fields in third byte */
        unsigned        rd :1;          /* recursion desired */
        unsigned        tc :1;          /* truncated message */
        unsigned        aa :1;          /* authoritative answer */
        unsigned        opcode :4;      /* purpose of message */
        unsigned        qr :1;          /* response flag */
                        /* fields in fourth byte */
        unsigned        rcode :4;       /* response code */
        unsigned        cd: 1;          /* checking disabled by resolver */
        unsigned        ad: 1;          /* authentic data from named */
        unsigned        unused :1;      /* unused bits (MBZ as of 4.9.3a3) */
        unsigned        ra :1;          /* recursion available */
#endif
                        /* remaining bytes */
        uint16_t        qdcount;    /* number of question records */
        uint16_t        ancount;    /* number of answer records */
        uint16_t        nscount;    /* number of authority records */
        uint16_t        arcount;    /* number of resource records */
};

/* A few macros that might be useful */
/* Feel free to add macros you want */
#define DNS_PORT 53
#define BUFFER_SIZE 2048 

int main() {
    /* A few variable declarations that might be useful */
    /* You can add anything you want */
    int sockfd;
    struct sockaddr_in server_addr, client_addr, delegate_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    /* PART2 TODO: Implement a local iterative DNS server */
    
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

    /* 4. Create the edu zone using TDNSCreateZone() */
    /* Add the UT nameserver ns.utexas.edu using using TDNSAddRecord() */
    /* Add an IP address for ns.utexas.edu domain using TDNSAddRecord() */

    TDNSCreateZone(ctx, "edu");
    TDNSAddRecord(ctx, "edu", "utexas", NULL, "ns.utexas.edu");
    // TDNSAddRecord(ctx, "ns.utexas.edu", "ns", "40.0.0.20", NULL);
    TDNSAddRecord(ctx, "utexas.edu", "ns", "40.0.0.20", NULL);
    // (ctx, "edu", "ns.utexas", "40.0.0.20", NULL);

    /* 5. Receive a message continuously and parse it using TDNSParseMsg() */

    while (1) {
        int len, n;

        len = sizeof(client_addr); // len is value/result
        n = recvfrom(sockfd, buffer, BUFFER_SIZE, MSG_WAITALL, (struct sockaddr *)&client_addr, &len);
        buffer[n] = '\0'; // Null-terminate the received message

        // Parse the received message
        struct TDNSParseResult *parsed = malloc(sizeof(struct TDNSParseResult));;
        if (parsed == NULL) {
            perror("malloc failed");
            exit(EXIT_FAILURE);
        }
        int TDNSType = TDNSParseMsg(buffer, sizeof(buffer), parsed);

        struct TDNSFindResult *res = malloc(sizeof(struct TDNSFindResult)); 
        if (res == NULL) {
            perror("malloc failed");
            exit(EXIT_FAILURE);
        }

        if (TDNSType == TDNS_QUERY) {
            if (parsed->qtype == 1 || parsed->qtype == 2 || parsed->qtype == 28) {  // A, NS, AAAA
                if (TDNSFind(ctx, parsed, res)) { 
                    if (parsed->nsIP) { // indicates delegation
                        char serialized[BUFFER_SIZE];
                        printf("Delegating query\n");
                        socklen_t delegate_len = sizeof(delegate_addr);
                        //delegate_addr.sin_addr.s_addr = (uint32_t) parsed->nsIP;
                        inet_aton(parsed->nsIP, &delegate_addr.sin_addr);
                        delegate_addr.sin_family =  AF_INET;
                        delegate_addr.sin_port = htons(DNS_PORT);
                        // not sure if that's the correct sockfd_in here
                        putAddrQID(ctx, parsed->dh->id, (struct sockaddr *)&client_addr);
                        putNSQID(ctx, parsed->dh->id, parsed->nsIP, parsed->nsDomain);
                        sendto(sockfd, buffer, n, 0, (struct sockaddr *)&delegate_addr, delegate_len);
                        printf("Exiting delegating query\n");
                    }
                    else {
                        sendto(sockfd, res->serialized, res->len, 0, (struct sockaddr *)&client_addr, client_len);
                    }
                }
                // response not found
                else {
                    sendto(sockfd, res->serialized, res->len, 0, (struct sockaddr *)&client_addr, client_len);
                }
                sendto(sockfd, res->serialized, res->len, 0, (struct sockaddr *)&client_addr, client_len);
                //return 0;
            }
        }
        else { // TDNS_RESPONSE
            if (parsed->nsIP) { // non-authoritative
                printf("Not authoritative\n");
                char serialized[BUFFER_SIZE];
                int64_t query_size = TDNSGetIterQuery(parsed, serialized);
                socklen_t delegate_len = sizeof(delegate_addr);
                inet_aton(parsed->nsIP, &delegate_addr.sin_addr);
                delegate_addr.sin_family =  AF_INET;
                delegate_addr.sin_port = htons(DNS_PORT);
                putNSQID(ctx, parsed->dh->id, parsed->nsIP, parsed->nsDomain);
                sendto(sockfd, buffer, query_size, 0, (struct sockaddr *)&delegate_addr, delegate_len);
            }
            else { // authoritative
                printf("Authoritative\n");
                getNSbyQID(ctx, parsed->dh->id, parsed->nsIP, parsed->nsDomain);
                getAddrbyQID(ctx, parsed->dh->id, (struct sockaddr *)&client_addr);
                uint64_t new_length = TDNSPutNStoMessage(buffer, n, parsed, parsed->nsIP, parsed->nsDomain);
                sendto(sockfd, buffer, new_length, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
                delAddrQID(ctx, parsed->dh->id);
                delNSQID(ctx, parsed->dh->id);
            }
        }

        free(parsed);
        free(res);
    }

    /* 6. If it is a query for A, AAAA, NS DNS record, find the queried record using TDNSFind() */
    /* You can ignore the other types of queries */

        /* a. If the record is found and the record indicates delegation, */
        /* send an iterative query to the corresponding nameserver */
        /* You should store a per-query context using putAddrQID() and putNSQID() */
        /* for future response handling */

        /* b. If the record is found and the record doesn't indicate delegation, */
        /* send a response back */

        /* c. If the record is not found, send a response back */

    /* 7. If the message is an authoritative response (i.e., it contains an answer), */
    /* add the NS information to the response and send it to the original client */
    /* You can retrieve the NS and client address information for the response using */
    /* getNSbyQID() and getAddrbyQID() */
    /* You can add the NS information to the response using TDNSPutNStoMessage() */
    /* Delete a per-query context using delAddrQID() and putNSQID() */

    /* 7-1. If the message is a non-authoritative response */
    /* (i.e., it contains referral to another nameserver) */
    /* send an iterative query to the corresponding nameserver */
    /* You can extract the query from the response using TDNSGetIterQuery() */
    /* You should update a per-query context using putNSQID() */

    return 0;
}

