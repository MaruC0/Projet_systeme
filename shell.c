#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

char* currentpath;

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

void setIO(char *args[], int nbargs){
    int lastI = -1;
    int lastO = -1;
    int fdi = -2;
    int fdo = -2;
    for (int i=1; i<nbargs; i++){
        if(args[i][0] == '<'){
            lastI = i;
        } else if(args[i][0] == '>'){
            lastO = i;
        }
    }
    if (nbargs > lastI+1 && lastI > -1){
        fdi = open(args[lastI+1], O_RDWR | O_CREAT, 00777);
    }
    if (nbargs > lastO+1 && lastO > -1){
        fdo = open(args[lastO+1], O_RDWR | O_CREAT, 00777);
    }
    if (fdi >= 0){
        dup2(fdi, STDIN_FILENO);
        close(fdi);
    } else if (fdi == -1){
        printf("erreur fdi: %s\n", strerror(errno));
    }
    if (fdo >= 0){
        dup2(fdo, STDOUT_FILENO);
        close(fdo);
    } else if (fdo == -1){
        printf("erreur fdo: %s\n", strerror(errno));
    }
}

size_t askInput(char** entry){
    /* Affiche le path actuel, demande une entrée,
    et la place dans la variable passée en paramètre. */
    printf("%s%s%s$ ", KCYN, currentpath, KNRM);
    free(*entry);
    *entry = NULL;
    size_t taille = 0;
    getline(entry, &taille, stdin);
    if(taille < 0){
        perror("askInput");
        exit(EXIT_FAILURE);
    }
    return taille;
}

int getArgs(char* tab[], char* entry, const char* separators){
    if(!separators){
        separators = " \n";
    }
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

void exec_command(char** args, uint nbargs){
    // Execute une ligne de commande simple

    char* command = args[0];

    if(strcmp(command, "exit") == 0) {
        exit(0);
    } else if(strcmp("cd", command) == 0) {
        if(nbargs > 2){
            fprintf(stderr, "cd: too many arguments\n");
        } else if(nbargs == 1){
            chdir(getenv("HOME"));
        } else {
            if(chdir(args[1]) != 0){
                fprintf(stderr, "cd: %s: %s\n", args[1], strerror(errno));
            }
        }
    // Retirer ça pour permettre d'utiliser ls par ex
   } else {
        setIO(args, nbargs);
        char* path = &command[0];
        char* argv2[] = {path, NULL}; // Tableau pour execv
        execv(path, argv2);

        if(compare("./", command) || compare("../", command) || command[0] == '/'){
            // Ici, l'erreur devrait être 'no such file or directory'
            fprintf(stderr, "%s: %s\n", command, strerror(errno));
        } else {
            fprintf(stderr, "%s: command not found\n", command);
        }
        free(args);
        exit(127); /* only if execv fails */
    }
    free(args);
}

int spawn_proc (int in, int out, char* pipes, char** args, int nbargs) {
    pid_t pid;
    char** current_args;
    uint current_nbargs;
    // Selon le cas où l'on vient d'un pipe ou d'une commande seul pour ne pas refaire le getArgs
    if(pipes){
        current_args = malloc(strlen(pipes) * sizeof(char*));
        current_nbargs = getArgs(current_args, pipes, " \n");
    }
    else{
        current_args = args;
        current_nbargs = nbargs;
    }
    if ((pid = fork()) == 0){
        if (in != 0){
            dup2(in, STDIN_FILENO);
            close(in);
        }

        if (out != 1){
            dup2(out, STDOUT_FILENO);
            close(out);
        }

        exec_command(current_args, current_nbargs);
        exit(0);
    } else {
        waitpid(pid, 0, 0);
    }
    if(pipes){
        free(current_args);
    }
    return pid;
}


int main(int argc, char *argv[]){

    char* entry = NULL;
    int in, fd[2];

    while(true) {

        currentpath = getcwd(NULL, 0);
        size_t entry_size = askInput(&entry);
        char** pipes = malloc(entry_size * sizeof(char*));
        uint nbcmd = getArgs(pipes, entry, "|\n");

        in = 0;

        if (nbcmd > 1){
            /* The first process should get its input from the original file descriptor 0.  */

            /* Note the loop bound, we spawn here all, but the last stage of the pipeline.  */
            for (int i = 0; i < nbcmd-1; ++i){
                pipe(fd);

                spawn_proc(in, fd[1], pipes[i], NULL, -1);

                close(fd[1]);

                in = fd[0];
            }
            // Dernière commande prend sont entrée depuis le dernier pipe et affiche sur la sortie standard
            spawn_proc(in, STDOUT_FILENO, pipes[nbcmd-1], NULL, -1);

            close(fd[0]);
            close(fd[1]);

            if (in != 0){
                dup2(0, in);
            }

        } else {
            char** args = malloc(strlen(pipes[0]) * sizeof(char*));
            uint nbargs = getArgs(args, pipes[0], " \n");
            char* command = args[0];

            if(strcmp(command, "exit") == 0) {
                exit(0);
            } else if(strcmp("cd", command) == 0) {
                if(nbargs > 2){
                    fprintf(stderr, "cd: too many arguments\n");
                } else if(nbargs == 1){
                    chdir(getenv("HOME"));
                } else {
                    if(chdir(args[1]) != 0){
                        fprintf(stderr, "cd: %s: %s\n", args[1], strerror(errno));
                    }
                }
            } else {
                spawn_proc(STDIN_FILENO, STDOUT_FILENO, NULL, args, nbargs);
            }
            free(args);
        }
        free(pipes);
        free(currentpath);
    }
    free(entry);
    return 0;
}
