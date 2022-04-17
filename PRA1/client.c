/*----------------------------
    Pràctica 1 de Xarxes
    Autora: Laura Haro Escoi
    Data: 17/04/2022
------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <stdbool.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>

#define BUFFSIZE 255

char buffer[BUFFSIZE + 2];

// inicialització de variables globals
struct sockaddr_in addr_cli;
struct sockaddr_in addr_server;
unsigned char estat_client;
struct hostent *ent;
struct config print_config;
bool debug = false;
char hora[20];
int elements = 0;

// enum per al tipus de packets
enum package {
    REG_REQ = 0xa0,
    REG_ACK,
    REG_NACK,
    REG_REJ,
    REG_INFO,   
    INFO_ACK,
    INFO_NACK,
    INFO_REJ,
    ALIVE = 0xb0,
    ALIVE_NACK,
    ALIVE_REJ,
};

// array per a poder imprimir en forma de string els packets
char *package_name[] = {
    [REG_REQ] = "REG_REQ",
    [REG_ACK] = "REG_ACK",
    [REG_NACK] = "REG_NACK",
    [REG_REJ] = "REG_REJ",
    [REG_INFO] = "REG_INFO",
    [INFO_ACK] = "INFO_ACK",
    [INFO_NACK] = "INFO_NACK",
    [INFO_REJ] = "INFO_REJ",
    [ALIVE] = "ALIVE",
    [ALIVE_NACK] = "ALIVE_NACK",
    [ALIVE_REJ] = "ALIVE_REJ"};

//enum dels estats del client
enum cli_stats {
    DISCONNECTED = 0xf0, 
    NOT_REGISTERED,
    WAIT_ACK_REG,
    WAIT_INFO,
    WAIT_ACK_INFO , 
    REGISTERED, 
    SEND_ALIVE,
};

//array per a poder imprimir en forma de string els estats del client
char *stats_name[] = {
    [DISCONNECTED]="DISCONNECTED", 
    [NOT_REGISTERED]="NOT_REGISTERED", 
    [WAIT_ACK_REG]="WAIT_ACK_REG", 
    [WAIT_INFO]="WAIT_INFO", 
    [WAIT_ACK_INFO]="WAIT_ACK_INFO", 
    [REGISTERED]="REGISTERED", 
    [SEND_ALIVE]="SEND_ALIVE"};

//constants pel temps d'espera
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

//funcio per a imprimir l'estat del client i els seus elements
void print_data_client(struct config config, int elements) {
    printf("********************* DADES DISPOSITIU ***********************\n\n");
    printf("  Identificador: %s\n", config.id);
    printf("  Estat: %s\n\n", stats_name[estat_client]);
    printf("    Param       Valor\n");
    printf("    -------     --------------\n");
    for(int i = 0; i < elements; i++) {
        printf("    %s-%d-%c     NONE\n", config.elements[i].magnitud, config.elements[i].ordinal, config.elements[i].tipus);
    }
    printf("\n***************************************************************\n"); 

}

//funció per a imprimir els missatges en el mode debug
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

//funció per a imprimir els missatges sense mode debug
void print_msg(char msg[]) {
    time_t timer;
    struct tm* tm_info; 
    time(&timer); 
    tm_info = localtime(&timer); 
    strftime(hora, 20, "%H:%M:%S", tm_info);
    printf("%s: MSG.  =>  %s\n", hora, msg);

}

//funció auxiliar de read_elements per a guardar els elements en un struct
void save_elements(char * elements, int i, struct config *config_parameters) {
    char * element;
    element = strtok(elements, "-");
    strcpy(config_parameters->elements[i].magnitud, element);

    element = strtok(NULL, "-");
    config_parameters->elements[i].ordinal = atoi(element);

    element = strtok(NULL, "-");
    config_parameters->elements[i].tipus = element[0];
}

//funció auxiliar de read_parametres per a guardar tots els elements per separat
int read_elements(char * values, struct config *config_parameters) {
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
    return i;
}

//funció per a llegir els fitxers de configuració i guardar les dades en un struct config
int read_parameters(char fn[64], struct config *config_parameters) {
    FILE *file = fopen(fn, "r");
    char *values;
    int elements = 0;
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
                elements = read_elements(values, config_parameters);
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
    return elements;
}

//funció per a configurar els sockets
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

//funció per a fer un binding dels sockets
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

//funció per a configurar la pdu del tipus de paquet que vulguem enviar
void config_pdu_UDP(struct pdu_UDP *pdu, int paquet, char id[], char id_comunicacio[], char dades[]) {
    pdu->tipus = paquet;
    strcpy(pdu->id_comunicacio, id_comunicacio);
    strcpy(pdu->id_transmissor, id);
    strcpy(pdu->dades, dades);
}

//funcio que ens canvia l'estat del client depenent del tipus de paquets que ens arribin o enviem
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

//funció auxiliar de register_select per a configurar el select segons un temps d'espera
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

//funcio per al select del registre
int register_select(int socket, struct pdu_UDP pdu, int num_tries) {
    int timeout = T;
    int result_select = 0;
    int a;
    
    do {
        a = sendto(socket, &pdu, sizeof(struct pdu_UDP), 0, (struct sockaddr*) &addr_server, sizeof(addr_server));
        sprintf(buffer, "Enviat: bytes=%d, paquet=%s, id=%s, id. com.=%s, dades=%s", a, package_name[pdu.tipus], pdu.id_transmissor, pdu.id_comunicacio, pdu.dades);
        print_debug(buffer);
        if(estat_client != WAIT_ACK_REG) {
            check_package(pdu);
            sprintf(buffer, "Dispositiu passa a estat: %s", stats_name[estat_client]);
            print_msg(buffer);
        }
        check_package(pdu);
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

//funció per a enviar un REG_REQ segons el proces de subscripcio en el que ens trobem
int send_reg_req(struct pdu_UDP pdu, int socket, int num_tries, int intent_reg) {
    int a = 0;
    while (intent_reg < O && a <= 0) {
        sprintf(buffer, "Dispositiu passa a estat: %s, procés de subscripció: %i", stats_name[estat_client], intent_reg + 1);
        print_msg(buffer);
        a = register_select(socket, pdu, num_tries);    

        ++intent_reg;
    }
    if (intent_reg >= O) {
            sprintf(buffer, "Superat el nombre de processos de subsctripció (3)");
            print_msg(buffer);
            num_tries = 0;
            intent_reg = 0;
            exit(-1);
        } 
    return a;
}

//funció per a llegir les comandes del terminal després d'entrar en l'estat SEND_ALIVE
void *command_stats() {
    while((fgets(buffer, sizeof(buffer), stdin) != NULL)) {
        if (strcmp(buffer, "quit\n") == 0) {
            exit(-1);
        } else if (strcmp(buffer, "stat\n") == 0) {
            print_data_client(print_config, elements);
        } else {
            printf("Comanda no vàlida\n");
        }
    }
    pthread_exit(NULL);
}

//funció per a enviar els paquets ALIVE 
void send_alive(struct pdu_UDP pdu, int sock_UDP, struct config config_parameters) {
    int count = 0, a = 0, b = 0, c = 0;
    do {    
        a = sendto(sock_UDP, &pdu, sizeof(struct pdu_UDP), 0, (struct sockaddr*) &addr_server, sizeof(addr_server));
        sprintf(buffer, "Enviat: bytes=%d, paquet=%s, id=%s, id. com.=%s, dades=%s", a, package_name[pdu.tipus], pdu.id_transmissor, pdu.id_comunicacio, pdu.dades);
        print_debug(buffer);

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
            b = recvfrom(sock_UDP, &pdu, sizeof(pdu), 0, (struct sockaddr *)0,(int)0);
            sprintf(buffer, "Rebut: bytes=%d, paquet=%s, id=%s, id. com.=%s, dades=%s", b, package_name[pdu.tipus], pdu.id_transmissor, pdu.id_comunicacio, pdu.dades);
            print_debug(buffer);
            if(b < 0) {
                fprintf(stderr,"Error al recvfrom\n");
                exit(-2);
            }
        }
        config_pdu_UDP(&pdu, ALIVE, config_parameters.id, pdu.id_comunicacio, "");
    } while(count < 3 && estat_client == SEND_ALIVE);
}


int main(int argc, char *argv[]) {
    //variables del main
    struct config config_parameters;
    int sock_UDP, a = 0, b = 0, c = 0;
    struct pdu_UDP pdu;
    char fn[64] = "client.cfg";
    estat_client = DISCONNECTED;
    int num_tries = 0, intent_reg = 0;
    int option;
    pthread_t thread_id = (pthread_t) NULL;
    
    //llegim el tipus de comanda que es vol fer
    while((option = getopt(argc, argv, "cd")) != -1) {
        switch (option) {
        case 'c':
            if (argv[2] != NULL) strcpy(fn, argv[2]);
            else {
                printf("ERROR  => No es pot obrir l'arxiu de configuració\n");
                exit(-1);
            }
            break;
        case 'd':
            debug = true;
            break;
        case '?':
            printf("Error en els paràmetres d'entrada.\n");
            exit(-1);
        default:
            break;
        }
    }
    //llegim els paràmetres de configuració
    elements = read_parameters(fn, &config_parameters);
    print_config = config_parameters;
    print_data_client(config_parameters, elements);

    //configurem els sockets
    sock_UDP = config_sockets(config_parameters);
    bind_sock(sock_UDP, config_parameters);
    sprintf(buffer, "Inici bucle de servei equip : %s", config_parameters.id);
    print_debug(buffer);
    estat_client = NOT_REGISTERED;
    
    //loop del registre on un switch determina que ha d'executar segons l'estat en el que ens trobem
    while (estat_client != DISCONNECTED) {
        switch(estat_client) {
            case NOT_REGISTERED:
                //si el paquet que ens arriba és un REG_REJ, es reinicia el proces de subscripcio
                if (pdu.tipus == REG_REJ){
                    num_tries = 0;
                    intent_reg += 1;
                    config_pdu_UDP(&pdu, REG_REQ, config_parameters.id, "0000000000", "");
                    a = send_reg_req(pdu, sock_UDP, num_tries, intent_reg);
                }
                //enviament del paquet REG_REQ
                config_pdu_UDP(&pdu, REG_REQ, config_parameters.id, "0000000000", "");
                a = send_reg_req(pdu, sock_UDP, num_tries, intent_reg);
                check_package(pdu);
                break;
            case WAIT_ACK_REG:
                //rebem el resultat de l'enviament del paquet REG_REQ
                if (a > 0) {
                    b = recvfrom(sock_UDP, &pdu, sizeof(pdu), 0, (struct sockaddr *)0,(int)0);
                    sprintf(buffer, "Rebut: bytes=%d, paquet=%s, id=%s, id. com.=%s, dades=%s", b, package_name[pdu.tipus], pdu.id_transmissor, pdu.id_comunicacio, pdu.dades);
                    print_debug(buffer);
                    if(b < 0) {
                        fprintf(stderr,"Error al recvfrom\n");
                        perror(argv[0]);
                        exit(-2);
                    }
                    //canviem l'estat del client
                    check_package(pdu);
                    if(estat_client == WAIT_ACK_REG) {
                        //agafem les dades del packet REG_ACK per a enviar-les al packet REG_INFO
                        addr_server.sin_port = htons(atoi(pdu.dades));
                        sprintf(buffer, "%d,%s", config_parameters.local_TCP, config_parameters.elements_str);

                        config_pdu_UDP(&pdu, REG_INFO, config_parameters.id, pdu.id_comunicacio, buffer);
                        //enviem un REG_INFO
                        a = sendto(sock_UDP, &pdu, sizeof(struct pdu_UDP), 0, (struct sockaddr*) &addr_server, sizeof(addr_server));
                        sprintf(buffer, "Enviat: bytes=%d, paquet=%s, id=%s, id. com.=%s, dades=%s", a, package_name[pdu.tipus], pdu.id_transmissor, pdu.id_comunicacio, pdu.dades);
                        print_debug(buffer);
                        if (a < 0) {
                            fprintf(stderr,"Error al sendto\n");
                            exit(-2);
                        }
                        check_package(pdu);
                    }
                }
                sprintf(buffer, "Dispositiu passa a estat: %s", stats_name[estat_client]); 
                print_msg(buffer);
                break;
            case WAIT_ACK_INFO:
                //rebem el resultat de l'enviament del paquet REG_INFO esperant 2t segons
                c = config_select(sock_UDP, 2 * T);
                if (c == 0) {
                    estat_client = NOT_REGISTERED;
                }
                if(c > 0 && estat_client == WAIT_ACK_INFO) {
                    b = recvfrom(sock_UDP, &pdu, sizeof(pdu), 0, (struct sockaddr *)0,(int)0);
                    sprintf(buffer, "Rebut: bytes=%d, paquet=%s, id=%s, id. com.=%s, dades=%s", b, package_name[pdu.tipus], pdu.id_transmissor, pdu.id_comunicacio, pdu.dades);
                    print_debug(buffer);
                    if(b < 0) {
                        fprintf(stderr,"Error al recvfrom\n");
                        perror(argv[0]);
                        exit(-2);
                    }
                    check_package(pdu);
                    addr_server.sin_port = htons(config_parameters.server_UDP);
                }
                sprintf(buffer, "Dispositiu passa a estat: %s", stats_name[estat_client]); 
                print_msg(buffer);
                sprintf(buffer, "Obert por TCP %d per la comunicació amb el servidor", config_parameters.local_TCP);
                print_msg(buffer);
                break;
            case REGISTERED:
                //enviem el primer ALIVE
                config_pdu_UDP(&pdu, ALIVE, config_parameters.id, pdu.id_comunicacio, "");
                send_alive(pdu, sock_UDP, config_parameters);
                check_package(pdu);
                sprintf(buffer, "Dispositiu passa a estat: %s", stats_name[estat_client]); 
                print_msg(buffer);
                break;
            case SEND_ALIVE:
                //seguim enviant alives fins que acabi la funció send_alives
                config_pdu_UDP(&pdu, ALIVE, config_parameters.id, pdu.id_comunicacio, "");
                pthread_create(&thread_id, NULL, command_stats, NULL);
                send_alive(pdu, sock_UDP, config_parameters);
                estat_client = NOT_REGISTERED;
                sprintf(buffer, "Dispositiu passa a estat: DISCONNECTED (Sense resposta a 3 ALIVES)"); 
                print_msg(buffer);
                break;
            default:
                break;
        }
    }
    exit(-1);
    close(sock_UDP);
}