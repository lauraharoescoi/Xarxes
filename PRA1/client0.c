#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <sys/select.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUFFSIZE 255

char buffer[BUFFSIZE + 2];

unsigned char REG_REQ = 0xa0;
unsigned char REG_ACK = 0xa1;
unsigned char REG_NACK = 0xa2;
unsigned char REG_REJ = 0xa3;
unsigned char REG_INFO = 0xa4;
unsigned char INFO_ACK = 0xa5;
unsigned char INFO_NACK = 0xa6;
unsigned char INFO_REJ = 0xa7;

struct sockaddr_in addr_cli;
struct sockaddr_in addr_server;

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
const int T = 1;
const int U = 2;
const int N = 8;
const int O = 3;
const int P = 2;
const int Q = 4;

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

int config_select(int socket, int timeout){
    struct timeval time;
    fd_set fd;
    FD_ZERO(&fd);
    FD_SET(socket, &fd);
    time.tv_sec = timeout;
    time.tv_usec = 0;
    int result = select(socket + 1, &fd, NULL, NULL, &time);
    if(result < 0) {
        printf("Error en el select");
        exit(-1);        
    }
    return result;
    
}


int register_select(int socket, struct pdu_UDP pdu) {
    int timeout = T;
    int num_tries = 0;
    int result_select = 0;
    int a;
    
    do {
        a = sendto(socket, &pdu, sizeof(struct pdu_UDP) + 1, 0, (struct sockaddr*) &addr_server, sizeof(addr_server));
        if (a < 0) {
            fprintf(stderr,"Error al sendto\n");
            exit(-2);
        }
        result_select = config_select(socket, timeout);
        ++num_tries;
        if(num_tries <= T * Q) {
            timeout = num_tries * T;
        }
        printf("intent: %i, temps: %i\n", num_tries, timeout);
    } while (result_select <= 0 && num_tries < N); 
    return result_select;
}

int main(int argc, char *argv[]) {
    struct hostent *ent;
    struct config config_parameters;
    int sock_UDP, a, b;
    struct pdu_UDP pdu;
    unsigned char estat_client;
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
    
    //EEEEEH ZORRAAA JA FUNCIONA, TENIES LA CARPETA CURSED
    ent = gethostbyname(config_parameters.server_name);
    if(!ent) {
        printf("Error! No trobat: %s \n",argv[1]);
        exit(-1);
    }
    
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

    //Binding del socket

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
    
    int intent_reg = 0;
    while (intent_reg < O) {
        a = register_select(sock_UDP, pdu);
        ++intent_reg;
    } 

    if (intent_reg == O) {
        printf("Error en el registre\n");
        exit(-2);
    }

    if (a > 0) {
        b = recvfrom(sock_UDP, &pdu, sizeof(pdu) + 1, 0, (struct sockaddr *)0,(int)0);
        if(b < 0) {
            fprintf(stderr,"Error al recvfrom\n");
            perror(argv[0]);
            exit(-2);
        }
        printf("Checkpoint\n");
    }
    close(sock_UDP);
}