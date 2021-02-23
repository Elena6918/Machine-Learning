#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

struct Command {
    pid_t pid;
    string args_in;
    string args_out;
    vector<string> args;
};

bool isOperator(const string &s) {
    return s == "|" || s == "<" || s == ">";
}

void redirect(int old_fd, int new_fd){
    if(old_fd != new_fd){
        dup2(old_fd, new_fd);
        close(old_fd);
    }
}

void parse_and_run_command(const string& init_command) {
    istringstream s(init_command);
    vector<string> tokens;
    string tk;
    while (s >> tk) {
        tokens.push_back(tk);
    }

    int TOKEN_SIZE = tokens.size();
    if (TOKEN_SIZE == 0) {
        return;
    }

    int count_in = 0, count_out = 0;
    vector<Command> commands;
    Command temp_Command;
    for (int i = 0; i < TOKEN_SIZE; i++) {
        auto &token = tokens[i];

        if (token == "<") {
            count_in += 1;
            if (count_in > 1) {
                cerr << "Invalid command: multiple redirection command." << endl;
                return;
            }
            if (i + 1 >= TOKEN_SIZE || isOperator(tokens[i + 1])) {
                cerr << "Invalid command: need argument after a redirection operator." << endl;
                return;
            }
            temp_Command.args_in = tokens[++i];
        } else if (token == ">") {
            count_out += 1;
            if (count_out > 1) {
                cerr << "Invalid command: multiple redirection command." << endl;
                return;
            }
            if (i + 1 >= TOKEN_SIZE || isOperator(tokens[i + 1])) {
                cerr << "Invalid command: need argument after a redirection operator." << endl;
                return;
            }
            temp_Command.args_out = tokens[++i];
        } else if (token == "|") {
            if (i + 1 >= TOKEN_SIZE || isOperator(tokens[i + 1])) {
                cerr << "Invalid command: need argument after a pipe operator." << endl;
                return;
            }

            if (!temp_Command.args.size()) {
                cerr << "Invalid command: need to specify command name." << endl;
                return;
            }
            commands.push_back(temp_Command);
            temp_Command = Command();
        } else if (token == "exit") {
            exit(0);
        } else {
            temp_Command.args.push_back(token);
        }
    }
    if (!temp_Command.args.size()) {
        cerr << "Invalid command: need to specify command name." << endl;
        return;
    }
    commands.push_back(temp_Command);

    int N = commands.size();
    for (int i = 0; i < N; i++) {
        auto& command = commands[i];

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return;
            command.pid = pid;
        } else if (pid == 0) {
            int arg_num = command.args.size();
            vector<char*> args(arg_num + 1);
            for (int i = 0; i < arg_num; i++) {
                args[i] = (char*)command.args[i].c_str();
            }
            args[arg_num] = NULL;

            if (command.args_in.size()) {
                int fd = open(command.args_in.c_str(), O_RDONLY);
                if (fd < 0) {
                    perror("Failed to open file");
                    exit(-1);
                }
                redirect(fd, 0);
            }
            if (command.args_out.size()) {
                int fd = open(command.args_out.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (fd < 0) {
                    perror("Failed to open file");
                    exit(-1);
                }
                redirect(fd, 1);
            }
            
            if (i < N - 1) {
                redirect(fd[1], 1);
                close(fd[0]);
            }

            execv(args[0], args.data());
            perror("execv");
            exit(-1);
        } else {
            command.pid = pid;
        }
    }

    for (auto &command : commands) {
        int status;
        waitpid(command.pid, &status, 0);  
        cout << command.args[0] << " exit status: " << WEXITSTATUS(status) << endl;
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
