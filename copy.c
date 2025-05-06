#define _GNU_SOURCE // Sert pour la copie de fichier/répertoire.
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>

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
    // Je reset le errno car il était set à autre chose lorsque je faisais deux copy d'affiler
    // (Il doit y avoir un truc à régler quelque part mais je ne sais pas)
    errno = 0;
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
