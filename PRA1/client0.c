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

struct Config config_parameters;

typedef struct Element {
    char magnitud[3];
    int ordinal;
    char tipus;
} Element;

typedef struct Config {
    char id[10];
    struct Element elements[5];
    int local_TCP;
    char server_name[10];
    int server_UDP;
} Config;

void save_elements(char * elements, int i) {
    char * element;
    element = strtok(elements, "-");
    strcpy(config_parameters.elements[i].magnitud, element);

    element = strtok(NULL, "-");
    config_parameters.elements[i].ordinal = atoi(element);

    element = strtok(NULL, "-");
    config_parameters.elements[i].tipus = element[0];
}

void read_elements(char * values) {
    char * elements[5];
    int i = 0;
    elements[i] = strtok(values, ";");
    
    do {
        i += 1;
    } while ((elements[i] = strtok(NULL, ";")) != NULL);

    for(int j = 0; j < i; j++) {
        save_elements(elements[j], j);
    }
}

void read_parameters(char fn[64]) {
    FILE *file = fopen(fn, "r");
    char values[64];
    int line = 1;
    while(fgets(buffer, BUFFSIZE, (FILE*) file) != NULL) {
        strcpy(values, strtok(buffer, " "));
        for (int i = 0; i < 2; i++) {
            strcpy(values, strtok(NULL, " "));
        }
        values[strcspn(values, "\n")] = 0;

        switch (line) {
            case 1:
                strcpy(config_parameters.id, values);
                break;
            
            case 2:
                read_elements(values);
                break;
            
            case 3:
                config_parameters.local_TCP = atoi(values);
                break;

            case 4:
                strcpy(config_parameters.server_name, values);
                break;

            case 5:
                config_parameters.server_UDP = atoi(values);
                break;
        
            default:
                break;
        }
        line += 1;
    }
    fclose(file);
}


int main(int argc, char *argv[]) {
    struct hostent *ent;
    ent = gethostbyname(config_parameters.server_name);
    char *fn;

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
    
    read_parameters(fn);

    for(int i = 0; i < 4; i++){         
        printf("%s-%i-%c\n",config_parameters.elements[i].magnitud, config_parameters.elements[i].ordinal,config_parameters.elements[i].tipus);     
    }

}