/*
 * Copyright (c) 2022, Justin Bradley
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

// Edited by Bryan Duong
// 09/29/2024

#include <iostream>
#include <string>
#include <vector>

#include "command.hpp"
#include "parser.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_ALLOWED_LINES 25

int execute(const shell_command& cmd)
{
    std::vector<char*> cstrs;
    cstrs.push_back(const_cast<char*>(cmd.cmd.c_str()));

    for(size_t i = 0; i < cmd.args.size(); ++i)
        cstrs.push_back(const_cast<char*>(cmd.args[i].c_str()));

    cstrs.push_back(NULL);

    return execvp(cstrs[0], cstrs.data());
}
    
void redirect_input(shell_command& cmd) {
    if(cmd.cin_mode == istream_mode::file) {
        int file_desc = open(cmd.cin_file.c_str(), O_RDONLY);
        dup2(file_desc, 0);
    }
}

void redirect_output(shell_command& cmd) {
    int file_desc;
    if(cmd.cout_mode == ostream_mode::file) {
        file_desc = open(cmd.cout_file.c_str(), O_CREAT | O_RDWR, 0644);
        dup2(file_desc, 1);
    }
    else if(cmd.cout_mode == ostream_mode::append) {
        file_desc = open(cmd.cout_file.c_str(), O_CREAT | O_APPEND | O_WRONLY, 0644);
        dup2(file_desc, 1);
    }
}

void run(std::vector<shell_command> shell_commands)
{
    int is_run = 1;
    int is_run_prev = 1;

    for (shell_command cmd: shell_commands) {
        pid_t cpid = fork();

        if(cpid < 0) {
            fprintf(stderr, "Fork Failed\n");
            exit(1);
        }
        else if (cpid == 0) {
            redirect_input(cmd);
            redirect_output(cmd);
            
            if (is_run)
                execute(cmd);                
            
            exit(1);

        }
        else {
            int status;
            waitpid(cpid, &status, 0);

            next_command_mode next_mode = cmd.next_mode;
            is_run = (next_mode == next_command_mode::on_success && WEXITSTATUS(status) == 0) ||
                      (next_mode == next_command_mode::on_fail && WEXITSTATUS(status) != 0)
                      || (next_mode == next_command_mode::always);

            is_run = is_run && is_run_prev;
            is_run_prev = is_run;
        }
    }
}

int main (int argc, char *argv[])
{
    std::string input_line;

    if (argc > 1 && argv[1] == std::string("-t")) {

      while (std::getline(std::cin, input_line)) {
        if(input_line == "exit") {
          exit(0);
        }
        else {
          try {
            std::vector<shell_command> shell_commands 
                = parse_command_string(input_line);

            run(shell_commands);
          }
          catch (const std::runtime_error& e) {
              std::cout << e.what() << std::endl;
          }
        }
      }
  }

    else { 

        for (int i=0;i<MAX_ALLOWED_LINES;i++) { // Limits the shell to MAX_ALLOWED_LINES
            // Print the prompt.
            std::cout << "osh> " << std::flush;

            // Read a single line.
            if (!std::getline(std::cin, input_line) || input_line == "exit") {
                break;
            }

            try {
                // Parse the input line.
                std::vector<shell_command> shell_commands
                        = parse_command_string(input_line);

                // Print the list of commands.
                std::cout << "-------------------------\n";
                for (const auto& cmd : shell_commands) {
                    std::cout << cmd;
                    std::cout << "-------------------------\n";
                }

                // run the commands.
                run(shell_commands);
            }
            catch (const std::runtime_error& e) {
                std::cout << "osh: " << e.what() << "\n";
            }
        }

        std::cout << std::endl;
}
    return 0;
}

