#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

struct Command {
    pid_t pid;
    string iredir;
    string oredir;
    vector<string> args;
};

bool isOperator(const std::string& s) {
    return s == "|" || s == "<" || s == ">";
}

void parse_and_run_command(const std::string& rawCommand) {
    istringstream s(rawCommand);
    vector<string> tokens;
    string _token;
    while (s >> _token) {
        tokens.push_back(_token);
    }
    int N = tokens.size();
    if (N == 0) {
        return;
    }

    int inRedirCount = 0, outRedirCount = 0;
    vector<Command> commands;
    Command curCommand;
    for (int i = 0; i < N; i++) {
        auto& token = tokens[i];
        if (token == "<") {
            inRedirCount += 1;

            if (inRedirCount > 1) {
                std::cerr << "Invalid command: at most 1 input redirection can appear in your command.\n";
                return;
            }
            if (i + 1 >= N || isOperator(tokens[i + 1])) {
                std::cerr << "Invalid command: no commands after a redirection operator!\n";
                return;
            }
            curCommand.iredir = tokens[++i];
        } else if (token == ">") {
            outRedirCount += 1;

            if (outRedirCount > 1) {
                std::cerr << "Invalid command: at most 1 output redirection can appear in your command.\n";
                return;
            }
            if (i + 1 >= N || isOperator(tokens[i + 1])) {
                std::cerr << "Invalid command: no commands after a redirection operator!\n";
                return;
            }
            curCommand.oredir = tokens[++i];
        } else if (token == "|") {
            if (i + 1 >= N || isOperator(tokens[i + 1])) {
                std::cerr << "Invalid command: no commands after a pipe operator!\n";
                return;
            }

            if (!curCommand.args.size()) {
                std::cerr << "Invalid command: no command name specified!\n";
                return;
            }
            commands.push_back(curCommand);
            curCommand = Command();
        } else if (token == "exit") {
            exit(0);
        } else {
            curCommand.args.push_back(token);
        }
    }
    if (!curCommand.args.size()) {
        std::cerr << "Invalid command: no command name specified!\n";
        return;
    }
    commands.push_back(curCommand);

    N = commands.size();

    // previous read and write end of the pipe
    int pread = -1;
    int pwrite = -1;
    for (int i = 0; i < N; i++) {
        auto& command = commands[i];

        int pipefd[2];
        if (i < N - 1) {
            int r = pipe(pipefd);
            if (r == -1) {
                perror("Failed to pipe");
                return;
            }
        }

        auto pid = fork();
        if (pid == 0) {
            int numArgs = command.args.size();
            vector<char*> args(numArgs + 1);
            for (int i = 0; i < numArgs; i++) {
                args[i] = (char*)command.args[i].c_str();
            }
            args[numArgs] = NULL;
            if (command.iredir.size()) {
                int fd = open(command.iredir.c_str(), O_RDONLY);
                if (fd < 0) {
                    perror("Failed to open file");
                    exit(-1);
                }
                dup2(fd, 0);
                close(fd);
            }
            if (command.oredir.size()) {
                int fd = open(command.oredir.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (fd < 0) {
                    perror("Failed to open file");
                    exit(-1);
                }
                dup2(fd, 1);
                close(fd);
            }
            // replace out with write end
            if (i < N - 1) {
                dup2(pipefd[1], 1);
                close(pipefd[0]);
                close(pipefd[1]);
            }
            // replace in with read end
            if (i > 0) {
                dup2(pread, 0);
                close(pread);
                close(pwrite);
            }

            execv(args[0], args.data());

            perror("Error running command");
            exit(-1);
        } else if (pid < 0) {
            perror("Fork failed");
            return;
            command.pid = pid;
        } else {
            command.pid = pid;
        }
        if (i > 0) {
            close(pread);
            close(pwrite);
        }
        pread = pipefd[0];
        pwrite = pipefd[1];
    }

    for (auto& command : commands) {
        int status;
        waitpid(command.pid, &status, 0);  // won't block if pid is -1
        cout << command.args[0] << " exit status: " << WEXITSTATUS(status) << "\n";
    }
}

int main(void) {
    std::string command;
    std::cout << "> ";
    while (std::getline(std::cin, command)) {
        parse_and_run_command(command);
        std::cout << "> ";
    }
    return 0;
}
