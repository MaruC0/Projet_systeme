#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define ENTRY_SIZE 500
#define PATH_SIZE 500

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

void askInput(char* entry){
    /* Affiche le path actuel, demande une entrée,
    et la place dans la variable passée en paramètre. */
    printf("%s $ ", currentpath);
    fgets(entry, ENTRY_SIZE, stdin);
}

void cutstr(char** ptr_str){
    // Formate la string en pour retirer tous les espaces et retour chariot en fin de string
    
    char* str = *ptr_str;
    int start = 0;
    while(str[start] == ' '){
        start++;
    }
    printf("Start char : %c\n", str[start]);
    // Positionnement du début de la commande
    *ptr_str = &str[start];
    
    int end = 1;
    // Parcours jusqu'au retour chariot
    while(str[end] != '\n' && str[end] != '\0'){
        end++;
    }
    // Parcours à l'envers tant qu'il y a des espaces
    while(str[end-1] == ' '){
        end--;
    }
    // Placement de la balise finale
    str[end] = '\0';
    printf("Inside '%s'\n", str);
}

int main(int argc, char *argv[]){

    char* entry = malloc(ENTRY_SIZE);
    do {
        getcwd(currentpath, PATH_SIZE);
        askInput(entry);
        cutstr(&entry);
        
        printf("'%s'\n", entry);
        break;
        
        if(compare("cd ", entry)) {
            // Recherche de l'indice du début du path
            int start = 3;
            while(entry[start] == ' '){
                start++;
            }
            // Recherche de l'indice de fin du path pour le message d'erreur
            // si plusieurs paramètres ont été donnés à cd
            int end = start + 1;
            while (entry[end] != ' ' && entry[end] != '\0') {
                end++;
            }
            entry[end] = '\0';
            if(chdir(&entry[start]) != 0){
                printf("cd: Ne peut pas aller au dossier %s\n", &entry[start]);
            }
        } else if(compare("./", entry)) {
            /*Spawn a child to run the program.*/
            pid_t pid = fork();
            if (pid == 0) { /* child process */
                char* name = &entry[2];
                printf("name %s\n", name);
                char *argv2[] = {name, NULL};
                execv(argv[1], argv2);
                exit(127); /* only if execv fails */
            } else { /* pid!=0; parent process */
                waitpid(pid, 0, 0); /* wait for child to exit */
            }
        } else {
            printf("%s : commande non reconnu\n", entry);
        }

    } while(strcmp(entry, "exit"));
    
    return 0;
}
