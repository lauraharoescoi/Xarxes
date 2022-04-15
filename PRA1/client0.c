#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <stdbool.h>

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

unsigned char ALIVE = 0xb0;
unsigned char ALIVE_NACK = 0xb1;
unsigned char ALIVE_REJ = 0xb2;

struct sockaddr_in addr_cli;
struct sockaddr_in addr_server;
unsigned char estat_client;
struct hostent *ent;
bool debug = false;
char hora[20];

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

char *stats_name[] = {
    [DISCONNECTED]="DISCONNECTED", 
    [NOT_REGISTERED]="NOT_REGISTERED", 
    [WAIT_ACK_REG]="WAIT_ACK_REG", 
    [WAIT_INFO]="WAIT_INFO", 
    [WAIT_ACK_INFO]="WAIT_ACK_INFO", 
    [REGISTERED]="REGISTERED", 
    [SEND_ALIVE]="SEND_ALIVE"};

//temps d'espera
const int T = 1;
const int U = 2;
const int N = 8;
const int O = 3;
const int P = 2;
const int Q = 4;
const int V = 2;
const int R = 2; 


//structs de configuracio
struct element {
    char magnitud[3];
    int ordinal;
    char tipus;
};

struct config {
    char id[11];
    struct element elements[5];
    char elements_str[41];
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

void print_data_client(struct config config) {
    printf("********************* DADES DISPOSITIU ***********************\n\n");
    printf("  Identificador: %s\n", config.id);
    printf("  Estat: DISCONNECTED\n\n");
    printf("    Param       Valor\n");
    printf("    -------     --------------\n");
    /*int i = 0;
    while(i < sizeof(config.elements)) {
        printf("    %s      NONE\n", config.elements_str);
        ++i;
    }*/
    printf("**************************************************************\n");

}

void print_debug(char msg[]) {
    if(debug) {
        time_t timer;
        struct tm* tm_info; 
        time(&timer); 
        tm_info = localtime(&timer); 
        strftime(hora, 20, "%H:%M:%S", tm_info);
        printf("%s: DEBUG  =>  %s\n", hora, msg);
    }
}

void print_msg(char msg[]) {
    time_t timer;
    struct tm* tm_info; 
    time(&timer); 
    tm_info = localtime(&timer); 
    strftime(hora, 20, "%H:%M:%S", tm_info);
    printf("%s: MSG.  =>  %s\n", hora, msg);

}

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
    strcpy(config_parameters->elements_str, values);
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

int config_sockets(struct config config_parameters) {
    ent = gethostbyname(config_parameters.server_name);
    if(!ent) {
        printf("Error! No trobat");
        exit(-1);
    }
    
    int sock_UDP = socket(AF_INET, SOCK_DGRAM, 0);

	if(sock_UDP < 0) {
		fprintf(stderr,"No puc obrir socket!!!\n");
		exit(-1);
	}

    memset(&addr_cli, 0, sizeof (struct sockaddr_in));
	addr_cli.sin_family = AF_INET;
	addr_cli.sin_addr.s_addr = INADDR_ANY;
	addr_cli.sin_port = htons(0);

    return sock_UDP;
}
void bind_sock(int socket, struct config config_parameters) {
    if(bind (socket, (struct sockaddr *) &addr_cli, sizeof(struct sockaddr_in)) < 0) {
		fprintf(stderr,"No puc fer el binding del socket!!!\n");
        exit(-2);
	}
    memset(&addr_server, 0, sizeof (struct sockaddr_in));
    addr_server.sin_family = AF_INET;
    addr_server.sin_addr.s_addr = (((struct in_addr *)ent->h_addr_list[0])->s_addr);
    addr_server.sin_port = htons(config_parameters.server_UDP);
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

int register_select(int socket, struct pdu_UDP pdu, int num_tries) {
    int timeout = T;
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
    } while (result_select <= 0 && num_tries < N); 
    sleep(U);
    
    return result_select;
}

int send_reg_req(struct pdu_UDP pdu, int socket, int num_tries, int intent_reg) {
    int a = 0;
    while (intent_reg < O && a <= 0) {
        sprintf(buffer, "Dispositiu passa a estat: %s, procés de subscripció: %i", stats_name[estat_client], intent_reg + 1);
        print_msg(buffer);
        a = register_select(socket, pdu, num_tries);    
        printf(" MSG.  =>  Dispositiu passa a estat: %s\n", stats_name[estat_client]); 
        ++intent_reg;
    }
    if (intent_reg == O) {
            printf(" MSG.  =>  Superat el nombre de processos de subsctripció (3)\n");
            num_tries = 0;
            intent_reg = 0;
        } 
    return a;
}

void check_package(struct pdu_UDP pdu) {
    if(pdu.tipus == REG_ACK) estat_client = WAIT_ACK_REG;
    if(pdu.tipus == REG_REQ) estat_client = WAIT_ACK_REG;
    if(pdu.tipus == REG_NACK) estat_client = NOT_REGISTERED;
    if(pdu.tipus == REG_REJ) estat_client = NOT_REGISTERED;
    if(pdu.tipus == REG_INFO) estat_client = WAIT_ACK_INFO;
    if(pdu.tipus == INFO_ACK) estat_client = REGISTERED;
    if(pdu.tipus == ALIVE) estat_client = SEND_ALIVE;
    if(pdu.tipus == ALIVE_REJ) estat_client = NOT_REGISTERED;
}

void send_alive(struct pdu_UDP pdu, int sock_UDP, struct config config_parameters) {
    int count = 0, a = 0, b = 0, c = 0;
    do {    
        a = sendto(sock_UDP, &pdu, sizeof(struct pdu_UDP) + 1, 0, (struct sockaddr*) &addr_server, sizeof(addr_server));
        if (a < 0) {
            fprintf(stderr,"Error al sendto\n");
            exit(-2);
        }
        sleep(V);
        c = config_select(sock_UDP, 0);

        if (c == 0) {
            ++count;
        } else {
            count = 0;
        }
        if(c > 0) {
            b = recvfrom(sock_UDP, &pdu, sizeof(pdu) + 1, 0, (struct sockaddr *)0,(int)0);
            if(b < 0) {
                fprintf(stderr,"Error al recvfrom\n");
                exit(-2);
            }
            check_package(pdu);
            printf(" MSG.  =>  Dispositiu passa a estat: %s\n", stats_name[estat_client]); 
        }
        config_pdu_UDP(&pdu, ALIVE, config_parameters.id, pdu.id_comunicacio, "");
    } while(count < 3 && estat_client == SEND_ALIVE);
}

int main(int argc, char *argv[]) {
    
    struct config config_parameters;
    int sock_UDP, a = 0, b = 0, c = 0;
    struct pdu_UDP pdu;
    char *fn;
    estat_client = NOT_REGISTERED;
    int num_tries = 0, intent_reg = 0;
    
    
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
    print_data_client(config_parameters);

    sock_UDP = config_sockets(config_parameters);
    
    bind_sock(sock_UDP, config_parameters);

    while (estat_client != DISCONNECTED) {
        switch(estat_client) {
            case NOT_REGISTERED:
                config_pdu_UDP(&pdu, REG_REQ, config_parameters.id, "0000000000", "");
                if (pdu.tipus == REG_REJ){
                    num_tries = 0;
                    intent_reg = 0;
                    a = send_reg_req(pdu, sock_UDP, num_tries, intent_reg);
                }
                a = send_reg_req(pdu, sock_UDP, num_tries, intent_reg);
                check_package(pdu);
                break;
            case WAIT_ACK_REG:
                if (a > 0) {
                    b = recvfrom(sock_UDP, &pdu, sizeof(pdu) + 1, 0, (struct sockaddr *)0,(int)0);
                    if(b < 0) {
                        fprintf(stderr,"Error al recvfrom\n");
                        perror(argv[0]);
                        exit(-2);
                    }
                    check_package(pdu);
                    if(estat_client == WAIT_ACK_REG) {
                        //agafem les dades del packet REG_ACK per a enviar-les al packet REG_INFO
                        addr_server.sin_port = htons(atoi(pdu.dades));
                        sprintf(buffer, "%d,%s", config_parameters.local_TCP, config_parameters.elements_str);

                        config_pdu_UDP(&pdu, REG_INFO, config_parameters.id, pdu.id_comunicacio, buffer);

                        a = sendto(sock_UDP, &pdu, sizeof(struct pdu_UDP) + 1, 0, (struct sockaddr*) &addr_server, sizeof(addr_server));
                        if (a < 0) {
                            fprintf(stderr,"Error al sendto\n");
                            exit(-2);
                        }
                        check_package(pdu);
                    }
                }
                printf(" MSG.  =>  Dispositiu passa a estat: %s\n", stats_name[estat_client]); 
                break;
            case WAIT_ACK_INFO:
                printf(" MSG.  =>  Dispositiu passa a l'estat: %s\n", stats_name[estat_client]); 
                c = config_select(sock_UDP, 2 * T);
                if (c == 0) {
                    estat_client = NOT_REGISTERED;
                }
                if(c > 0 && estat_client == WAIT_ACK_INFO) {
                    b = recvfrom(sock_UDP, &pdu, sizeof(pdu) + 1, 0, (struct sockaddr *)0,(int)0);
                    if(b < 0) {
                        fprintf(stderr,"Error al recvfrom\n");
                        perror(argv[0]);
                        exit(-2);
                    }
                    check_package(pdu);
                    addr_server.sin_port = htons(config_parameters.server_UDP);
                }
                printf(" MSG.  =>  Dispositiu passa a estat: %s\n", stats_name[estat_client]); 
                break;
            case REGISTERED:
                config_pdu_UDP(&pdu, ALIVE, config_parameters.id, pdu.id_comunicacio, "");
                send_alive(pdu, sock_UDP, config_parameters);
                break;
            case SEND_ALIVE:
                config_pdu_UDP(&pdu, ALIVE, config_parameters.id, pdu.id_comunicacio, "");
                send_alive(pdu, sock_UDP, config_parameters);
                estat_client = NOT_REGISTERED;
                break;
            default:
                break;
        }
    }
    printf(" MSG.  =>  Dispositiu passa a estat: %s (Sense resposta a 3 ALIVES)\n", stats_name[estat_client]); 
    exit(-1);
    close(sock_UDP);
}