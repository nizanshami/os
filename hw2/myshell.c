#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

int process_arglist(int, char **);
int run_pipas(int ,char **, int);
int run_process_redirect(int, char **, char *);
int run_process_background(int, char **);
void reset_sigint_handler();
int run_process(int, char **);
int contains_pipas(int , char **, int *);

// prepare and finalize calls for initialization and destruction of anything required
int prepare(void){
    // shell ignore SIGINT
    struct sigaction ignore_sigint;
    ignore_sigint.sa_sigaction = (void *)SIG_IGN;
    ignore_sigint.sa_flags = SA_RESTART;
    if(sigaction(SIGINT, &ignore_sigint, 0) == -1){
        perror("sigaction has falled");
        exit(1);
    }
    // handle zombies with the help from http://www.microhowto.info/howto/reap_zombie_processes_using_a_sigchld_handler.html
    struct sigaction sa_sigchld;
    sa_sigchld.sa_handler = SIG_IGN;
    sa_sigchld.sa_flags = 0;
    if(sigaction(SIGCHLD, &sa_sigchld, 0) == -1){
        perror("sigaction has falled");   
    }
    
    return 0;
}

// arglist - a list of char* arguments (words) provided by the user
// it contains count+1 items, where the last item (arglist[count]) and *only* the last is NULL
// RETURNS - 1 if should continue, 0 otherwise
int process_arglist(int count, char **arglist){
    int pipe_symbel_ind = -1;
    
    if(strcmp(arglist[count-1], "&") == 0){
        return run_process_background(count -1, arglist);   
    }else if (count > 2 && strcmp(arglist[count-2], ">>") == 0){
        return run_process_redirect(count ,arglist, arglist[count-1]);
    }else if (contains_pipas(count, arglist, &pipe_symbel_ind) >= 0){
        return run_pipas(count, arglist, pipe_symbel_ind);
    }else{
        return run_process(count, arglist);
    }
}

int run_pipas(int count,char **arglist, int pipe_symbel_ind){
    int pipefd[2];
    arglist[pipe_symbel_ind] = NULL;
    char **arglist1 = arglist;
    char **arglist2 = arglist + (sizeof(char) * (pipe_symbel_ind+1));

    if(pipe(pipefd) == -1){
        perror("pipe has falled");
        exit(1);
    }
    pid_t pid1 = fork();
    if(pid1 == -1){
        perror("fork has falled");
        exit(1);
    }else if(pid1 == 0){
        reset_sigint_handler();
        if(dup2(pipefd[1], STDOUT_FILENO) == -1){
            perror("dup2 has falled");
        }
        close(pipefd[1]);
        close(pipefd[0]);
        execvp(arglist1[0], arglist);
        perror("execvp has falled");
        exit(1);
    }else {
        close(pipefd[1]);
        pid_t pid2 = fork();
        if(pid2 == -1){
            perror("fork has falled");
            exit(1);
        }else if(pid2 == 0){
            reset_sigint_handler();
            if(dup2(pipefd[0], STDIN_FILENO) == -1){
                perror("dup2 has falled");  
            }
            close(pipefd[0]);
            execvp(arglist2[0], arglist2);
            perror("execvp has falled");
            exit(1);
        }else{
            close(pipefd[0]);
            waitpid(pid1, NULL, WUNTRACED);
            waitpid(pid2, NULL, WUNTRACED);
        }
    }
    return 1;
}

int run_process_redirect(int count, char **arglist, char *output_filename){
    arglist[count-2]= NULL;
    int f_out;
    pid_t pid = fork();
    if(pid == -1){
        perror("fork has falled");
        exit(1);
    }else if(pid == 0){
        reset_sigint_handler();
        f_out = open(output_filename, O_CREAT | O_APPEND | O_RDWR, 0644); 
        if(dup2(f_out, STDOUT_FILENO) == -1){
            perror("dup2 has falled");
            exit(1);
        }
        execvp(arglist[0], arglist);
        perror("evecvp has falled");
        exit(1);
        close(f_out);
    }else{
        waitpid(pid, NULL, WUNTRACED);
    }
    return 1;
} 

int run_process_background(int count, char **arglist){
    arglist[count] = NULL;
    pid_t pid = fork();
    if(pid == -1){
        perror("fork has falled");
        exit(1);
    }else if (pid == 0){
        reset_sigint_handler();
        execvp(arglist[0], arglist);
        perror("execvp has falled");
        exit(1);
    }
    return 1;
}

int contains_pipas(int count, char **arglist, int *is_pipe){
    int i;
    for(i = 0;i < count;i++){
       if(strcmp(arglist[i], "|") == 0){
           *is_pipe = i;
           break;
       } 
    }
    return *is_pipe;
}

void reset_sigint_handler(){
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sa.sa_flags = SA_RESTART;
    if(sigaction(SIGINT, &sa, 0) == -1){
        perror("sigaction has falled");
    }

}

int run_process(int count, char **arglist){
    pid_t pid = fork();
    if(pid == -1){
        perror("fork has falled");
        exit(1);
    }else if (pid == 0){
        reset_sigint_handler();
        execvp(arglist[0], arglist);
        perror("execvp has falled");
        exit(1);  
    }else{
        waitpid(pid, NULL, WUNTRACED);
    }
    return 1;
}

//return 0 on success
int finalize(void){
    return 0;
}
