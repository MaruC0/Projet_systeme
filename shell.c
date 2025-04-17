#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#define ENTRY_SIZE 500
#define PATH_SIZE 500

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

char currentpath[PATH_SIZE];

bool compare(char* str1, char* str2){
    // Compare la chaîne str1 au début de la chaîne str2.
    int i=0;
    if(str1[0] == '\0' && str2[0] == '\0'){ // Cas deux string vide.
        return true;
    }
    if(str1[0] == '\0' || str2[0] == '\0'){ // Cas une des deux string vide
        return false;
    }
    while(str1[i] != '\0'){ // Tant qu'on a pas fini de lire str1
        // Si les caractères sont différents ou str2 est plus petit que str1
        if (str1[i] != str2[i] || str2[i] == '\0'){
            return false;
        }
        i += 1;
    }
    return true;
}

void setIO(char *args[],int nbargs){
    int lastI = -1;
    int lastO = -1;
    int fdi = -2;
    int fdo = -2;
    for (int i=1;i<nbargs;i++){
        if(compare(args[i], "<")){
            lastI = i;
        } else if(compare(args[i], ">")){
            lastO = i;
        }
    }
    if (nbargs>lastI+1 && lastI>-1){
        fdi = open(args[lastI+1],O_RDWR | O_CREAT, 00777);
    }
    if (nbargs>lastO+1 && lastO>-1){
        fdo = open(args[lastO+1],O_RDWR | O_CREAT, 00777);
    }
    if (fdi>=0){
        dup2(fdi, STDIN_FILENO);
        close(fdi);
    } else if (fdi == -1){
        printf("erreur fdi: %s\n", strerror(errno));
    }
    if (fdo>=0){
        dup2(fdo, STDOUT_FILENO);
        close(fdo);
    } else if (fdo == -1){
        printf("erreur fdo: %s\n", strerror(errno));
    }
}

void askInput(char** entry){
    /* Affiche le path actuel, demande une entrée,
    et la place dans la variable passée en paramètre. */
    printf("%s%s%s$ ", KCYN, currentpath, KNRM);
    size_t taille = sizeof(entry);
    if(getline(entry, &taille, stdin) < 0){
        perror("askInput");
        exit(EXIT_FAILURE);
    }
}

int getArgs(char* entry, char* tab[]){
    const char* separators = " \n";
    int i = 0;
    char* strToken = strtok(entry, separators);
    while (strToken != NULL) {
        tab[i] = strToken;
        // On demande le token suivant.
        strToken = strtok(NULL, separators);
        i++;
    }
    return i;
}

int main(int argc, char *argv[]){

    char* entry = malloc(sizeof(char) * ENTRY_SIZE);
    char* args[ENTRY_SIZE / 2]; // Tableau d'arguments en entrée
    int nbargs = 0;

    while(true) {
        getcwd(currentpath, PATH_SIZE);
        askInput(&entry);
        nbargs = getArgs(entry, args);

        char* command = args[0];

        if(strcmp(command, "exit") == 0) {
                break;
        } else if(strcmp("cd", command) == 0) {
            if(nbargs > 2){
                printf("cd: too many arguments\n");
            } else if(nbargs == 1){
                chdir(getenv("HOME"));
            } else {
                if(chdir(args[1]) != 0){
                    printf("cd: %s: %s\n", args[1], strerror(errno));
                }
            }
        } else if(compare("./", command) || compare("../", command) || command[0] == '/'){
            /*Spawn a child to run the program.*/
            pid_t pid = fork();
            if (pid == 0) { /* child process */
                printf("Dans le bon if\n");
                setIO(args, nbargs);
                char* name;
                char* path;
                if(command[0] == '/'){
                    path = &command[0];
                }
                else{
                    name = &command[0];
                    path = malloc(sizeof(currentpath) + strlen(name) + 1);
                    strcpy(path, currentpath);
                    strcat(path, "/");
                    strcat(path, name);
                }
                char* argv2[] = {path, NULL}; // Tableau pour execv
                execv(path, argv2);
                printf("%s: %s\n", command, strerror(errno));
                exit(127); /* only if execv fails */
            } else { /* pid!=0; parent process */
                waitpid(pid, 0, 0); /* wait for child to exit */
            }
        } else {
            printf("%s : command not found\n", entry);
        }

    }
    return 0;
}
