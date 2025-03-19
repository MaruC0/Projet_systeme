#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>

char currentpath[200];

bool compare(char* str1, char* str2){
    // Compare la chaîne str1 au début de la chaîne str2.
    printf("ATTENTION str1 = %s, str2 = %s ATTENTION\n", str1, str2);
    int i=0;
    if(str1[0] == '\0' && str2[0] == '\0'){ // Cas deux string vide.
        return true;
    }
    if(str1[0] == '\0' || str2[0] == '\0'){ // Cas une des deux string vide
        return false;
    }
    while(str1[i] != '\0'){ // Tant qu'on a pas fini de lire str1
        printf("str 1 : %d, str2 : %d\n", str1[i], str2[i]);
        if ((str1[i] != str2[i] && !((int)str1[i] == 32 && (int)str2[i] == 0)) || str2[i] == '\0'){ // Si les caractères sont différents et que c'est pas l'espace ou que str2 est plus petit que str1
            return false;
        }
        i += 1; 
    }
    return true;
}

void changeDirectory(char* path){
    printf("Path : %s\n", path);
    if (path == NULL){
        strcpy(currentpath,"/~");
    } else if(path[0] == '.' && strlen(path) == 1) {
        printf("Path : BONJOUR %s\n", path);
        return;
    } else if(path == "..") {
        // TODO Si à la racine, ne rien faire, sinon remonter
        return;
    } else if(path[0] == '/'){ // Chemin absolu
        struct stat stats;
        if (stat(path, &stats)){
            strcpy(currentpath,path);
        }
    } else { // Chemin relatif
        char* finalpath = malloc(strlen(path) + strlen(currentpath) + 2);
        strcpy(finalpath,currentpath);
        strcat(finalpath,"/");
        strcat(finalpath,path);
        struct stat stats;
        if (stat(finalpath, &stats) == 0){
            strcpy(currentpath,finalpath);
        }
        free(finalpath);
    }
}

void cutstr(char* str){
    // Formate la string en pour retirer tous les espaces et retour chariot en fin de string
    int i=0;
    // Parcours jusqu'au retour chariot
    while(str[i] != '\n' || str[i] != '\0'){
        i++;
    }
    // Parcours à l'envers tant qu'il y a des caractères vides ou des espaces
    while((int)str[i] == 0 || str[i] == ' '){
        i--;
    }
    // Placement de la balise finale
    str[i+1] = '\0';
}

int main(int argc, char *argv[]){
    char entry[200];
    fgets(entry,sizeof(entry),stdin); // Il faut cut le string au \n
    cutstr(entry);
    //scanf("%s",entry); // PROBLEME : S'ARRETE AUX ESPACES

    getcwd(currentpath,200); // récupère le path actuel

    if(compare("cd ",entry)) { // NE MARHCE PAS AVEC CD + RIEN
        char* start = &entry[3];
        printf("Réussi : entry = %s\n", start);
        changeDirectory(start); // IL FAUT COUPER LE CD
    } else if(compare("./",entry)) {
        /*Spawn a child to run the program.*/
        pid_t pid=fork();
        if (pid==0) { /* child process */
            char* name = &entry[2];
            printf("name %s\n", name);
            /*char* path = malloc(sizeof(entry) + sizeof(currentpath) + 1); // On avait mis ça mais ça me parait inutile ?
            strcpy(path,currentpath);
            strcat(path,"/");
            strcat(path,name);*/
            char *argv2[]={name,NULL};
            execv(argv[1],argv2);
            //free(path); // A relocaliser ? 
            exit(127); /* only if execv fails */
        }
        else { /* pid!=0; parent process */
            waitpid(pid,0,0); /* wait for child to exit */
        }
    }

    printf("Le path actuel est %s\n", currentpath);
    
    return 0;
}
