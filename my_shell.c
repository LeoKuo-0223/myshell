#ifndef _POSIX_C_SOURCE
	#define  _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include<stdbool.h>
#include <fcntl.h>
#include <sys/stat.h>
#define BUFFSIZE 1024
#define TOKN_BUFFSIZE 64
#define CMD_BUFFSIZE 16
#define ECHOBUFFSIZE 64

typedef struct Queue{
    char **queue;
    int capacity;
    int front;
    int rear;
    int num_cmd;
}Queue;

///////////////////////////////
// Queue functions declaration
Queue* Queue_new(int capacity);
void Queue_Push(Queue *q, char* x);
void Queue_Pop(Queue *q);
bool Queue_IsEmpty(Queue *q);
////////////////////////////////


//shell functions and variables declaration
void command_loop(void);
char* read_line();
int split_line(char*line, char **args);
int shell_launch(char **args, bool lastcmd);
int shell_execute(char **args, int nargs);
int launch_builtinFunct(char **args, int cmd_idx, bool lastcmd);
void redirectIn(char *fileName);
int redirectOut(char *fileName, char**args);
void createPipe(char *args[]);
void recoverstdInOut();
static Queue *cmdBuffer;

bool bgRunning = false;

//////////////////////////////

//built in function declarations
void my_cd(char **args);
void my_help(char **args);
void my_exit(char **args);
void my_echo(char **args);
void my_record(char **args);
void my_pid(char **args);
/////////////////////////////////


Queue* Queue_new(int capacity){ //constructor for queue
    Queue *q=(Queue*)malloc(sizeof(Queue));
    q->queue = (char**)malloc(sizeof(char*)*(capacity+1));  //+1, because queue structure
    if(q==NULL){
        printf("Unable to allocate queue bufffer");
        exit(EXIT_FAILURE);
    }
    for(int i=0;i<capacity;i++) q->queue[i]=NULL;
    q->front = q->rear = 0;
    q->capacity = capacity;
    q->num_cmd=0;
    return q;
}

bool Queue_IsEmpty(Queue *q){       //check the queue is empty or not
    return q->front==q->rear;
}

void Queue_Push(Queue *q, char* x){      //add new element to queue
    if((q->rear+1)%(q->capacity+1)==q->front){ //if queue is full
        Queue_Pop(q);
    }
    q->rear = (q->rear+1)%(q->capacity+1);
    q->queue[q->rear] = x;
}

void Queue_Pop(Queue *q){       //delete element from queue
    if(Queue_IsEmpty(q)){   //if queue is empty
        printf("Something wrong");
    }
    free(q->queue[q->front]); //free the mem of the first element in Queue
    q->front = (q->front+1)%(q->capacity+1);
}


//shell main function
int main(){
    printf("==================================================\n");
    printf("*   Welcome to my shell                          *\n");
    printf("*   Type help to see builtin function            *\n");
    printf("*                                                *\n");
    printf("*   redirection '>' or '<'                       *\n");
    printf("*   pipe: '|'                                    *\n");
    printf("*   background '&'                               *\n");
    printf("*                                                *\n");
    printf("*   Have Fun!!                                   *\n");
    printf("==================================================\n");
    command_loop();
	//printf("hello\n");
	return 0;
}

void command_loop(void){
    char *line=(char*)malloc(sizeof(char) * BUFFSIZE);
    char **args=(char**)malloc(sizeof(char*)*TOKN_BUFFSIZE);
    if(args==NULL || line==NULL){
        printf("Unable to allocate bufffer");
        exit(EXIT_FAILURE);
    }
    int status;
    int nargs; //number of arguments
    cmdBuffer = Queue_new(CMD_BUFFSIZE);    //new queue
    do{
        printf(">>> $ ");
        read_line(line);
        nargs = split_line(line, args);
        status = shell_execute(args, nargs);
        bgRunning = false;
        recoverstdInOut();
    }while(status);
    free(cmdBuffer);
    free(line);
    free(args);
}


//read the command into char array
char* read_line(char *buffer){
    int buffer_size=BUFFSIZE;
    int pos = 0;
    int ch;
    while(1){
        ch = getchar();
        if(ch==EOF || ch=='\n'){
            buffer[pos] = '\0'; //set the last element to \0, stand for ending
            break;
        }else{
            buffer[pos] = ch;
        }
        pos++;
        if(pos>= buffer_size){
            buffer_size+=BUFFSIZE;
            buffer = realloc(buffer, buffer_size);
                if(buffer==NULL){
                    printf("Unable to allocate bufffer");
                    exit(EXIT_FAILURE);
                }
        }
    }
    //printf("finish read line\n");
}


//split the command string into tokens
int split_line(char*line, char **args){
    if(strlen(line)==0) return -1;
    int pos = 0;
    int buffer_size = TOKN_BUFFSIZE;
    char *line_copy = (char*)malloc(sizeof(char)*128); //use to save in record queue
    strcpy(line_copy,line);
    char *delimiters = " \t\r\n\a";
    char *saveptr = NULL;
    char* token = strtok_r(line, delimiters, &saveptr);
    int tmp_pos;
    bool replay=false;

    while (token != NULL) {
        if(!strcmp(token, "replay")){ //replace the cmd with old cmds in the buffer
            replay=true;
            token = strtok_r(NULL, delimiters, &saveptr);
            if(token==NULL){ //replay has no idx
                printf("replay: wrong args\n");
                return -1;
            }
            int cmd_idx = atoi(token);
            if (cmd_idx<1 || cmd_idx>16){
                printf("replay: wrong args\n");
                return -1;
            }else{ //legal cmd
                int start = (cmdBuffer->front+1)%cmdBuffer->capacity;
                //in case, replay cmd idx smaller than the item number in queue
                if((cmd_idx-1+start)%(cmdBuffer->capacity+1) > cmdBuffer->num_cmd){
                    printf("replay: args error\n");
                    return -1;
                }
                char *old_cmd = cmdBuffer->queue[(cmd_idx-1+start)%(cmdBuffer->capacity+1)];
                char *tmp = (char*)malloc(sizeof(char)*128);
                strcpy(tmp, old_cmd);
                token = strtok(tmp, " \t");
                tmp_pos = pos;
                while(token!=NULL){
                    args[tmp_pos]=token;
                    tmp_pos++;
                    token = strtok(NULL, " \t\n");	
                }
                token = strtok_r(NULL, delimiters, &saveptr);
                pos = tmp_pos;
                continue;
            }
        }else{
            args[pos]=token;
            pos++;
            if(pos>=buffer_size){
                buffer_size+=TOKN_BUFFSIZE; //double the buffer size 
                args = realloc(args, buffer_size);
            }
            token = strtok_r(NULL, delimiters, &saveptr);
        }

    }
    args[pos] = NULL;  //the last one is NULL element
    if(!strcmp(args[pos-1], "&")) bgRunning=true;
    if(replay){
        char *cmd_resemble=malloc(sizeof(char)*128);
        memset(cmd_resemble, 0, sizeof(cmd_resemble));
        for(int i=0;i<pos;i++){ //rebuild the whole command into string
            strcat(cmd_resemble, args[i]);
            strcat(cmd_resemble, " ");
        }
        Queue_Push(cmdBuffer, cmd_resemble);
        if(cmdBuffer->num_cmd < 17)cmdBuffer->num_cmd=(cmdBuffer->num_cmd+1);
        free(line_copy);
    }else{
        if(args[0]!=NULL){
            Queue_Push(cmdBuffer, line_copy);
            if(cmdBuffer->num_cmd < 17)cmdBuffer->num_cmd=(cmdBuffer->num_cmd+1);
        }
    }
    return pos;
}


//launch the function except built-in functions
int shell_launch(char **args, bool lastcmd){ 
    int status;
    pid_t pid, wpid;
    pid = fork();

    if (pid == 0){ //successfully create child process
        if(lastcmd && bgRunning) printf("[Pid]: %d\n",getpid());
        if (execvp(args[0], args) == -1){
            printf("Invalid Command\n");
        }
        //if code run to the part below this line, mean execvp fail
        exit(EXIT_FAILURE);
    } else if (pid < 0){ //unsuccessful
        perror("Error");
    } else {    //positive value, stand for returning the parent(caller)

        if(bgRunning==true){
            bgRunning=true;
        }else{
            waitpid(pid, NULL, 0);
        }
    }
    //recover stdin, stdout
    recoverstdInOut(); //重新使輸出輸入回到終端
    
    return 1;
}

////////////////////////////////
/*implement built in funciton*/
//////////////////////////////


//built-in function list
char* builtin_str[] = {
    "cd",
    "help",
    "exit",
    "echo",
    "record",
    "mypid",
    "replay",
};

char* help_info[] = {
    ": change directory",
    ": show all the built-in commands info",
    ": exit the shell directly",
    ": echo the strings to standard output",
    ": show the last 16 commands you used",
    ": show the pid with arguments -i, -p,-c for itself, parent, child",
    ": re-execute the commands in records",
};

//built-in function pointer array
void (*builtin_func[]) (char **args) = {
    &my_cd,
    &my_help,
    &my_exit,
    &my_echo,
    &my_record,
    &my_pid,
};

int myshell_num_builtins() {
    return sizeof(builtin_str) / sizeof(char *);
}


//launch built-in Function
int launch_builtinFunct(char **args, int cmd_idx, bool lastcmd){
    int status;
    pid_t pid, wpid;

    if (builtin_str[cmd_idx]=="exit"){ //if cmd is exit
    
        (*builtin_func[cmd_idx])(args);
        //recover stdin, stdout
        recoverstdInOut();
        printf("Exit the shell\n");
        return 0;
    }else if(bgRunning==false){ //沒有背景執行的情況下，built-in command 會是由跑 shell 的這個原本的 process 來執行
        
        (*builtin_func[cmd_idx])(args);

    }else{
        pid = fork();
        if (pid == 0){ //successfully create child process
            if(lastcmd && bgRunning) printf("[Pid]: %d\n",getpid());
            (*builtin_func[cmd_idx])(args);
            recoverstdInOut();
            // printf("finish child process %s\n", builtin_str[cmd_idx]);
            exit(EXIT_SUCCESS); //terminate child process

        } else if (pid < 0){ //unsuccessful
            perror("Error");

        } else {    //positive value, stand for returning the parent(caller)
            if(bgRunning==true){
                bgRunning=true;
            }else{
                waitpid(pid, NULL, 0);
            }
        }
    }
    //recover stdin, stdout
    recoverstdInOut();//重新使輸出輸入回到終端


    return 1;
}



//built-in functions implementation
void my_cd(char **args){
    if (args[1] == NULL) {
        fprintf(stderr, "expected argument to \"cd\"\n");
    }else {
        if (chdir(args[1]) != 0){
            perror("Error");
        }
    }
}

void my_help(char **args){
    int i;
    printf("Type program names and arguments, and hit enter.\n");
    printf("The following are built in:\n");

    for (i = 0; i < myshell_num_builtins(); i++) {
        printf("%d:  %s %s\n",i+1, builtin_str[i], help_info[i]);
    }

    printf("Use the man command for information on other programs.\n");
}

void my_exit(char **args){
    printf("See You Next Time\n");
}


void my_echo(char **args){
    int i=1;
    bool flagset=false;
    char *para = "-n";
    // sleep(2); //check running in background works
    if(args==NULL || args[1]==NULL){
        recoverstdInOut();
        printf("echo: Error arguments\n");
        return ;
    }
    if(!strcmp(args[i], para)){
        flagset=true;
    }

    if(flagset){    //-n command is detected
        i+=1;
        while(args[i]!=NULL){
            if(i!=2) printf(" ");
            printf("%s", args[i]);
            i++;
        }
    }else{  //-n command isn't detected
        while(args[i]!=NULL){
            if(i!=1) printf(" ");
            printf("%s", args[i]);
            i++;
        }
        printf("\n");
    }

}

void my_record(char **args){
    int start = (cmdBuffer->front+1)%cmdBuffer->capacity;
    int end = cmdBuffer->rear;
    // printf("start: %d, end: %d\n", start, end);
    int cnt=1;
    for(int i=0;i<=15;i++){
        printf("%2d: %s\n",cnt,cmdBuffer->queue[(i+start)%(cmdBuffer->capacity+1)]);
        cnt++;
    }
}


void split_PPID(char readbuf[], char **output){
    char *token;
    int pos=0;
    token = strtok(readbuf, " \t");
    while(token!=NULL){
        output[pos] = token;
        pos++;
        token = strtok(NULL, " \t");
    }
    output[pos] = NULL;
}

void my_pid(char **args){
    pid_t pid = getpid();
    if(args[1]==NULL){
        printf("mypid: wrong args\n");
        return;
    }
    if(strcmp(args[1],"-i")==0){
        printf("%d\n", pid);
        return;
    }
    if(args[2]==NULL){ 
        printf("arguments error\n");
        return ;
    }
    char Loc[32];
    char *output[16];
    memset(Loc, 0, sizeof(Loc));
    strcat(Loc, "/proc/");
    strcat(Loc, args[2]);
    if(strcmp(args[1],"-p")==0){
        strcat(Loc, "/status");
        // printf("Loc: %s\n", Loc);
        FILE *f = fopen(Loc, "r");
        if (f != NULL) {
            char readbuf[32];
            for(int i=0;i<7;i++){   //PPID is located at line 5
                fgets(readbuf, 32, f);
            }
            // printf("%s", readbuf);
            split_PPID(readbuf, output);
            int i=1;
            while(output[i]!=NULL){
                printf("%d  ",atoi(output[i]));
                i++;
            }
            printf("\n");
            fclose(f);
        } else printf("Process id do not exit\n");

    }else if(strcmp(args[1],"-c")==0){
        strcat(Loc, "/task/");
        strcat(Loc, args[2]);
        strcat(Loc, "/children");
        // printf("%s\n", Loc);
        FILE *f = fopen(Loc, "r");
        if (f != NULL) {
            char readbuf[32];
            int i=1;
            while(fgets(readbuf, 32, f)!=NULL){
                printf("%s\n",readbuf);
                i++;
            }
            fclose(f);
        } else printf("Process id do not exit\n");

    }else{
        printf("Invalid arguments\n");
    }

}


void recoverstdInOut(){
    int out = open("/dev/tty", O_WRONLY | O_TRUNC | O_CREAT, 0600);
    dup2(out, 1);
    close(out);

    int in = open("/dev/tty", O_RDONLY);
    dup2(in, 0); //replace stdin to fd
    close(in);
}


void redirectIn(char *fileName){
    int in = open(fileName, O_RDONLY);
    dup2(in, 0); //replace stdin to fd
    close(in);
}


int redirectOut(char *fileName, char **args){
    int out = open(fileName, O_WRONLY | O_TRUNC | O_CREAT, 0600);
    pid_t pid = fork();
    if(pid==0){
        if(bgRunning) printf("[Pid]: %d\n",getpid());
        dup2(out, 1);
        close(out);
        for (int i = 0; i < myshell_num_builtins(); i++) {
            if (strcmp(args[0], builtin_str[i]) == 0) {
                (*builtin_func[i])(args);
                recoverstdInOut();
                exit(EXIT_SUCCESS); //terminate child process
            }
            if(i==myshell_num_builtins()-1){ //if not built-in function
                if (execvp(args[0], args) == -1){
                    printf("Invalid Command\n");
                }
                //if code run to the part below this line, mean execvp fail
                exit(EXIT_FAILURE);
            }
        }
        
    }else if(pid<0){
        perror("Error");
    }else{
        if(bgRunning==false){
            waitpid(pid, NULL, 0);
        }
    }
    return 1;
}


void createPipe(char *args[]){
    int fd[2];
    pipe(fd);   //create pipe

    dup2(fd[1], 1); //replace the stdout to fd write
    close(fd[1]);   //close original fd

    //printf("args = %s\n", *args);

//run builtin function or non-builtin function
    for (int i = 0; i < myshell_num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            launch_builtinFunct(args, i, false);
            break;
        }
        if(i==myshell_num_builtins()-1){
            shell_launch(args, false);
            break;
        }
    }
    dup2(fd[0], 0); //replace the stdin to fd read
    close(fd[0]);
}

int shell_execute(char **args, int nargs){
    if(args[0]==NULL || nargs==-1) return 1; //if command is invalid or NULL, ignore it
    
    int num_of_args = nargs;
    char **tmp_args = malloc(sizeof(char*)*16);
    int return_value;
    if(tmp_args==NULL){
        printf("allocate memory space error\n");
        return 0;
    }

    if(strcmp(args[nargs-1], "&")==0){ //if has '&' flag
        bgRunning=true;
        args[nargs-1] = NULL; //alreadiy record the bgRunning flag so clear & to NULL
        num_of_args--;
        //printf("found & flag\n");
    }

    int i=0, j=0;
    while (i<num_of_args) {
        if (!strcmp(args[i],"<")) {
            redirectIn(args[++i]);
        } else if (!strcmp(args[i],">")) {
            return_value=redirectOut(args[++i], tmp_args);
            free(tmp_args);
            return return_value;
        } else if (!strcmp(args[i], "|")) {
            tmp_args[j] = NULL;
            createPipe(tmp_args);
            j = 0; //reset tmp_args idx
        } else {
            tmp_args[j] = args[i];
            j++;
        }
        i++;
    }
    tmp_args[j] = NULL;
    
    for (int i = 0; i < myshell_num_builtins(); i++) {
        if (strcmp(tmp_args[0], builtin_str[i]) == 0) {
            return_value = launch_builtinFunct(tmp_args, i, true);
            break;
        }
        if(i==myshell_num_builtins()-1){
            return_value = shell_launch(tmp_args, true);
            break;
        }
    }

    
    free(tmp_args);
    return return_value;
}
