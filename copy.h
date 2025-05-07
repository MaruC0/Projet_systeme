#ifndef _COPY_H_
#define _COPY_H_

#define _GNU_SOURCE // Sert pour la copie de fichier/répertoire.
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>

/*  Permet d'ajouter un nom de fichier au path pour obtenir son path absolue.  */
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

/*  Copie un fichier source vers un fichier cible.  */
void file_copy(const char* source, const char* target){
    int file1 = open(source, O_RDONLY);
    if(file1 == -1){
        perror("Erreur lors de l'ouverture du fichier source: ");
        return;
    }
    int file2 = open(target, O_WRONLY | O_CREAT | O_EXCL);
    if(file2 == -1){
        if(errno != 17){    // Erreur donc on arrête.
            perror("Erreur lors de l'ouverture du fichier cible: ");
            close(file1);
            return;
        }
        else{   // Le fichier existe déjà donc on le passe.
            perror("Erreur lors de l'ouverture du fichier cible: ");
            return;
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
        return;
    }
    int taille = buffer.st_size;
    ssize_t nb_by;
    while((nb_by = copy_file_range(file1, NULL, file2, NULL, taille, 0)) != 0){     // Copy un bloc dans file2.                                                 , 
        if(nb_by == -1){
            perror("Erreur lors de l'écriture dans le fichier cible: ");
            if(remove(target) == -1){
                perror("Erreur lors de la suppression du fichier cible: ");
            }
            close(file1);
            close(file2);
            return;
        }
    }
    if(chmod(target, buffer.st_mode) == -1){
        perror("Erreur lors du changement des permissions du fichier cible: ");
        if(remove(target) == -1){
            perror("Erreur lors de la suppression du fichier cible: ");
        }
        close(file1);
        close(file2);
        return;
    }
    close(file1);
    close(file2);
}

/*  Copie un répertoire source vers un répertoire cible.  */
void directory_copy(const char* source, const char* target){
    DIR* dirS = opendir(source);
    if(dirS == NULL){
        perror("Erreur lors de l'ouverture du répertoire source: ");
        return;
    }
    DIR* dirC = opendir(target);
    if(dirC == NULL){   // On vérifie que le répertoire cible existe.
        if(errno == 2){     // Si le répertoire cible n'existe pas.
            struct stat bufferS;
            if(stat(source, &bufferS) == -1){
                perror("Erreur lors de la récupération des permissions du fichier source");
                closedir(dirS);
                return;
            }
            if(mkdir(target, bufferS.st_mode) == -1){
                perror("Erreur lors du changement des permissions du répertoire cible");
                closedir(dirS);
                return;
            }
        }
        else{   // Autre erreur.
            perror("Erreur lors de l'ouverture du répertoire cible");
            closedir(dirS);
            return;
        }
    }
    else{
        closedir(dirC);
    }
    struct dirent* dpS;
    while((dpS = readdir(dirS))!= NULL){    // On boucle sur les éléments à l'intérieur du répertoire.
        int dpS_length = strlen(dpS->d_name);
        if(!(dpS->d_name[0] == '.' && dpS_length == 1) && !(dpS->d_name[0] == '.' && dpS->d_name[1] == '.' && dpS_length == 2)){    // On ne copie pas les répertoires . et le ..
            char* path1 = malloc(sizeof(char)*(strlen(source) + dpS_length + 2));
            char* path2 = malloc(sizeof(char)*(strlen(target) + dpS_length + 2));
            path_formatting(source, dpS->d_name, path1);    // On récupére le chemin absolue de l'élément dans le répertoire source.
            path_formatting(target, dpS->d_name, path2);    // On créer le chemin absolue dans le répertoire cible.
            struct stat buffer;
            if(stat(path1, &buffer) == -1){
                free(path1);
                free(path2);
                perror("Erreur lors de la récupération des permissions du fichier source: ");
                closedir(dirS);
                return;
            }
            if(S_ISDIR(buffer.st_mode)){    // Si l'élément est un autre répertoire.
                if(mkdir(path2, 0777) == -1){   // On vérifie qu'on ai les permissions.
                    if(errno != 17){
                        free(path1);
                        free(path2);
                        perror("Erreur lors de la création du répertoire: ");
                        closedir(dirS);
                        return;
                    }
                }
                directory_copy(path1, path2);   // On appelle récursivement la fonction de copy de répertoire.
                if(chmod(path2, buffer.st_mode) == -1){     // On copie les permissions du répertoire.
                    free(path1);
                    free(path2);
                    perror("Erreur lors du changement des permissions du répertoire ");
                    return;
                }
            }
            else{   // Si c'est un fichier.
                file_copy(path1, path2);    // On copie le fichier.
            }
            free(path1);
            free(path2);
        }
    }
    closedir(dirS);
}

#endif
