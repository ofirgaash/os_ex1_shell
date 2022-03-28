#include <stdio.h> 
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>


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
        pipe(pipe_fds);             

        // create the 2 processes
        pid = fork();
        if (pid == 0)
        {
            close(pipe_fds[0]);
            dup2(pipe_fds[1], 1);
            execvp(arglist[0], arglist);
        }
        if (pid > 0)
        {
            pid2 = fork();
            if (pid2 == 0)
            {
                close(pipe_fds[1]);
                dup2(pipe_fds[0], 0);
                execvp(arglist[pipe_ind + 1], arglist + pipe_ind + 1);
            }
            else
            {
                close(pipe_fds[0]);
                close(pipe_fds[1]);
                waitpid(pid, &status, 0);
                waitpid(pid2, &status2, 0);
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
            if (pid == 0)
                execvp(arglist[0], arglist);
            
            // NO WAITING 
        }
        else
            // if the cmd contains ">>", process the redirection cmd
            if (strlen(arglist[count-2]) == 2 && 
                arglist[count-2][0] == 62 && 
                arglist[count-2][1] == 62)
                {
                    // remove '>>' from arglist
                    arglist[count-2] = NULL;

                    pid = fork();
                    if (pid == 0)
                    {
                        // create output file, and redirect program output to it
                        output_file_fs = open(arglist[count-1], O_WRONLY | O_CREAT | O_APPEND, S_IRWXO);
                        dup2(output_file_fs, 1);
                        close(output_file_fs);

                        // change image to requested program
                        execvp(arglist[0], arglist);
                    }
                    if (pid > 0)
                    {
                        waitpid(pid, &status, 0);
                    }
                }
                else
                {
                    pid = fork();
                    if (pid == 0)
                    {
                        execvp(arglist[0], arglist);
                    }
                    if (pid > 0)
                    {
                        waitpid(pid, &status, 0);
                    }

                }

    return 1;
}


int prepare(void)
{
    return 0;
}


int finalize(void)
{
    return 0;
}