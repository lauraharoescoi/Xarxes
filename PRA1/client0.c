#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUFFSIZE 255

char buffer[BUFFSIZE + 2];

//paquets UDP registre
/*enum register_packages {
    REG_REQ = 0xa0,
    REG_ACK = 0xa1,
    REG_NACK = 0xa2,
    REG_REJ = 0xa3,
    REG_INFO = 0xa4,
    INFO_ACK = 0xa5,
    INFO_NACK = 0xa6,
    INFO_REJ = 0xa7
};*/

unsigned char REG_REQ = 0xa0;
unsigned char REG_ACK = 0xa1;
unsigned char REG_NACK = 0xa2;
unsigned char REG_REJ = 0xa3;
unsigned char REG_INFO = 0xa4;
unsigned char INFO_ACK = 0xa5;
unsigned char INFO_NACK = 0xa6;
unsigned char INFO_REJ = 0xa7;


//estats del client
enum cli_stats {
    DISCONNECTED, 
    NOT_REGISTERED, 
    WAIT_ACK_REG, 
    WAIT_INFO, 
    WAIT_ACK_INFO, 
    REGISTERED, 
    SEND_ALIVE,
};

//temps d'espera
int t = 1;
int u = 2;
int n = 8;
int o = 3;
int p = 2;
int q = 4;

/*chars dels estats
char *DISCONNECTED = "DISCONNECTED";
char[] NOT_REGISTERED = "NOT_REGISTERED";
char[] WAIT_ACK_REG = "WAIT_ACK_REG";
char[] WAIT_INFO = "WAIT_INFO";
char[] WAIT_ACK_INFO = "WAIT_ACK_INFO";
char[] REGISTERED = "REGISTERED";
char[] SEND_ALIVE = "SEND_ALIVE"*/

char *stats_name[] = {[DISCONNECTED]="DISCONNECTED", [NOT_REGISTERED]="NOT_REGISTERED", [WAIT_ACK_REG]="WAIT_ACK_REG", [WAIT_INFO]="WAIT_INFO", [WAIT_ACK_INFO]="WAIT_ACK_INFO", [REGISTERED]="REGISTERED", [SEND_ALIVE]="SEND_ALIVE"};

//structs de configuracio
struct element {
    char magnitud[3];
    int ordinal;
    char tipus;
};

struct config {
    char id[11];
    struct element elements[5];
    int local_TCP;
    char server_name[11];
    int server_UDP;
};

typedef struct pdu_UDP {
    unsigned char tipus;
    char id_transmissor[11];
    char id_comunicacio[11];
    char dades[61];
} pdu_UDP;

void save_elements(char * elements, int i, struct config *config_parameters) {
    char * element;
    element = strtok(elements, "-");
    strcpy(config_parameters->elements[i].magnitud, element);

    element = strtok(NULL, "-");
    config_parameters->elements[i].ordinal = atoi(element);

    element = strtok(NULL, "-");
    config_parameters->elements[i].tipus = element[0];
}

void read_elements(char * values, struct config *config_parameters) {
    char * elements[5];
    int i = 0;
    elements[i] = strtok(values, ";");
    
    do {
        i += 1;
    } while ((elements[i] = strtok(NULL, ";")) != NULL);

    for(int j = 0; j < i; j++) {
        save_elements(elements[j], j, config_parameters);
    }
}

void read_parameters(char fn[64], struct config *config_parameters) {
    FILE *file = fopen(fn, "r");
    char *values;
    int line = 1;
    while(fgets(buffer, BUFFSIZE, (FILE*) file) != NULL) {
        values = strtok(buffer, " ");
        for (int i = 0; i < 2; i++) {
            strcpy(values, strtok(NULL, " "));
        }
        values[strcspn(values, "\n")] = 0;
        
        switch (line) {
            case 1:
                strcpy(config_parameters->id, values);
                break;
            
            case 2:
                read_elements(values, config_parameters);
                break;
            
            case 3:
                config_parameters->local_TCP = atoi(values);
                break;

            case 4:
                strcpy(config_parameters->server_name, values);
                break;

            case 5:
                config_parameters->server_UDP = atoi(values);
                break;
        
            default:
                break;
        }
        line += 1;
    }
    fclose(file);
}

void config_pdu_UDP(struct pdu_UDP *pdu, char paquet, char id[], char id_comunicacio[], char dades[]) {
    pdu->tipus = paquet;
    strcpy(pdu->id_comunicacio, id_comunicacio);
    strcpy(pdu->id_transmissor, id);
    strcpy(pdu->dades, dades);
}

int main(int argc, char *argv[]) {
    struct hostent *ent;
    struct config config_parameters;
    struct sockaddr_in addr_cli;
    struct sockaddr_in addr_server;
    int sock_UDP, a;
    struct pdu_UDP pdu;

    char *fn;
    
    //getopt mirar 

    switch(*argv[1]) {
        case 'c':
            if (argv[2] != NULL) { 
                fn = argv[2];
            } else {
                fn = "client.cfg";
            }
            break;

        case 'd':
            printf("DEBUG TIME!! \n");
            break;

        default:
            break;
    }

    read_parameters(fn, &config_parameters);
    
    //EEEEEH ZORRAAA AIXO VA MALAMENT, NO ET LLEGEIX BÉ EL SERVER_NAME DEL STRUCT CONFIG
    //strcpy(config_parameters.server_name, "localhost");
    ent = gethostbyname(config_parameters.server_name);
    if(!ent) {
        printf("Error! No trobat: %s \n",argv[1]);
        exit(-1);
    }
    
    //printf("%s\n", ent->h_addr);
    sock_UDP = socket(AF_INET, SOCK_DGRAM, 0);

	if(sock_UDP < 0) {
		fprintf(stderr,"No puc obrir socket!!!\n");
		perror(argv[0]);
		exit(-1);
	}

    memset(&addr_cli, 0, sizeof (struct sockaddr_in));
	addr_cli.sin_family = AF_INET;
	addr_cli.sin_addr.s_addr = INADDR_ANY;
	addr_cli.sin_port = htons(0);


    if(bind (sock_UDP, (struct sockaddr *) &addr_cli, sizeof(struct sockaddr_in)) < 0) {
		fprintf(stderr,"No puc fer el binding del socket!!!\n");
        perror(argv[0]);
        exit(-2);
	}
    memset(&addr_server, 0, sizeof (struct sockaddr_in));
	addr_server.sin_family = AF_INET;
	addr_server.sin_addr.s_addr = INADDR_ANY;
	addr_server.sin_port = htons(config_parameters.server_UDP);
    config_pdu_UDP(&pdu, REG_REQ, config_parameters.id, "0000000000", "");
    printf("%s\n", pdu.id_transmissor);
    
    a = sendto(sock_UDP, &pdu, sizeof(struct pdu_UDP) + 1, 0, (struct sockaddr*) &addr_server, sizeof(addr_server));
    if (a < 0) {
        fprintf(stderr,"Error al sendto\n");
        perror(argv[0]);
        exit(-2);
    }
    printf("Checkpoint\n");

    //aixo no saps ni el que fa autista
    /*a = recvfrom(sock_UDP, pdu_UDP.dades, BUFFSIZE, 0, (struct sockaddr *)0, (int *)0);
    if (a < 0) {
        fprintf(stderr, "Error al recvfrom\n");
        perror(argv[0]);
        exit(-2);
    }
    pdu_UDP.dades[a] = '\0';
    printf("%s\n", pdu_UDP.dades);*/

    close(sock_UDP);
}