#include <iostream>
#include <fstream>
#include <sstream>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/stat.h>

#include <vector>
#include <string>

#include <iomanip>

#include "Tokenizer.h"

static const int MAX_MESSAGE = 256;

// all the basic colours for a shell prompt
#define RED "\033[1;31m"
#define GREEN "\033[1;32m"
#define YELLOW "\033[1;33m"
#define BLUE "\033[1;34m"
#define WHITE "\033[1;37m"
#define NC "\033[0m"

using namespace std;

int main()
{
    vector<pid_t> bg;
    // create copies of stdin/stdout; dup;
    int stdin_copy = dup(0);
    int stdout_copy = dup(1);
    // previous directory
    char prev_dir[MAX_MESSAGE];
    getcwd(prev_dir, MAX_MESSAGE);

    for (;;)
    {
        // add check for background process - add pid to vector if bg and don't waitpid() in par
        // implement iteration over vector of background pid (vector also declared outside of loop)
        // waitpid() - using flag for non-blocking
        
        // redirecting input and output
        dup2(stdin_copy, 0);
        dup2(stdout_copy, 1);

        // implement user prompt
        // implement username with GETENV()
        // implement curdir with getcwd()
        // need date/time, username, and absolute path to current dir
        time_t curr_time = time(NULL);
        string date = ctime(&curr_time);
        date = date.substr(4, 16);
        string user_name = getenv("USER");
        char curr_dir[MAX_MESSAGE];
        getcwd(curr_dir, MAX_MESSAGE);
        cout << YELLOW << date << user_name << ":" << curr_dir << "$" << NC << " ";

        // get user inputted command
        string input;
        getline(cin, input);

        if (input == "exit")
        { // print exit message and break out of infinite loop
            cout << RED << "Now exiting shell..." << endl
                 << "Goodbye" << NC << endl;
            break;
        }

        // chdir for 'cd -'
        // outside variable for previous directory
        if (input.find("cd") != string::npos)
        {
            // cout << "changing directory" << endl;
            if (input.find(" -") != string::npos)
            {
                // cout << "going to previous directory" << endl;
                chdir(prev_dir);
                continue;
            }
            else
            {
                // const char *new_dir = const_cast<char *>(input.substr(3).c_str());
                // cout << "going to new directory:" << new_dir << ":"<<endl;
                chdir(const_cast<char *>(input.substr(3).c_str()));
                continue;
            }
        }

        // get tokenized commands from user input
        Tokenizer tknr(input);
        if (tknr.hasError())
        { // continue to next prompt if input had an error
            continue;
        }

        for (size_t i = 0; i < bg.size(); ++i)
        {
            if (waitpid(bg[i], 0, WNOHANG))
            {
                bg.erase(bg.begin() + i);
                i--;
                printf("Process %ld ended", i);
            }
        }

        // // print out every command token-by-token on individual lines
        // // prints to cerr to avoid influencing autograder
        /*
        for (auto cmd : tknr.commands)
        {
            for (auto str : cmd->args)
            {
                cerr << "|" << str << "| ";
            }
            if (cmd->hasInput())
            {
                cerr << "in< " << cmd->in_file << " ";
            }
            if (cmd->hasOutput())
            {
                cerr << "out> " << cmd->out_file << " ";
            }
            cerr << endl;
        }*/

        // for piping
        // for cmd : commands
        //      call pipe() to make pipe
        //      fork() - in child, redirect stoud; in par, redirect stdin
        // add checks for first/last commands
        for (size_t cmd = 0; cmd < tknr.commands.size(); ++cmd)
        {
            int fd[2];
            pipe(fd);

            // fork to create child
            pid_t pid = fork();
            if (pid < 0)
            { // error check
                perror("fork");
                exit(2);
            }

            if (pid == 0) // if child, exec to run command
            {
                // run single commands with no arguments
                // implement multiple arguments
                size_t command_len = tknr.commands[cmd]->args.size();
                char **args = new char *[command_len + 1];
                for (size_t i = 0; i < command_len; ++i)
                    args[i] = const_cast<char *>(tknr.commands[cmd]->args[i].c_str());
                args[command_len] = NULL;

                // if current command is redirected, then open file and dup2 std(in/out) that's being redirected
                // implement it safely for both at same time
                if (tknr.commands[cmd]->hasInput()) // redirected input
                {
                    int read = open(tknr.commands[cmd]->in_file.c_str(), O_RDONLY, S_IWUSR | S_IRUSR);
                    dup2(read, 0);
                    close(read);
                }
                if (tknr.commands[cmd]->hasOutput()) // redirected output
                {
                    int write = open(tknr.commands[cmd]->out_file.c_str(), O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
                    dup2(write, 1);
                    close(write);
                }

                if (cmd < tknr.commands.size() - 1) // to avoid NULL at end of args
                {
                    dup2(fd[1], 1);
                    close(fd[1]);
                }

                if (execvp(args[0], args) < 0) // error check
                {
                    perror("execvp");
                    exit(2);
                }

                delete [] args;
            }
            else // if parent, wait for child to finish
            {
                if (tknr.commands[cmd]->isBackground())
                    bg.push_back(pid);
                else
                {
                    if (cmd < tknr.commands.size() - 1) // to avoid NULL at end of args
                    {
                        int status = 0;
                        waitpid(pid, &status, 0);
                        if (status > 1) // exit if child didn't exec properly
                            exit(status);
                    }
                }

                dup2(fd[0], 0);
                close(fd[1]);
            }
        }

        // restore stdin/stdout (variable should be outside the loop)
        dup2(stdin_copy, 0);
        dup2(stdout_copy, 1);
    }
}
