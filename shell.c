#define _GNU_SOURCE // Sert pour la copie de fichier/répertoire.
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <dirent.h>

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
    char* currentpath = getcwd(NULL, 0);
    printf("%s%s%s$ ", KCYN, currentpath, KNRM);
    free(currentpath);
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
    tab[i] = NULL; // Tableau finis par NULL pour execvp
    return i;
}

void put_job_in_foreground (pid_t pgid, int cont){
    // Donne le terminal au job (groupe de processus qui ne contient techniquement qu'un seul processus et ses enfants dans notre cas)
    tcsetpgrp(STDIN_FILENO, pgid);
    
    if(cont) {
        // On lui dit de continuer si il est interrompu j'ai mis ça sur 1 par défaut car on le stoppe jamais donc au cas ou voilà
        kill(-pgid, SIGCONT);
    }
    
    waitpid(-pgid, 0, WUNTRACED);
    
    // Redonne contrôle au shell
    tcsetpgrp(STDIN_FILENO, shell_pid);
}

void put_job_in_background (pid_t pgid){
    // On ne lui donne pas le terminal, et on le continue si jamais il était interrompu
    kill(-pgid, SIGCONT);
}

pid_t str_to_pid(const char* input){
    // Transforme l'input qui est un char* en pid_t, avec gestion d'erreur si l'entrée est pas un nombre.
    errno = 0;
    char* endptr;
    long val = strtol(input, &endptr, 10);
    
    if (errno != 0 || *endptr != '\0' || val < 1){
        return -1;
    }
    
    return (pid_t)val;
}

void path_formatting(const char* e1, const char* e2, char* res){
    int t1 = 0;
    int t2 = 0;
    while(e1[t1] != '\0'){
        res[t1] = e1[t1];
        t1 += 1;
    }
    if(res[t1-1] != '/'){
        res[t1] = '/';
        t1 += 1;
    }
    while(e2[t2] != '\0'){
        res[t1+t2] = e2[t2];
        t2 += 1;
    }
    res[t1+t2] = '\0';
}

void file_copy(const char* source, const char* target){
    int file1 = open(source, O_RDONLY);
    if(file1 == -1){
        perror("Erreur lors de l'ouverture du fichier source: ");
        exit(EXIT_FAILURE);
    }
    int file2 = open(target, O_WRONLY | O_CREAT | O_EXCL);
    if(file2 == -1){
        if(errno != 17){
            perror("Erreur lors de l'ouverture du fichier cible: ");
            close(file1);
            exit(EXIT_FAILURE);
        }
        else{
            perror("Erreur lors de l'ouverture du fichier cible: ");
            return;
        }
    }
    int taille = 200;
    ssize_t nb_by;
    while((nb_by = copy_file_range(file1, NULL, file2, NULL, taille, 0)) != 0){
        if(nb_by == -1){
            perror("Erreur lors de l'écriture dans le fichier cible: ");
            if(remove(target) == -1){
                perror("Erreur lors de la suppression du fichier cible: ");
            }
            close(file1);
            close(file2);
            exit(EXIT_FAILURE);
        }
    }
    struct stat buffer;
    if(stat(source, &buffer) == -1){
        perror("Erreur lors de la récupération des permissions du fichier: ");
        if(remove(target) == -1){
            perror("Erreur lors de la suppression du fichier cible: ");
        }
        close(file1);
        close(file2);
        exit(EXIT_FAILURE);
    }
    if(chmod(target, buffer.st_mode) == -1){
        perror("Erreur lors du changement des permissions du fichier cible: ");
        if(remove(target) == -1){
            perror("Erreur lors de la suppression du fichier cible: ");
        }
        close(file1);
        close(file2);
        exit(EXIT_FAILURE);
    }
    close(file1);
    close(file2);
}

void directory_copy(const char* source, const char* target){
    DIR* dirS = opendir(source);
    if(dirS == NULL){
        perror("Erreur lors de l'ouverture du répertoire source: ");
        exit(EXIT_FAILURE);
    }
    struct dirent* dpS;
    while((dpS = readdir(dirS))!= NULL){
        int dpS_length = strlen(dpS->d_name);
        if(!(dpS->d_name[0] == '.' && dpS_length == 1) && !(dpS->d_name[0] == '.' && dpS->d_name[1] == '.' && dpS_length == 2)){
            char* path1 = malloc(sizeof(char)*(strlen(source) + dpS_length + 2));
            char* path2 = malloc(sizeof(char)*(strlen(target) + dpS_length + 2));
            path_formatting(source, dpS->d_name, path1);     
            path_formatting(target, dpS->d_name, path2);
            struct stat buffer;
            if(stat(path1, &buffer) == -1){
                free(path1);
                free(path2);
                perror("Erreur lors de la récupération des permissions du fichier source: ");
                closedir(dirS);
                exit(EXIT_FAILURE);
            }
            if(S_ISDIR(buffer.st_mode)){                                        
                if(mkdir(path2, 0777) == -1){                                   
                    if(errno != 17){
                        free(path1);
                        free(path2);                                      
                        perror("Erreur lors de la création du répertoire: ");   
                        closedir(dirS);
                        exit(EXIT_FAILURE);
                    }
                }
                directory_copy(path1, path2);                                 // On remplace l'appelle récursif.
                if(chmod(path2, buffer.st_mode) == -1){
                    free(path1);
                    free(path2);
                    perror("Erreur lors du changement des permissions du répertoire ");
                    exit(EXIT_FAILURE);
                }
            }
            else{                                                               
                file_copy(path1, path2);                          // On remplace la fonction copie fichier.
            }
            free(path1);
            free(path2);
        }
    }
    int c = closedir(dirS);
}

void copy(const char* source, const char* target){
    errno = 0; // Je reset le errno car il était set à autre chose lorsque je faisais deux copy d'affiler (Il doit y avoir un truc à régler quelque part mais je ne sais pas)
    DIR* dirS = opendir(source);
    printf("source = %s\n", source);
    if(errno == 20){ // La source n'est pas un répertoire.
        closedir(dirS);
        file_copy(source, target);
    } else if (errno != 0){   // Sinon c'est une autre erreur.
        closedir(dirS);
        fprintf(stderr, "cp: %s\n", strerror(errno));
    } else{ // Sinon c'est un répertoire.
        closedir(dirS);
        directory_copy(source, target);
    }
}

void exec_command(char** args, uint nbargs, bool background){
    // Execute une ligne de commande simple

    // Gestion de l'entrée vide ou avec uniquement des espaces
    if(!nbargs){
        return;
    }

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
    } else if(strcmp("fg", command) == 0) {
        // Si je fais fg, je mets en foreground le processus passé en paramètre.
        if (nbargs > 1){
            pid_t newpid = str_to_pid(args[1]);
            if (newpid != -1){
                put_job_in_foreground(newpid, 1);
            } else {
                fprintf(stderr, "%s: invalid pid\n", command);
            }
        } else {
            fprintf(stderr, "%s: missing arguments\n", command);
        }
    } else if(strcmp("bg", command) == 0) {
        // Si je fais bg, je mets en background le processus passé en paramètre.
        if (nbargs > 1){
            pid_t newpid = str_to_pid(args[1]);
            if (newpid != -1){
                put_job_in_background(newpid);
            } else {
                fprintf(stderr, "%s: invalid pid\n", command);
            }
        } else {
            fprintf(stderr, "%s: missing arguments\n", command);
        }
    } else if(strcmp("cp", command) == 0){
        if (nbargs > 2){
            copy(args[1], args[2]);
        } else {
            fprintf(stderr, "%s: missing arguments\n", command);
        }
    } else {
        // Gère les entrées sorties si il y a des redirections
        setIO(args, nbargs);
        
        pid_t pid = fork();
        if(pid == 0){
            // On met le fils dans son propre groupe
            setpgid(0,0);
            // On lui donne le contrôle du terminal si il est pas lancé en background
            if(!background){
                tcsetpgrp(STDIN_FILENO, getpid());
            }
            
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
            setpgid(pid,pid);
            
            if(background){
                // Print le pid du processus envoyé à la manière d'un shell classique
                printf("[%d]\n",pid);
            }
            // Si le processus est en foreground, on lui donne le terminal, on l'attend puis il le redonne quand il a fini.
            if(!background){
                tcsetpgrp(STDIN_FILENO,pid);
                waitpid(pid, 0, WUNTRACED);
                tcsetpgrp(STDIN_FILENO,shell_pid);
            }
        }
    }

}

int spawn_proc (int in, int out, char* pipes, bool background) {

    char** args = malloc(strlen(pipes) * sizeof(char*));
    uint nbargs = getArgs(args, pipes, " \n");

    pid_t pid;
    if ((pid = fork()) == 0){
        if (in != 0){
            dup2(in, STDIN_FILENO);
            close(in);
        }

        if (out != 1){
            dup2(out, STDOUT_FILENO);
            close(out);
        }

        exec_command(args, nbargs, background);
        exit(0);
    } else {
        waitpid(pid, 0, 0);
    }

    free(args);
    return pid;
}

int main(int argc, char *argv[]){
    // On ignore tous les signaux qui peuvent fermer le terminal sans le vouloir.
    signal(SIGTTOU, SIG_IGN);  // Ignore le signal envoyé quand un processus tente de prendre contrôle du terminal
    signal(SIGTTIN, SIG_IGN);  // Ignore tentative de lecture stdin sans contrôle
    signal(SIGTSTP, SIG_IGN);  // Ignore Ctrl+Z dans le shell lui-même
    
    // On récupère le pid de notre terminal
    shell_pid = getpid();
       
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
    int in, fd[2];

    while(true) {
        size_t entry_size = askInput(&entry);
        char** pipes = malloc(entry_size * sizeof(char*));
        uint nbcmd = getArgs(pipes, entry, "|\n");
        bool background = false;

        // Gestion entrée vide
        if(!nbcmd){
            free(pipes);
            continue;
        }

        in = 0;

        if (nbcmd > 1){
            if (pipes[nbcmd-1][strlen(pipes[nbcmd-1])-1] == '&' ){
                pipes[nbcmd-1][strlen(pipes[nbcmd-1])-1] = '\0';
                background = true;
            } else if (pipes[nbcmd-1][0] == '&'){       // A supprimer également?
                nbcmd = nbcmd - 1;
                background = true;
            }
            /* The first process should get its input from the original file descriptor 0.  */

            /* Note the loop bound, we spawn here all, but the last stage of the pipeline.  */
            for (int i = 0; i < nbcmd-1; ++i){
                pipe(fd);

                spawn_proc(in, fd[1], pipes[i], false); // J'ai mis ça a false, pour que seul la dernière fonction du pipe soit en background, je sais pas si c'est bien mais dans le cas contraire test1 récupérait la valeur du print du pid quand je faisait ./test2 | ./test1&.

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
            // C'est le bordel je sais et j'en suis désolé faites attention si vous faites le ménage ça fonctionne à peine.
            // On teste pour le background de la dernière fonction ici car getArgs détruit pipes dans l'appel suivant, je le stocke aussi dans un bool différent
            bool lastbackground = false;
            int arg_length = strlen(pipes[0])-1;
            if (pipes[0][arg_length] == '&' ){
                pipes[0][arg_length] = '\0';
                lastbackground = true;
            }
            
            // On parse selon &
            char** bgargs = malloc((arg_length+1) * sizeof(char*));
            uint nb_bgargs = getArgs(bgargs, pipes[0], "&\n");
            background = true;
            
            // Tous les éléments du tableau sauf le dernier sont suivis d'un & donc je les mets en background
            for (int j=0;j<nb_bgargs-1;j++){
                // Obligé de découper selon espace 1 par 1 et de les traiter comme des commandes solo
                char** cmd = malloc(strlen(bgargs[j]) * sizeof(char*));
                uint nb = getArgs(cmd, bgargs[j], " \n");
                exec_command(cmd,nb,background);
                free(cmd);
            }
            
            char** cmd = malloc(strlen(bgargs[nb_bgargs-1]) * sizeof(char*));
            uint nb = getArgs(cmd, bgargs[nb_bgargs-1], " \n");

            exec_command(cmd, nb, lastbackground);

            free(bgargs);
            free(cmd);
        }
        free(pipes);
    }
    free(entry);
    return 0;
}
