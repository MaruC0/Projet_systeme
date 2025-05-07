#include "copy.c"

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <termios.h>
#include <signal.h>
#include <unistd.h>

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

// Variable globale qui garde en mémoire le pid de notre shell
pid_t shell_pid;
int shell_stdout;

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

/*  Redirige les entrées et sorties du processus courant
    selon les chevrons présents dans la ligne de commande  */
void setIO(char *args[], int nbargs){
    int fdi = -2, fdo = -2;
    for (int i=1; i<nbargs; i++){
        if(args[i][0] == '<' && nbargs > i+1){
            fdi = open(args[i+1], O_RDWR | O_CREAT | O_TRUNC, 0777);
        } else if(args[i][0] == '>' && nbargs > i+1){
            fdo = open(args[i+1], O_RDWR | O_CREAT | O_TRUNC, 0777);
        }
    }
    if (fdi >= 0){
        dup2(fdi, STDIN_FILENO);
        close(fdi);
    } else if(fdi == -1) {
        printf("erreur fdi: %s\n", strerror(errno));
    }
    if (fdo >= 0){
        dup2(fdo, STDOUT_FILENO);
        close(fdo);
    } else if(fdo == -1){
        printf("erreur fdo: %s\n", strerror(errno));
    }
}

/*  Affiche le path actuel, demande une entrée,
    et la place dans la variable passée en paramètre.  */
size_t askInput(char** entry){
    char* currentpath = getcwd(NULL, 0);
    printf("%s%s%s$ ", KCYN, currentpath, KNRM);
    free(currentpath);
    size_t taille = 0;
    getline(entry, &taille, stdin);
    if(taille < 0){
        free(*entry);
        perror("askInput");
        exit(EXIT_FAILURE);
    }
    return taille;
}

/*  Parse la string entry selon les caractères dans la string seprators,
    qui servent de délimiteurs, et places chaque token dans le tableau tab.
    Si separators est NULL, les séparateur spar défauts sont ' ' et '\n'  */
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
    tab[i] = NULL; // Tableau finis par NULL pour execvp
    return i;
}

void put_job_in_foreground (pid_t pgid, bool cont){
    // Donne le terminal au job (groupe de processus qui ne contient techniquement qu'un seul processus et ses enfants dans notre cas)
    tcsetpgrp(STDIN_FILENO, pgid);

    if(cont) {
        // Il continue s'il est interrompu
        // cont par défaut à 1 dans les appels car on ne le stope jamais
        kill(-pgid, SIGCONT);
    }

    waitpid(-pgid, 0, WUNTRACED);

    // Redonne contrôle au shell
    tcsetpgrp(STDIN_FILENO, shell_pid);
}

void put_job_in_background (pid_t pgid){
    // Le processus ne prend pas le terminal, et il continue s'il était interrompu
    kill(-pgid, SIGCONT);
}

/*  Converti la string d'entrée en pid_t
    Renvoi -1 si la conversion est impossible */
pid_t str_to_pid(const char* input){
    errno = 0;
    char* endptr;
    long val = strtol(input, &endptr, 10);

    if (errno != 0 || *endptr != '\0' || val < 1){
        return -1;
    }

    return (pid_t)val;
}

/*  Execute une ligne de commande simple,
    et la place en background si spécifié  */
void exec_command(char* line, bool background){

    // Parsage de la ligne de commande sur les espaces et les retours à la ligne
    char** args = malloc(strlen(line) * sizeof(char*));
    uint nbargs = getArgs(args, line, " \n");

    // Gestion de l'entrée vide ou avec uniquement des espaces
    if(nbargs == 0){
        return;
    }

    // Gère les entrées sorties si il y a des redirections
    setIO(args, nbargs);

    char* command = args[0];

    if(strcmp(command, "exit") == 0) {
        free(args);
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
    } else if(strcmp("fg", command) == 0) {
        // Le processus passé en paramètre est mis en foreground
        if (nbargs > 1){
            pid_t newpid = str_to_pid(args[1]);
            if (newpid != -1){
                put_job_in_foreground(newpid, 1);
            } else {
                fprintf(stderr, "fg: invalid pid\n");
            }
        } else {
            fprintf(stderr, "fg: missing arguments\n");
        }
    } else if(strcmp("bg", command) == 0) {
        // Le processus passé en paramètre est mis en background
        if (nbargs > 1){
            pid_t newpid = str_to_pid(args[1]);
            if (newpid != -1){
                put_job_in_background(newpid);
            } else {
                fprintf(stderr, "bg: invalid pid\n");
            }
        } else {
            fprintf(stderr, "bg: missing arguments\n");
        }
    } else if(strcmp("cp", command) == 0){
        if (nbargs > 2){
            copy(args[1], args[2]);
        } else {
            fprintf(stderr, "cp: missing arguments\n");
        }
    } else {

        pid_t pid = fork();
        if(pid == 0){
            // On met le fils dans son propre groupe
            setpgid(pid, pid);

            // On lui donne le contrôle du terminal s'il n'est pas lancé en background
            if(!background){
                tcsetpgrp(STDIN_FILENO, pid);
            }

            // Le processus s'arrêtera si une entrée est demandée
            // ou si un autre signal d'arrêt lui est envoyé
            signal (SIGINT, SIG_DFL);
            signal (SIGQUIT, SIG_DFL);
            signal (SIGTSTP, SIG_DFL);
            signal (SIGTTIN, SIG_DFL);
            signal (SIGTTOU, SIG_DFL);
            signal (SIGCHLD, SIG_DFL);

            // On éxécute la fonction
            execvp(args[0], args);

            // Seulement si le exec ne passe pas, éxecute la suite
            if(compare("./", command) || compare("../", command) || command[0] == '/'){
                // Ici, l'erreur devrait être 'no such file or directory'
                fprintf(stderr, "%s: %s\n", command, strerror(errno));
            } else {
                fprintf(stderr, "%s: command not found\n", command);
            }
            free(args);
            exit(127);
        } else{
            // On met le fils dans son propre groupe
            setpgid(pid, pid);

            if(background){
                // Print le pid du processus envoyé à la manière d'un shell classique
                printf("[%d]\n",pid);
            } else {
                // Si le processus est en foreground, on lui donne le terminal, on l'attend puis il le redonne quand il a fini.
                tcsetpgrp(STDIN_FILENO, pid);
                waitpid(pid, 0, WUNTRACED);
                tcsetpgrp(STDIN_FILENO, shell_pid);


            }
        }
    }
    // Redonne la sortie standard au shell
    // Force l'affichage des autres processus avant de rétablir la sortie standard
    fflush(stdout);
    dup2(shell_stdout, STDOUT_FILENO);

    free(args);
}

/*  Crée un processus qui va exécuter une command simple
    sur les entrées et sorties des files descriptor spécifiés en paramètres */
void spawn_proc (int in, int out, char* pipes, bool background) {

    pid_t pid = fork();
    if (pid == 0){
        if (in != 0){
            dup2(in, STDIN_FILENO);
            close(in);
        }

        if (out != 1){
            dup2(out, STDOUT_FILENO);
            close(out);
        }

        exec_command(pipes, background);
        exit(0);
    } else {
        waitpid(pid, 0, 0);
    }

    return;
}

/*  Exécute une ligne de commande contenant potentiellment des pipes  */
void exec_command_line(char* line, size_t size){

    char** pipes = malloc(size * sizeof(char*));
    uint nbcmd = getArgs(pipes, line, "|\n");

    // Gestion entrée vide
    if(!nbcmd){
        free(pipes);
        return;
    }

    bool background = false;
    int fd[2], in;

    if (nbcmd > 1){
        /* Faire vérif & ailleurs que dernier pipe */
        char* last_cmd = pipes[nbcmd-1];
        ulong i_last_char = strlen(last_cmd)-1;
        if (pipes[nbcmd-1][0] == '&' && i_last_char == 0){
            nbcmd = nbcmd - 1;
            background = true;
        } else if (last_cmd[i_last_char] == '&' ){
            last_cmd[i_last_char] = '\0';
            background = true;
        }
        // Le premier processus doit prendre comme entrée le file descriptor original
        in = 0;

        // On crée tous les processus dans les pipes, sauf le dernier
        for (int i = 0; i < nbcmd-1; ++i){
            pipe(fd);

            spawn_proc(in, fd[1], pipes[i], false);

            close(fd[1]);

            in = fd[0];
        }
        // Dernière commande prend sont entrée depuis le dernier pipe et affiche sur la sortie standard
        spawn_proc(in, STDOUT_FILENO, pipes[nbcmd-1], background);

        close(fd[0]);
        close(fd[1]);

        // Remet la lecture du shell à l'entrée standard
        if (in != 0){
            dup2(0, in);
        }

    } else {
        // A FAIRE
        // Vérifier qu'il n'y a pas trop de &, -> erreur syntaxe
        bool last_bg = false;
        int i_last_char = strlen(pipes[0])-1;
        if (pipes[0][i_last_char] == '&' ){
            pipes[0][i_last_char] = '\0';
            last_bg = true;
        }

        // On parse selon &
        char** bgargs = malloc((i_last_char+1) * sizeof(char*));
        uint nb_bgargs = getArgs(bgargs, pipes[0], "&\n");

        // Tous les éléments du tableau sauf le dernier sont suivis d'un &
        // donc ils vont en background
        for (int j=0; j < nb_bgargs-1; j++){
            exec_command(bgargs[j], true);
        }

        exec_command(bgargs[nb_bgargs-1], last_bg);

        free(bgargs);
    }
    free(pipes);
}

int main(int argc, char *argv[]){

    // Initialisation du shell

    // On ignore tous les signaux qui peuvent fermer le terminal sans le vouloir.
    signal(SIGTTOU, SIG_IGN);  // Ignore le signal envoyé quand un processus tente de prendre contrôle du terminal
    signal(SIGTTIN, SIG_IGN);  // Ignore tentative de lecture stdin sans contrôle
    signal(SIGTSTP, SIG_IGN);  // Ignore Ctrl+Z dans le shell lui-même

    // On récupère le pid de notre terminal et le file descriptor de la sortie standard
    shell_pid = getpid();
    shell_stdout = dup(STDOUT_FILENO);

    // On le met dans son propre groupe
    if (setpgid(shell_pid,shell_pid) < 0) {
        perror("setpgid");
        exit(1);
    }

    // On lui donne contrôle du terminal
    if (tcsetpgrp(STDIN_FILENO,shell_pid) < 0) {
        perror("tcsetpgrp");
        exit(1);
    }

    char* entry = NULL;

    // Boucle principale du shell
    while(true) {
        size_t entry_size = askInput(&entry);

        char** commands = malloc(entry_size * sizeof(char*));

        uint nb_cmd = 1;
        ulong i = 0, cmd_start = 0;

        // Parsage selon '&&'
        commands[0] = entry;
        while(entry[i] != '\n' && i <= entry_size){
            if(entry[i] == '&' && entry[i+1] == '&'){
                entry[i] = '\0';
                cmd_start = i+2;
                commands[nb_cmd] = &entry[cmd_start];
                nb_cmd++;
                i++;
            }
            i++;
        }
        // Peut se faire sur une addresse qui ne fait pas partie du malloc
        // si la tailler alouer automatiquement pour entry fait exactement la taille de l'entrée.
        // Pour l'instant ce n'est jamais arrivée.
        entry[i+1] = '\0';

        // Exécution de toutes les commandes séparémment
        for(int i = 0; i<nb_cmd; i++){
            exec_command_line(commands[i], strlen(commands[i]));
        }

        free(commands);
        free(entry);
    }

    return 0;
}