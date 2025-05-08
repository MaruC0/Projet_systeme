#include "copy.h"

#include <stdlib.h>
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
char* last_path = NULL;

/* Compare la chaîne str1 au début de la chaîne str2. */
bool compare(char* str1, char* str2){
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
    selon les chevrons présents dans la ligne de commande.
    Renvoie 1 si c'était une redirection de void, 0 sinon.  */
int setIO(char *args[], int nbargs){
    int skip_first = 0;
    //printf("args[0][0] = %s\n", args[1]);
    if(args[0][0] == '>'){  // Si on output la fonction void.
        int trunc = open(args[1], O_RDWR | O_CREAT | O_TRUNC, 0777);    // on écrase le fichier.
        close(trunc);
        if(nbargs > 2){     // S'il y a une autre instruction, on continue.
            skip_first = 2;
        }
        else{               // Sinon, on s'arrête.
            return 1;
        }
    }
    else if(args[0][0] == '<'){     // Si on input dans la fonction void
        if(nbargs > 2){     // S'il y a une autre instruction, on continue.
            skip_first = 2;
        }
        else{               // Sinon, on s'arrête.
            return 1;
        }
    }
    int fdi = -2, fdo = -2;
    for (int i=1+skip_first; i<nbargs; i++){
        if(args[i][0] == '<' && nbargs > i+1){
            fdi = open(args[i+1], O_RDWR | O_CREAT | O_TRUNC, 0777);
            args[i] = NULL;
        } else if(args[i][0] == '>' && nbargs > i+1){
            fdo = open(args[i+1], O_RDWR | O_CREAT | O_TRUNC, 0777);
            args[i] = NULL;
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
    return 0;
}

/*  Free toutes les cases de args  */
void free_case(char** args, int nbargs){
    for(int i=0; i<=nbargs; i+=1){
        free(args[i]);
    }
}

/*  Alloue un nouveau tableau d'args dans lequel tous les < et les > sont dans leur propre case.
    Renvoie NULL s'il y a une erreur de syntaxe  */
char** seperateIO(char* tab[], int nbargs_before_separation, int* nbargs){
    int hidden_redirection_count = 0;
    for(int i = 0; i < nbargs_before_separation; i+=1){     // On compte le nombre de redirection pour allouer un nouveau tableau.
        int j = 0;
        while(tab[i][j] != '\0'){
            if((tab[i][j] == '<' || tab[i][j] == '>') && (tab[i][j+1] != '\0' || j != 0)){
                hidden_redirection_count += 2;
            }
            j+=1;
        }
    }
    *nbargs = nbargs_before_separation+hidden_redirection_count;
    char** args = malloc(sizeof(char*)*(*nbargs+1));
    int ibs = 0;
    int ias = 0;
    bool following = false;     // Pour détécter les erreurs de syntaxe.
    while(ibs < nbargs_before_separation){
        int jbs = 0;
        int jas = 0;
        args[ias] = malloc(sizeof(char)*(strlen(tab[ibs])+1));   // Obligé d'avoir une string de base pour travailler sur les caractères un par un.
        bool detected = false;
        while(tab[ibs][jbs] != '\0'){
            detected = false;
            if(tab[ibs][jbs] == '<' || tab[ibs][jbs] == '>'){   // S'il y a une redirection
                if(following){  // Si deux redirections se suivent.
                    fprintf(stderr, "syntax error near unexpected token `%c'\n", tab[ibs][jbs]);
                    free_case(args, ias);
                    free(args);
                    return NULL;
                }
                if(jbs != 0){       // Si c'est pas le premier caractère        ex: ab<cd
                    args[ias][jas] = '\0';
                    args[ias+1] = malloc(sizeof(char)*2);
                    args[ias+1][0] = tab[ibs][jbs];
                    args[ias+1][1] = '\0';
                    args[ias+2] = malloc(sizeof(char)*(strlen(tab[ibs])+1));
                    ias += 2;
                    jas = 0;
                    detected = true;    // Pour éviter les erreurs d'incrémentation de ias dans le cas où il y a rien après.      ex: ab<
                }
                else if(tab[ibs][jbs+1] != '\0'){   // Sinon, on vérifie qu'il n'est pas seul.         ex: <cd
                    args[ias][jas] = tab[ibs][jbs];
                    args[ias][jas+1] = '\0';
                    args[ias+1] = malloc(sizeof(char)*(strlen(tab[ibs])+1));
                    ias += 1;
                    jas = 0;
                }
                else{   // Sinon, c'est une redirection qui est déjà dans sa propre case.
                    args[ias][jas] = tab[ibs][jbs];
                    jas +=1;
                }
                following = true;
            }
            else{   // Si ce n'est pas une redirection
                args[ias][jas] = tab[ibs][jbs];
                jas += 1;
                following = false;
            }
            jbs += 1;
        }
        if(!detected){
            args[ias][jas] = '\0';
            ias += 1;
        }
        ibs += 1;
    }
    args[ias] = NULL;
    *nbargs = ias;
    return args;
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
        if(last_path != NULL) free(last_path);
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

/*  Donne le terminal au job spécificé en paramètre, l'attend puis redonne le terminal au shell.
    Si cont est true, continue le job si il était stoppé  */
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

/*  Fait continuer le job donné en paramètre sans lui donner accès au terminal  */
void put_job_in_background (pid_t pgid){
    // Le processus ne prend pas le terminal, et il continue s'il était interrompu
    kill(-pgid, SIGCONT);
}

/*  Convertis la string d'entrée en pid_t
    Renvoie -1 si la conversion est impossible  */
pid_t str_to_pid(const char* input){
    errno = 0;
    char* endptr;
    long val = strtol(input, &endptr, 10);

    if (errno != 0 || *endptr != '\0' || val < 1){
        return -1;
    }

    return (pid_t)val;
}

/*  Exécute une ligne de commande simple,
    et la place en background si spécifié  */
void exec_command(char* line, bool background){

    // Parsage de la ligne de commande sur les espaces et les retours à la ligne
    char** args_before_seperation = malloc((strlen(line)+1) * sizeof(char*));
    uint nbargs_before_separation = getArgs(args_before_seperation, line, " \n");

    // Gestion de l'entrée vide ou avec uniquement des espaces
    if(nbargs_before_separation == 0){
        return;
    }

    // Gère les entrées sorties si il y a des redirections
    int nbargs = 0;
    char** args = seperateIO(args_before_seperation, nbargs_before_separation, &nbargs);
    free(args_before_seperation);
    if(args == NULL){
        return;
    }
    if(setIO(args, nbargs)){
        free_case(args, nbargs);
        free(args);
        return;
    }
    char* command = args[0];

    if(strcmp(command, "exit") == 0) {
        free_case(args, nbargs);
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
                put_job_in_foreground(newpid, true);
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
        if(strcmp("-r", args[1]) == 0){    // copie de répertoire
            if(nbargs > 3){
                directory_copy(args[2], args[3]);
            } else {
                fprintf(stderr, "cp: missing arguments\n");
            }
        } else {                                  // copie de fichier
            if (nbargs > 2){
                file_copy(args[1], args[2]);
            } else {
                fprintf(stderr, "cp: missing arguments\n");
            }
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

            // On exécute la fonction
            execvp(args[0], args);

            // Seulement si le exec ne passe pas, exécute la suite
            if(compare("./", command) || compare("../", command) || command[0] == '/'){
                // Ici, l'erreur devrait être 'no such file or directory'
                fprintf(stderr, "%s: %s\n", command, strerror(errno));
            } else {
                fprintf(stderr, "%s: command not found\n", command);
            }
            free_case(args, nbargs);
            free(args);
            if(last_path != NULL) free(last_path);
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
    free_case(args, nbargs);
    free(args);
}

/*  Crée un processus qui va exécuter une commande simple
    sur les entrées et sorties des file descriptors spécifiés en paramètre  */
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

/*  Exécute une ligne de commande contenant potentiellement des pipes  */
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
        // Détection de & pour mise en background
        char* last_cmd = pipes[nbcmd-1];
        ulong i_last_char = strlen(last_cmd)-1;

        // Détection de & placer ailleurs que dans la dernière commande
        for(int i = 0; i<nbcmd-1; i+=1){
            int last_non_space = strlen(pipes[i]) -1;
            while(last_non_space > 0 && pipes[i][last_non_space] == ' '){
                last_non_space -= 1;
            }
            if(pipes[i][last_non_space] == '&'){
                fprintf(stderr, "syntax error near unexpected token `|'\n");
                return;
            }
        }

        // Détection de & à la fin de la dernière commande
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
        // Détection de & pour la mise en background
        bool last_bg = false;
        int i_last_char = strlen(pipes[0])-1;
        if (pipes[0][i_last_char] == '&' ){
            pipes[0][i_last_char] = '\0';
            last_bg = true;
        }

        // On parse selon &
        char** bgargs = malloc((i_last_char+2) * sizeof(char*));
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
        while(entry[i] != '\0' && i < entry_size-1){
            // Cas pour une entrée complètement vide, avec une redirection de l'entrée par exemple
            if(entry[i] == -66){
                break;
            } else if(entry[i] == '&' && entry[i+1] == '&'){
                entry[i] = '\0';
                cmd_start = i+2;
                commands[nb_cmd] = &entry[cmd_start];
                nb_cmd++;
                i++;
            }
            i++;
        }
        entry[i] = '\0';

        // Exécution de toutes les commandes séparémment
        for(int i = 0; i<nb_cmd; i++){
            exec_command_line(commands[i], strlen(commands[i]));
        }

        free(commands);
        free(entry);
        fflush(stdout);
    }

    return 0;
}
