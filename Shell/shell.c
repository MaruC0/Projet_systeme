#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

char currentpath[200];

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

void askInput(char* entry, unsigned int maxLength){
    /* Affiche le path actuel, demande une entrée,
    et la place dans la variable passée en paramètre. */
    printf("%s $ ", currentpath);
    fgets(entry, maxLength, stdin);
}

void cutstr(char* str){
    // Formate la string en pour retirer tous les espaces et retour chariot en fin de string
    int i=0;
    // Parcours jusqu'au retour chariot
    while(str[i] != '\n' && str[i] != '\0'){
        i++;
    }
    // Parcours à l'envers tant qu'il y a des espaces
    while(str[i-1] == ' '){
        i--;
    }
    // Placement de la balise finale
    str[i] = '\0';
}

void getArgs(char* entry, char* tab[]){
    const char * separators = " \n";
    int i = 0;
    char * strToken = strtok ( entry, separators );
    while ( strToken != NULL ) {
        tab[i] = strToken;
        printf ( "%s ", tab[i] );
        // On demande le token suivant.
        strToken = strtok ( NULL, separators );
        i++;
    }
    printf("\n");
}

int main(int argc, char *argv[]){

    char entry[200];
    char *tab[100];
    
    do {
        getcwd(currentpath, 200);
        askInput(entry, sizeof(entry));
        getArgs(entry,tab);

        if(compare("cd", tab[0])) {
            printf("%s\n", tab[1]);
            if(chdir(tab[1]) != 0){
                printf("cd: Ne peut pas aller au dossier %s", tab[1]);
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
        }

    } while(strcmp(entry, "exit"));
    
    return 0;
}
