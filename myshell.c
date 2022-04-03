#include <stdlib.h>
#include <stdio.h> 
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

void exit_on_error(char *err_str)
{
    fprintf(stderr, "%s", err_str);
    exit(1);
}


void handle_sigint(int sig_num)
{
}

void use_sigaction()
{
     struct sigaction sigterm_action;
    // memset(&sigterm_action, 0, sizeof(sigterm_action));

    sigterm_action.sa_flags = SA_RESTART;
    sigterm_action.sa_handler = &handle_sigint;

    sigfillset(&sigterm_action.sa_mask);    // when handler is running, block all signals

    sigaction(SIGINT, &sigterm_action, NULL);
}


/*
 *  Returns the index of '|' in arglist, or -1 if '|' is not found.
 *  Implementation uses the assumptions written in the guidelines.
 */
int index_of_pipe_symbol(int count, char** arglist)
{
    int i;
    for (i = 0; i < count; i++)
        if (arglist[i][0] == 124)
            return i;
    
    return -1;
}


int process_arglist(int count, char** arglist)
{
    int pipe_fds[2];
    int pid, pid2;
    int pipe_ind;  
    int output_file_fs;
    int status, status2;
    
    pipe_ind = index_of_pipe_symbol(count, arglist);

    // if the cmd contains '|', process the pipe cmd
    if (pipe_ind != -1)
    {
        // remove '|' from arglist (this splits arglist to 2 parts)
        arglist[pipe_ind] = NULL;   

        // create the pipe to be used
        if (pipe(pipe_fds) == -1)
        {
            fprintf(stderr, "%s", "error when creating pipe\n");
            return 0;
        }

        // create the first child
        pid = fork();
        if (pid < 0)
        {
            fprintf(stderr, "%s", "fork failed\n");
            return 0;
        }
        if (pid == 0)
        {
            // make child sensitive to ctrl-c
                        signal(SIGINT, SIG_DFL);
                        
            // this is the first child - make pipe adjustments, then run cmd
            if (close(pipe_fds[0]) == -1)
                exit_on_error("error when closing pipe fs\n");
            if (dup2(pipe_fds[1], 1) == -1)
                exit_on_error("error when using dup2 for piping\n");
            if (execvp(arglist[0], arglist) == -1)
                exit_on_error("error on execvp (maybe invalid cmd)\n");
        }
        if (pid > 0)
        {
            // create the second child
            pid2 = fork();
            if (pid < 0)
            {
                fprintf(stderr, "%s", "fork failed\n");
                return 0;
            }
            if (pid2 == 0)
            {
                // this is the second child - make pipe adjustments, then run cmd
                if (close(pipe_fds[1]) == -1)
                    exit_on_error("error when closing pipe fs\n");
                if (dup2(pipe_fds[0], 0) == -1)
                    exit_on_error("error when using dup2 for piping\n");
                if (execvp(arglist[pipe_ind + 1], arglist + pipe_ind + 1) == -1)
                    exit_on_error("error on execvp (maybe invalid cmd)\n");
            }
            if (pid > 0)
            {
                // close two pipes in the parent process
                if (close(pipe_fds[0]) == -1)
                {
                    fprintf(stderr, "%s", "error when closing pipe fs\n");
                    return 0;
                }
                if (close(pipe_fds[1]) == -1)
                {
                    fprintf(stderr, "%s", "error when closing pipe fs\n");
                    return 0;
                }
                
                // wait for two child processes to end
                if (waitpid(pid, &status, 0) == -1)
                    if (errno != ECHILD && errno != EINTR)
                    {
                        fprintf(stderr, "%s", "error in wait() of parent process\n");
                        return 0;
                    }
                if (waitpid(pid2, &status2, 0) == -1)
                    if (errno != ECHILD && errno != EINTR)
                    {
                        fprintf(stderr, "%s", "error in wait() of parent process\n");
                        return 0;
                    }
            }
        }
    }
    else 
        // if the cmd contains '&', process the background cmd
        if (arglist[count-1][0] == 38)
        {
            // remove '&' from arglist
            arglist[count-1] = NULL;

            pid = fork();
            if (pid < 0)
            {
                fprintf(stderr, "%s", "fork failed\n");
                return 0;
            }
            if (pid == 0)
            {
                // use_sigaction();
                signal(SIGINT, SIG_IGN);
                if (execvp(arglist[0], arglist) == -1)
                    exit_on_error("error on execvp (maybe invalid cmd)\n");
            }
            
            // NO WAITING 
        }
        else
            // if the cmd contains ">>", process the redirection cmd
            if (count >= 2 && 
                strlen(arglist[count-2]) == 2 && 
                arglist[count-2][0] == 62 && 
                arglist[count-2][1] == 62)
                {
                    // remove '>>' from arglist (effectively removes everything after '>>')
                    arglist[count-2] = NULL;

                    pid = fork();
                    if (pid < 0)
                    {
                        fprintf(stderr, "%s", "fork failed\n");
                        return 0;
                    }
                    if (pid == 0)
                    {
                        // make child sensitive to ctrl-c
                        signal(SIGINT, SIG_DFL);

                        // create output file, and redirect program output to it
                        output_file_fs = open(arglist[count-1], O_RDWR | O_CREAT | O_APPEND, 0666);
                        if (output_file_fs == -1)
                            exit_on_error("error when opening file\n");
                        if (dup2(output_file_fs, 1) == -1)
                            exit_on_error("error when using dup on file\n");
                        if (close(output_file_fs) == -1)
                            exit_on_error("error when closing file\n");

                        // change image to requested program
                        if (execvp(arglist[0], arglist) == -1)
                            exit_on_error("error on execvp (maybe invalid cmd)\n");
                    }
                    if (pid > 0)
                    {
                        if (waitpid(pid, &status, 0) == -1)
                            if (errno != ECHILD && errno != EINTR)
                            {
                                fprintf(stderr, "%s", "error in wait() of parent process\n");
                                return 0;
                            }
                    }
                }
                // if you reach this point, then you process a REGULAR CMD
                else
                {
                    pid = fork();
                    if (pid < 0)
                    {
                        fprintf(stderr, "%s", "fork failed\n");
                        return 0;
                    }
                    if (pid == 0)
                    {
                        // make child sensitive to ctrl-c
                        signal(SIGINT, SIG_DFL);
                        
                        if (execvp(arglist[0], arglist) == -1)
                            exit_on_error("error on execvp (maybe invalid cmd)\n");
                    }
                    if (pid > 0)
                    {
                        if (waitpid(pid, &status, 0) == -1)
                            if (errno != ECHILD && errno != EINTR)
                            {
                                fprintf(stderr, "%s", "error in wait() of parent process\n");
                                return 0;
                            }
                    }
                }

    return 1;
}


int prepare(void)
{
    // use_sigaction();
    signal(SIGINT, SIG_IGN);
    return 0;
}


int finalize(void)
{
    return 0;
}