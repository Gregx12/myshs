#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <signal.h>



// Definicije konstant
#define MAX_LINE 100 // The maximum length of a command 
#define MAX_ARGS 10  // The maximum number of arguments
#define MAX_TOKENS 21 // The maximum number of tokens 
#define MAX_PATHS 10 // The maximum number of paths 
#define MAX_PATH_LENGTH 50 // The maximum length of a path 
#define MAX_NAME_LENGTH 8 // The maximum length of a name


// Definicije globalnih spremenljivk
int dirchange_command(char *path);
const char *podprti_ukazi[] = {
    "help",
    "status",
    "exit",
    "name",
    "print",
    "echo",
    "pid",
    "ppid",
    "dirchange",
    "dirwhere",
    "dirbase",
    "dirmake",
    "dirremove",
    "dirlist",
    "linkhard",
    "linksoft",
    "linkread",
    "linklist",
    "unlink",
    "remove",
    "rename",
    "cpcat",
    "sysinfo",
    "shellinfo",
    "proc",
    "pids",
    "pinfo",
    "waitone",
    "waitall",
    "piepes"
    // TODO
};
int zadnji_status = 0; // Zadnji status izhoda
char ime_lupine[MAX_NAME_LENGTH + 1] = "mysh"; // Ime lupine
int input_descritor = STDIN_FILENO; // vhodni deskriptor
int output_descritor = STDOUT_FILENO; // izhodni deskriptor
int error_descritor = STDERR_FILENO; // izhodni deskriptor za napake
char *arguments[MAX_ARGS]; // argumenti
char cwd[1024]; // trenutna delovna mapa
char proc_path[1024] = "/proc"; // pot do /proc
typedef struct {
    char pid[6];  // PID
    char ppid[6]; // PPID
    char state[7]; // State
    char name[256]; // Name
} ProcessInfo;


int read_args(char *line, int i){
    int in_quotes = 0;
    char* token = strtok(line, " ");
    while (token != NULL && token[0] != '\n' && token[0] != '\r' && token[0] != '#' && i < MAX_ARGS)
        {
            char* newline_pos = strchr(token, '\n');
            if (newline_pos != NULL) {
                *newline_pos = '\0';
            }

            // Preveri, ali je trenutni znak začetek ali konec narekovajev
            if (token[0] == '"')
            {
                // Če je začetek narekovajev, nastavi 'in_quotes' na 1
                in_quotes = 1;
                //odstrani prvi znak token (narekovaj)
                token++;
            }
            // Če nismo znotraj narekovajev, loči simbole s presledki
            if (!in_quotes)
            {
                arguments[i++] = token;
            }
            else
            {
                char temp[MAX_LINE] = " ";
                strcat(temp, strtok(NULL, "\""));
                strcat(token, temp);
                in_quotes = 0;
                arguments[i++] = token;
            }
            token = strtok(NULL, " ");
        }
    arguments[i] = NULL;
    return i;
}

int compare(const void *a, const void *b) {
    return (*(int *)a - *(int *)b);
}
int compare_process(const void *a, const void *b) {
    const ProcessInfo *pa = (const ProcessInfo *)a;
    const ProcessInfo *pb = (const ProcessInfo *)b;
    int pid_a = atoi(pa->pid);
    int pid_b = atoi(pb->pid);
    return pid_b - pid_a;
}

void sigchld_handler(int sig) {
    int status;
    pid_t child_pid;
    while((child_pid = waitpid(-1, &status, WNOHANG)) > 0){
        if(WIFEXITED(status)){
            zadnji_status = WEXITSTATUS(status);
        }
    }
}

//TUKAJ NAPREJ SO UKAZI

//ZUNANJI UKAZI

int execute_command(int i, int i_red, int o_red, int back){
    if (strcmp(arguments[0], "cd") == 0) {
        if (arguments[1] == NULL) {
            // No directory provided, change to the user's home directory
            dirchange_command(getenv("HOME"));
        } else {
            // Change to the specified directory
            dirchange_command(arguments[1]);
        }
        // Set the status to indicate successful execution
        zadnji_status = 0;
        return 0;
    }
    //izvede ukaz
    fflush(stdout);
    int pid = fork();
    fflush(stdout);
    if (pid == 0) {
        // otrok
        if (i_red != 0) {
            // preusmeri stdin
            close(STDIN_FILENO);
            dup2(input_descritor, STDIN_FILENO);
            close(input_descritor);
        }
        if (o_red != 0) {
            // preusmeri stdout
            close(STDOUT_FILENO);
            dup2(output_descritor, STDOUT_FILENO);
            close(output_descritor);
        }
        // izvedi ukaz
        execvp(arguments[0], arguments);
        // če se je izvajanje vrnilo, je prišlo do napake
        write(error_descritor, "napaka: ", 8);
        write(error_descritor, strerror(errno), strlen(strerror(errno)));
        write(error_descritor, "\n", 1);
        fflush(stdout);
        exit(1);
    }
    else if (pid > 0) {
        // starš
        if (back == 0) {
            // čakaj na otroka
            waitpid(pid, &zadnji_status, 0);
        }
        else {
            // ne čakaj na otroka
            zadnji_status = 0;
        }
    }
    else {
        // napaka
        write(error_descritor, "napaka: ", 8);
        write(error_descritor, strerror(errno), strlen(strerror(errno)));
        write(error_descritor, "\n", 1);
        fflush(stdout);
        zadnji_status = 1;
    }
    return 0;
}

//PREPROSTI VGRAJENI UKAZI

int help_command() {

    // Izpiše vse podprte ukaze
    printf("Podprti ukazi:\n");
    for (int i = 0; i < sizeof(podprti_ukazi) / sizeof(podprti_ukazi[0]); i++) {
        write(output_descritor, "- ", 2);
        write(output_descritor, podprti_ukazi[i], strlen(podprti_ukazi[i]));
        write(output_descritor, "\n", 1);
    }
    zadnji_status = 0;
    return 0;
}

int status_command() {
    //izpiše zadnji status
    char buffer[20];        // Buffer to hold the converted string
    int length = snprintf(buffer, sizeof(buffer), "%d", zadnji_status);
    if (length >= 0 && length < sizeof(buffer)) {
        write(output_descritor, buffer, length);
    }
    write(output_descritor, "\n", 1);
    return 0;
}

int exit_command(int status) {
    //izhod iz lupine
    exit(status);
}

int name_command(char name[]) {
    //nastavi ime lupine oziroma izpiše obstoječe ime
    if(strcmp(name, ime_lupine) == 0){
        write(output_descritor, ime_lupine, strlen(ime_lupine));
        write(output_descritor, "\n", 1);
        zadnji_status = 0;
        return 0;
    }
    if (strlen(name) > MAX_NAME_LENGTH) {
        zadnji_status = 1;
        return 1;
    }
    if (name == NULL) {
        //printf("%s\n", ime_lupine);
        write(output_descritor, ime_lupine, strlen(ime_lupine));
        zadnji_status = 0;
        return 0;
    }
    strcpy(ime_lupine, name);
    zadnji_status = 0;
    return 0;
}

int print(int argc, char *argv[]) {
    //izpiše vse argumente
    if(argc == 1){
        zadnji_status = 0;
        return 0;
    }
    for (int i = 1; i < argc-1; i++) {
        write(output_descritor, argv[i], strlen(argv[i]));
        write(output_descritor, " ", 1);
    }
    //do not print if last argument is space
    if(strcmp(argv[argc-1], "\n") != 0){
        write(output_descritor, argv[argc-1], strlen(argv[argc-1]));
    }
    zadnji_status = 0;
    return 0;
}

int echo(int argc, char *argv[]){
    //izpiše vse argumente s skokom v novo vrstico
    if(argc == 1){
        write(output_descritor, "\n", 1);
        zadnji_status = 0;
        return 0;
    }
    for (int i = 1; i < argc-1; i++) {
        write(output_descritor, argv[i], strlen(argv[i]));
        write(output_descritor, " ", 1);
    }
    write(output_descritor, argv[argc-1], strlen(argv[argc-1]));
    char newline = '\n';
    write(output_descritor, &newline, 1);
    zadnji_status = 0;
    return 0;
}

int pid_command() {
    //izpiše PID procesa
    pid_t pid = getpid();
    char pid_str[16];
    int pid_len = sprintf(pid_str, "%d\n", pid);
    write(output_descritor, pid_str, pid_len);
    write(output_descritor, "\n", 1);
    zadnji_status = 0;
    return 0;
}

int ppid_command(){
    //izpiše PPID procesa
    pid_t ppid = getppid();
    char ppid_str[16];
    int ppid_len = sprintf(ppid_str, "%d\n", ppid);
    write(output_descritor, ppid_str, ppid_len);
    write(output_descritor, "\n", 1);
    zadnji_status = 0;
    return 0;
}

//DELO Z IMENIKI

int dirchange_command(char *path) {
    //spremeni trenutno delovno mapo
    if (path == NULL) {
        chdir("/.");
        getcwd(cwd, sizeof(cwd));
        zadnji_status = 0;
        return 0;
    }
    if (chdir(path) == -1) {
        int status = errno;
        perror("dirchange");
        fflush(stdout);
        zadnji_status = status;
        return status;
    }
    getcwd(cwd, sizeof(cwd));
    zadnji_status = 0;
    return 0;
}

int dirwhere_command(){
    //izpiše trenutni delovni imenik
    write(output_descritor, cwd, strlen(cwd));
    write(output_descritor, "\n", 1);
    zadnji_status = 0;
    return 0;
}

int dirbase_command(){
    //izpis osnove trenutnega delovnega imenika
    char *base = basename(cwd);
    write(output_descritor, base, strlen(base));
    write(output_descritor, "\n", 1);
    zadnji_status = 0;
    return 0;
}

int dirmake_command(char* path){
    //ustvari nov direktorij
    if (mkdir(path, 0777) == -1) {
        int status = errno;
        perror("dirmake");
        fflush(stdout);
        zadnji_status = status;
        return status;
    }
    zadnji_status = 0;
    return 0;
}

int dirremove_command(char* path){
    //odstrani direktorij
    if (rmdir(path) == -1) {
        int status = errno;
        perror("dirremove");
        fflush(stdout);
        zadnji_status = status;
        return status;
    }
    zadnji_status = 0;
    return 0;
}

int dirlist_command(char *path){
    //izpiše vsebino direktorija
    DIR *dir;
    struct dirent *entry;
    if (path == NULL) {
        dir = opendir(".");
    }
    else {
        dir = opendir(path);
    }
    if (dir == NULL) {
        int status = errno;
        perror("dirlist");
        fflush(stdout);
        zadnji_status = status;
        return status;
    }
    while ((entry = readdir(dir)) != NULL) {
        write(output_descritor, entry->d_name, strlen(entry->d_name));
        write(output_descritor, "  ", 2);
    }
    write(output_descritor, "\n", 1);
    closedir(dir);
    zadnji_status = 0;
    return 0;
}

//DELO Z DATOTEKAMI

int linkhard_command(char *path1, char *path2){
    //ustvari trdi link
    if (link(path1, path2) == -1) {
        int status = errno;
        perror("linkhard");
        fflush(stdout);
        zadnji_status = status;
        return status;
    }
    zadnji_status = 0;
    return 0;
}
int linksoft_command(char *path1, char *path2){
    //ustvari simbolično povezavo
    if (symlink(path1, path2) == -1) {
        int status = errno;
        perror("linksoft");
        fflush(stdout);
        zadnji_status = status;
        return status;
    }
    zadnji_status = 0;
    return 0;
}
int linkread_command(char *path){
    //izpiše vsebino simbolične povezave
    char buf[1024];
    int len = readlink(path, buf, sizeof(buf));
    if (len == -1) {
        int status = errno;
        perror("linkread");
        fflush(stdout);
        zadnji_status = status;
        return status;
    }
    write(output_descritor, buf, len);
    write(output_descritor, "\n", 1);
    zadnji_status = 0;
    return 0;
}

int linklist_command(char *path){
    //Izpiši vse trde povezave na datoteko
    struct stat st;
    if (lstat(path, &st) == -1) {
        int status = errno;
        perror("linklist");
        fflush(stdout);
        zadnji_status = status;
        return status;
    }
    char *name = basename(path);
    DIR *dir = opendir(".");
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        struct stat st2;
        if (lstat(entry->d_name, &st2) == -1) {
            int status = errno;
            perror("linklist");
            fflush(stdout);
            zadnji_status = status;
            return status;
        }
        if (st.st_ino == st2.st_ino && strcmp(name, entry->d_name) != 0) {
            write(output_descritor, entry->d_name, strlen(entry->d_name));
            write(output_descritor, "  ", 2);
        }
    }
    write(output_descritor, name, strlen(name));
    write(output_descritor, "\n", 1);
    closedir(dir);
    zadnji_status = 0;
    return 0;
}

int unlink_command(char *path){
    //odstrani povezavo
    if (unlink(path) == -1) {
        int status = errno;
        perror("unlink");
        fflush(stdout);
        zadnji_status = status;
        return status;
    }
    zadnji_status = 0;
    return 0;
}

int rename_command(char *path1, char *path2){
    //preimenuje datoteko
    if (rename(path1, path2) == -1) {
        int status = errno;
        perror("rename");
        fflush(stdout);
        zadnji_status = status;
        return status;
    }
    zadnji_status = 0;
    return 0;
}

int remove_command(char *path){
    //odstrani datoteko
    if (remove(path) == -1) {
        int status = errno;
        perror("remove");
        fflush(stdout);
        zadnji_status = status;
        return status;
    }
    zadnji_status = 0;
    return 0;
}

int cpcat_command(char argc, char *argv[]){
    //kopira vsebino datoteke
    char buff;
    int fd1, fd2, n;
    if(argc < 2 || argc > 3){
        //kopiraj stdin v stdout
        fflush(stdout);
        while((n = read(input_descritor, &buff, 1)) > 0){
            write(output_descritor, &buff, 1);
        }
    }
    else if(argc == 2){
        //kopiraj datoteko v stdout
        fd1 = open(argv[1], O_RDONLY);
        if(fd1 == -1){
            int status = errno;
            perror("cpcat");
            fflush(stdout);
            zadnji_status = status;
            return status;
        }
        while((n = read(fd1, &buff, 1)) > 0){
            write(output_descritor, &buff, 1);
        }
        close(fd1);
    }
    else if (strcmp(argv[1], "-") == 0){
        //kopiraj stdin v datoteko
        fd2 = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if(fd2 == -1){
            int status = errno;
            perror("cpcat");
            fflush(stdout);
            zadnji_status = status;
            return status;
        }
        while((n = read(input_descritor, &buff, 1)) > 0){
            write(fd2, &buff, 1);
        }
        close(fd2);
    }
    else{
        //kopiraj datoteko v datoteko
        fd1 = open(argv[1], O_RDONLY);
        if(fd1 == -1){
            int status = errno;
            perror("cpcat");
            fflush(stdout);
            zadnji_status = status;
            return status;
        }
        fd2 = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if(fd2 == -1){
            int status = errno;
            perror("cpcat");
            fflush(stdout);
            zadnji_status = status;
            return status;
        }
        while((n = read(fd1, &buff, 1)) > 0){
            write(fd2, &buff, 1);
        }
        close(fd1);
        close(fd2);
    }
    zadnji_status = 0;
    return 0;
}

//PODATKI O SISTEMU

int sysinfo_command(){
    //izpiše informacije o sistemu
    struct utsname buf;
    if (uname(&buf) == -1) {
        int status = errno;
        perror("sysinfo");
        fflush(stdout);
        zadnji_status = status;
        return status;
    }
    write(output_descritor, "Sysname: ", 9);
    write(output_descritor, buf.sysname, strlen(buf.sysname));
    write(output_descritor, "\n", 1);
    write(output_descritor, "Nodename: ", 10);
    write(output_descritor, buf.nodename, strlen(buf.nodename));
    write(output_descritor, "\n", 1);
    write(output_descritor, "Release: ", 9);
    write(output_descritor, buf.release, strlen(buf.release));
    write(output_descritor, "\n", 1);
    write(output_descritor, "Version: ", 9);
    write(output_descritor, buf.version, strlen(buf.version));
    write(output_descritor, "\n", 1);
    write(output_descritor, "Machine: ", 9);
    write(output_descritor, buf.machine, strlen(buf.machine)); 
    write(output_descritor, "\n", 1);
    zadnji_status = 0;
    return 0;
}

int shellinfo_command(){
    //izpiše informacije o lupini
    write(output_descritor, "Uid: ", 5);
    char uid[10];
    sprintf(uid, "%d", getuid());
    write(output_descritor, uid, strlen(uid));
    write(output_descritor, "\n", 1);
    write(output_descritor, "EUid: ", 6);
    char euid[10];
    sprintf(euid, "%d", geteuid());
    write(output_descritor, euid, strlen(euid));
    write(output_descritor, "\n", 1);
    write(output_descritor, "Gid: ", 5);
    char gid[10];
    sprintf(gid, "%d", getgid());
    write(output_descritor, gid, strlen(gid));
    write(output_descritor, "\n", 1);
    write(output_descritor, "EGid: ", 6);
    char egid[10];
    sprintf(egid, "%d", getegid());
    write(output_descritor, egid, strlen(egid));
    write(output_descritor, "\n", 1);
    zadnji_status = 0;
    return 0;
}

//DELO Z PROCESI

int proc_command(char *pot){
    //Nastavitev poti do podatkov o procesih
    
    if (pot == NULL) {
        write(output_descritor, proc_path, strlen(proc_path));
        write(output_descritor, "\n", 1);
    }
    else {
        if(access(pot, F_OK|R_OK) == -1){
            int status = 1;
            fflush(stdout);
            zadnji_status = status;
            return status;
        }
        strcpy(proc_path, pot);
    }
    zadnji_status = 0;
    return 0;
}

int pids_command(){
    //Izpiši trenutne procese iz procs urejene po PID
    DIR *dir;
    struct dirent *ent;
    struct stat st;
    int pids[1024];
    int i = 0;
    if ((dir = opendir (proc_path)) != NULL) {
        while ((ent = readdir (dir)) != NULL) {
            if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                continue;
            char filepath[2048];
            sprintf(filepath, "%s/%s", proc_path, ent->d_name);
            if (stat(filepath, &st) == 0 && S_ISDIR(st.st_mode)) {
                pids[i] = atoi(ent->d_name);
                i++;
            }
        }
        closedir (dir);
    } else {
        int status = errno;
        perror("pids");
        fflush(stdout);
        zadnji_status = status;
        return status;
    }
    qsort(pids, i, sizeof(int), compare);
    for(int j = 0; j < i; j++){
        char pid[10];
        sprintf(pid, "%d", pids[j]);
        write(output_descritor, pid, strlen(pid));
        write(output_descritor, "\n", 1);
    }
    zadnji_status = 0;
    return 0;

}

int pinfo_command(){
    //Izpis informacij o trenutnih procesih
    printf("%5s %5s %6s %s\n", "PID", "PPID", "STANJE", "IME");
    DIR *dir = opendir(proc_path);
    struct dirent *ent;
    struct stat st;
    ProcessInfo pids[1024];
    int i = 0;
    if ((dir = opendir (proc_path)) != NULL) {
        while ((ent = readdir (dir)) != NULL) {
            if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0 || !isdigit(ent->d_name[0])){
                continue;
            }
            char filepath[2048];
            sprintf(filepath, "%s/%s", proc_path, ent->d_name);
            if (stat(filepath, &st) == 0 && S_ISDIR(st.st_mode)) {
                strcpy(pids[i].pid, ent->d_name);
                char status[2048];
                sprintf(status, "%s/%s", filepath, "stat");
                int fd = open(status, O_RDONLY);
                char buf[2048];
                read(fd, buf, 2048);
                char *token = strtok(buf, " ");
                token = strtok(NULL, " ");
                token++;
                strcpy(pids[i].name, token);
                token = strtok(NULL, " ");
                strcpy(pids[i].state, token);
                token = strtok(NULL, " ");
                strcpy(pids[i].ppid, token);
            }
            i++;
        }
        closedir (dir);
    }
    else {
        int status = errno;
        perror("pids");
        fflush(stdout);
        zadnji_status = status;
        return status;
    }
    qsort(pids, i, sizeof(ProcessInfo), compare_process);
    while(i > 0){
        i--;
        pids[i].name[strlen(pids[i].name) - 1] = '\0';
        printf("%5s %5s %6s %s\n", pids[i].pid, pids[i].ppid, pids[i].state, pids[i].name);
    }
}

int waitone_command(char *pid){
    //Počakaj na zaključek procesa
    int status;
    pid_t pidt = atoi(pid);
    waitpid(pidt, &status, 0);
    if(WIFEXITED(status)){
        return WEXITSTATUS(status);
    }
    else if(WIFSIGNALED(status)){
        return WTERMSIG(status);
    }
    else if(WIFSTOPPED(status)){
        return WSTOPSIG(status);
    }
    else if(WIFCONTINUED(status)){
        return 0;
    }
    else{
        return 0;
    }
}

int waitall_command(){
    //Počakaj na zaključek vseh procesov
    int status;
    while(wait(&status) > 0);
    if(WIFEXITED(status)){
        return WEXITSTATUS(status);
    }
    else if(WIFSIGNALED(status)){
        return WTERMSIG(status);
    }
    else if(WIFSTOPPED(status)){
        return WSTOPSIG(status);
    }
    else if(WIFCONTINUED(status)){
        return 0;
    }
    else{
        return 0;
    }
}

//CEVOVODI

int pipes_command(){
    //Izvedi ukaze z uporabo cevi
    int fd[2];
    
}

int main()
{
    getcwd(cwd, sizeof(cwd));
    char input[MAX_LINE];

    //register signal handler
    signal(SIGCHLD, sigchld_handler);

    

    while (1)
    {
        //Obnovi deskriptorje
        if(output_descritor != 1){
            close(output_descritor);
            output_descritor = 1;
        }
        if(input_descritor != 0){
            close(input_descritor);
            input_descritor = 0;
        }

        if(isatty(0)){
        printf("%s>", ime_lupine);
        }
        fgets(input, sizeof(input), stdin); // Preberi vhodno vrstico
        fflush(stdin);
        for (int i = 0; i < MAX_ARGS; i++) {arguments[i] = NULL;}
        //preveri ali je dosežen EOF
        
        if (feof(stdin)) {
            exit_command(0);
        }
        else if (ferror(stdin)) {
            int status = errno;
            perror("Napaka pri branju iz stdin");
            exit_command(status);
        }
        //check if all chars in input are whitespaces and if so then continue
        for (int k = 0; k < sizeof(input); k++) {
            if (!isspace(input[k])) {
                break;
            }
            if (k == sizeof(input) - 1) {
                continue;
            }
        }
        if (isspace(input[0]) || input[0] == '\n' || input[0] == '#')
        {
            continue;
        }
        // Preberi argumente
        int i = read_args(input, 0);
        arguments[i] = NULL;

        int input_re = 0;  // 0 - ni preusmeritve, 1 - preusmeritev vhoda
        int output_re = 0; // 0 - ni preusmeritve, 1 - preusmeritev izhoda
        int background_exe = 0; // 0 - ni izvajanja v ozadju, 1 - izvajanje v ozadju
        
        //print all args
        //for (int j = 0; j < i; j++){printf("%s\n", arguments[j]);}
        if (i > 0)
        {
            // Preveri, ali je zadnji argument '&'
            if (strcmp(arguments[i - 1], "&") == 0)
            {
                background_exe = 1;
                arguments[i - 1] = NULL;
                i--;

            }
            // Preveri, ali je zadnji argument '>'
            if (strncmp(arguments[i - 1], ">", 1) == 0)
            {
                output_re = 1;
                arguments[i-1]++;
                output_descritor = open(arguments[i-1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (output_descritor == -1)
                {
                    perror("rerouting");
                    output_descritor = STDOUT_FILENO;
                    continue;
                }
                arguments[i - 1] = NULL;
                i--;
                
            }
            // Preveri, ali je zadnji argument '<'
            if (strncmp(arguments[i - 1], "<", 1) == 0)
            {
                input_re = 1;
                arguments[i-1]++;
                input_descritor = open(arguments[i-1], O_RDONLY);
                if(input_descritor == -1){
                    perror("rerouting");
                    input_descritor = STDIN_FILENO;
                } 
                arguments[i - 1] = NULL;
                i--;
                //struct stat file_stat;
                //off_t input_size = file_stat.st_size;
                //char f_input[input_size + 1];
                //ssize_t bytes_read = read(input_descritor, f_input, sizeof(input));
                //f_input[bytes_read] = '\0';
                //i = read_args(f_input, 1);
                //for (int j = 0; j < i; j++){printf("%s ", arguments[j]);}
            }

            
        }
        eval(i, arguments, input_re, output_re, background_exe);
    }

    return 0;
}

int eval(int i, char* arguments[MAX_ARGS], int input_re, int output_re, int background_exe){
    pid_t pid;
    fflush(stdout);
    if(strcmp(arguments[0], "help") == 0){
            if(background_exe == 0)
                help_command();
            else{
                pid = fork();
                if(pid == 0){
                    help_command();
                    exit_command(0);
                }
            }
        }
        else if(strcmp(arguments[0], "status") == 0){
            if(background_exe == 0)
                status_command();
            else{
                pid = fork();
                if(pid == 0){
                    status_command();
                    exit_command(0);
                }
            }
        }
        else if(strcmp(arguments[0], "exit") == 0){
            if(background_exe == 0){
                if (i > 1) {
                    exit_command(atoi(arguments[1]));
                }
                exit_command(0);
            }
            else{
                pid = fork();
                if(pid == 0){
                    if (i > 1) {
                        exit_command(atoi(arguments[1]));
                    }
                    exit_command(0);
                }
            }
        }
        else if(strcmp(arguments[0], "name") == 0){
            if(background_exe == 0){
                if (i > 1)
                {
                    name_command(arguments[1]);
                }
                else {
                    name_command(ime_lupine);
                }
            }
            else{
                pid = fork();
                if(pid == 0){
                    if (i > 1)
                    {
                        name_command(arguments[1]);
                    }
                    else {
                        name_command(ime_lupine);
                    }
                    exit_command(0);
                }
            }
            
        }
        else if(strcmp(arguments[0], "print") == 0){
            if(background_exe == 0)
                print(i, arguments);
            else{
                pid = fork();
                if(pid == 0){
                    print(i, arguments);
                    exit_command(0);
                }
            }
        }
        else if(strcmp(arguments[0], "echo") == 0){
            if(background_exe == 0){
                echo(i, arguments);
            }
            else{
                pid = fork();
                if(pid == 0){
                    echo(i, arguments);
                    exit_command(0);
                }
            }
        }
        else if(strcmp(arguments[0], "pid") == 0){
            if (background_exe == 0){
                pid_command();
            }
            else{
                pid = fork();
                if(pid == 0){
                    pid_command();
                    exit_command(0);
                }
            }
            
        }
        else if(strcmp(arguments[0], "ppid") == 0){
            if (background_exe == 0){
                ppid_command();
            }
            else{
                pid = fork();
                if(pid == 0){
                    ppid_command();
                    exit_command(0);
                }
            }
        }
        else if (strcmp(arguments[0], "dirchange") == 0){
            if(background_exe == 0){
                dirchange_command(arguments[1]);
            }
            else{
                pid = fork();
                if(pid == 0){
                    dirchange_command(arguments[1]);
                    exit_command(0);
                }
            }
        }
        else if(strcmp(arguments[0], "dirwhere") == 0){
            if(background_exe == 0){
                dirwhere_command();
            }
            else{
                pid = fork();
                if(pid == 0){
                    dirwhere_command();
                    exit_command(0);
                }
            }
        }
        else if(strcmp(arguments[0], "dirbase") == 0){
            if (background_exe){
                pid = fork();
                if(pid == 0){
                    dirbase_command();
                    exit_command(0);
                }
            }
            else{
                dirbase_command();
            }
        }
        else if(strcmp(arguments[0], "dirmake") == 0){
            if(background_exe == 0){
                dirmake_command(arguments[1]);
            }
            else{
                pid = fork();
                if(pid == 0){
                    dirmake_command(arguments[1]);
                    exit_command(0);
                }
            }
        }
        else if (strcmp(arguments[0], "dirremove") == 0){
            if(background_exe){
                pid = fork();
                if(pid == 0){
                    dirremove_command(arguments[1]);
                    exit_command(0);
                }
            }
            else{
                dirremove_command(arguments[1]);
            }
        }
        else if(strcmp(arguments[0], "dirlist") == 0){
            if (background_exe){
                pid = fork();
                if(pid == 0){
                    dirlist_command(arguments[1]);
                    exit_command(0);
                }
            }
            else{
                dirlist_command(arguments[1]);
            }
        }
        else if(strcmp(arguments[0], "linkhard") == 0){
            if(background_exe){
                pid = fork();
                if(pid == 0){
                    linkhard_command(arguments[1], arguments[2]);
                    exit_command(0);
                }
            }
            else{
                linkhard_command(arguments[1], arguments[2]);
            }
        }
        else if(strcmp(arguments[0], "linksoft") == 0){
            if (background_exe){
                pid = fork();
                if(pid == 0){
                    linksoft_command(arguments[1], arguments[2]);
                    exit_command(0);
                }
            }
            else{
                linksoft_command(arguments[1], arguments[2]);
            }
        }
        else if(strcmp(arguments[0], "linkread") == 0){
            if(background_exe){
                pid = fork();
                if(pid == 0){
                    linkread_command(arguments[1]);
                    exit_command(0);
                }
            }
            else{
                linkread_command(arguments[1]);
            }
        }
        else if(strcmp(arguments[0], "linklist") == 0){
            if(background_exe){
                pid = fork();
                if(pid == 0){
                    linklist_command(arguments[1]);
                    exit_command(0);
                }
            }
            else{
                linklist_command(arguments[1]);
            }
        }
        else if(strcmp(arguments[0], "unlink") == 0){
            if(background_exe){
                pid = fork();
                if(pid == 0){
                    unlink_command(arguments[1]);
                    exit_command(0);
                }
            }
            else{
                unlink_command(arguments[1]);
            }
        }
        else if(strcmp(arguments[0], "rename") == 0){
            if(background_exe){
                pid = fork();
                if(pid == 0){
                    rename_command(arguments[1], arguments[2]);
                    exit_command(0);
                }
            }
            else{
                rename_command(arguments[1], arguments[2]);
            }
        }
        else if(strcmp(arguments[0], "remove") == 0){
            if(background_exe){
                pid = fork();
                if(pid == 0){
                    remove_command(arguments[1]);
                    exit_command(0);
                }
            }
            else{
                remove_command(arguments[1]);
            }
        }
        else if(strcmp(arguments[0], "cpcat") == 0){
            if(background_exe){
                pid = fork();
                if(pid == 0){
                    cpcat_command(i, arguments);
                    exit_command(0);
                }
            }
            else{
                cpcat_command(i, arguments);
            }
        }
        else if(strcmp(arguments[0], "sysinfo") == 0){
            if(background_exe){
                pid = fork();
                if(pid == 0){
                    sysinfo_command();
                    exit_command(0);
                }
            }
            else{
                sysinfo_command();
            }
        }
        else if(strcmp(arguments[0], "shellinfo")==0){
            if(background_exe){
                pid = fork();
                if(pid == 0){
                    shellinfo_command();
                    exit_command(0);
                }
            }
            else{
                shellinfo_command();
            }
        }
        else if(strcmp(arguments[0], "proc")==0){
            if(background_exe){
                pid = fork();
                if(pid == 0){
                    proc_command(arguments[1]);
                    exit_command(0);
                }
            }
            else{
                proc_command(arguments[1]);
            }
        }
        else if(strcmp(arguments[0], "pids")==0){
            if(background_exe){
                pid = fork();
                if(pid == 0){
                    pids_command();
                    exit_command(0);
                }
            }
            else{
                pids_command();
            }
        }
        else if(strcmp(arguments[0], "pinfo")==0){
            if(background_exe){
                pid = fork();
                if(pid == 0){
                    pinfo_command(arguments[1]);
                    exit_command(0);
                }
            }
            else{
                pinfo_command(arguments[1]);
            }
        }
        else if(strcmp(arguments[0], "waitone")==0){
            if(background_exe){
                pid = fork();
                if(pid == 0){
                    waitone_command(arguments[1]);
                    exit_command(0);
                }
            }
            else{
                waitone_command(arguments[1]);
            }
        }
        else if(strcmp(arguments[0], "waitall")==0){
            if(background_exe){
                pid = fork();
                if(pid == 0){
                    waitall_command();
                    exit_command(0);
                }
            }
            else{
                waitall_command();
            }
        }
        else if(strcmp(arguments[0], "pipes") == 0){
            if(background_exe){
                pid = fork();
                if(pid == 0){
                    pipes_command(arguments);
                    exit_command(0);
                }
            }
            else{
                pipes_command(arguments);
            }
        }
        else {
            //printf("Izvajam ukaz: %s\n", arguments[0]);
            execute_command(i, input_re, output_re, background_exe);
        }
        return 0;
}


