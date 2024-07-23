#include <iostream>
#include <unistd.h>
#include <sys/utsname.h>
#include <vector>
#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string>
#include <sstream>
#include <sys/stat.h>
#include <dirent.h>
#include <ctime>
#include <pwd.h>
#include <grp.h>
#include <fstream>
#include <csignal>
#include <readline/readline.h>
#include <readline/history.h>

using namespace std;

pid_t fgPid = -1; // Global variable to keep track of the foreground process ID
const string HISTORY_FILE = ".custom_shell_history"; // History file
const int MAX_HISTORY_SIZE = 20; // Maximum number of commands to store in history
const int MAX_DISPLAY_HISTORY = 10; // Maximum number of commands to display with 'history' command

vector<string> commandHistory; // Vector to store command history

void handleSigTSTP(int sig) {
    if (fgPid != -1) {
        kill(fgPid, SIGTSTP);
        cout << endl;
    }
}

void handleSigINT(int sig) {
    if (fgPid != -1) {
        kill(fgPid, SIGINT);
        cout << endl;
    }
}

void echoCommand(const string& text) {
    cout << text << endl;
}

void cdCommand(const string &path) {
    if (path.empty()) {
        // No argument provided, change to the user's home directory
        const char* homePath = getenv("HOME");
        if (homePath) {
            if (chdir(homePath) != 0) 
                cerr << "Error changing directory to home." << endl;
        } else {
            cerr << "User's home directory not found." << endl;
        }
    } else if (path == "~") {
        const char* homePath = getenv("HOME");
        if (chdir(homePath) != 0)
            cerr << "Error changing directory to home." << endl;
    } else if (path == "-") {
        const char* prevDir = getenv("OLDPWD");
        if (prevDir) {
            if (chdir(prevDir) != 0)
                cerr << "Error changing directory to previous one." << endl;
        } else {
            cerr << "Previous directory not found." << endl;
        }
    } else {
        if (chdir(path.c_str()) != 0)
            cerr << "Error changing directory to " << path << endl;
    }
}

void pwdCommand() {
    char buffer[1024];
    if (getcwd(buffer, sizeof(buffer)) != nullptr) {
        cout << buffer << endl;
    } else {
        cerr << "Error getting current directory." << endl;
    }
}

void printFileDetails(const string& path, const string& filename) {
    struct stat fileStat;
    if (stat((path + "/" + filename).c_str(), &fileStat) == -1) {
        perror("stat");
        return;
    }

    cout << ((S_ISDIR(fileStat.st_mode)) ? 'd' : '-');
    cout << ((fileStat.st_mode & S_IRUSR) ? 'r' : '-');
    cout << ((fileStat.st_mode & S_IWUSR) ? 'w' : '-');
    cout << ((fileStat.st_mode & S_IXUSR) ? 'x' : '-');
    cout << ((fileStat.st_mode & S_IRGRP) ? 'r' : '-');
    cout << ((fileStat.st_mode & S_IWGRP) ? 'w' : '-');
    cout << ((fileStat.st_mode & S_IXGRP) ? 'x' : '-');
    cout << ((fileStat.st_mode & S_IROTH) ? 'r' : '-');
    cout << ((fileStat.st_mode & S_IWOTH) ? 'w' : '-');
    cout << ((fileStat.st_mode & S_IXOTH) ? 'x' : '-');
    cout << " " << fileStat.st_nlink;
    cout << " " << getpwuid(fileStat.st_uid)->pw_name;
    cout << " " << getgrgid(fileStat.st_gid)->gr_name;
    cout << " " << fileStat.st_size;
    cout << " " << ctime(&fileStat.st_mtime);
    cout << " " << filename;
    cout << endl;
}

void lsCommand(const vector<string>& args) {
    bool showAll = false;
    bool longFormat = false;
    vector<string> paths;

    for (const string& arg : args) {
        if (arg == "-a") {
            showAll = true;
        } else if (arg == "-l") {
            longFormat = true;
        } else if (arg == "-la" || arg == "-al") {
            showAll = true;
            longFormat = true;
        } else {
            paths.push_back(arg);
        }
    }

    if (paths.empty()) {
        paths.push_back("."); // Default path is current directory
    }

    for (const string& path : paths) {
        DIR* dir = opendir(path.c_str());
        if (!dir) {
            cerr << "Error opening directory: " << path << endl;
            continue;
        }

        struct dirent* entry;
        vector<string> files;
        while ((entry = readdir(dir)) != nullptr) {
            if (!showAll && entry->d_name[0] == '.') {
                continue;
            }
            files.push_back(entry->d_name);
        }
        closedir(dir);

        if (longFormat) {
            for (const string& file : files) {
                printFileDetails(path, file);
            }
        } else {
            for (const string& file : files) {
                cout << file << " ";
            }
            cout << endl;
        }
    }
}

// Function to generate matches for autocompletion
char* commandGenerator(const char* text, int state) {
    static vector<string> matches;
    static size_t matchIndex;

    const vector<string> commands = {"echo", "cd", "pwd", "ls", "history", "exit"};

    if (state == 0) {
        matches.clear();
        matchIndex = 0;

        for (const string& cmd : commands) {
            if (strncmp(cmd.c_str(), text, strlen(text)) == 0) {
                matches.push_back(cmd);
            }
        }

        DIR* dir = opendir(".");
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                if (strncmp(entry->d_name, text, strlen(text)) == 0) {
                    matches.push_back(entry->d_name);
                }
            }
            closedir(dir);
        }
    }

    if (matchIndex < matches.size()) {
        return strdup(matches[matchIndex++].c_str());
    }

    return nullptr;
}

// Function to set up autocompletion
void initializeReadline() {
    rl_attempted_completion_function = [](const char* text, int start, int end) -> char** {
        rl_attempted_completion_over = 1;
        return rl_completion_matches(text, commandGenerator);
    };
}

// Function to load history from file
void loadHistory() {
    ifstream historyFile(HISTORY_FILE);
    if (historyFile.is_open()) {
        string line;
        while (getline(historyFile, line)) {
            commandHistory.push_back(line);
        }
        historyFile.close();
    }
}

// Function to save history to file
void saveHistory() {
    ofstream historyFile(HISTORY_FILE);
    if (historyFile.is_open()) {
        for (const auto& cmd : commandHistory) {
            historyFile << cmd << endl;
        }
        historyFile.close();
    }
}

// Function to display the last 10 commands
void historyCommand() {
    int start = max(0, static_cast<int>(commandHistory.size()) - MAX_DISPLAY_HISTORY);
    for (int i = start; i < commandHistory.size(); ++i) {
        cout << i + 1 << " " << commandHistory[i] << endl;
    }
}

// Function to add a command to the history
void addToHistory(const string& cmd) {
    if (commandHistory.size() >= MAX_HISTORY_SIZE) {
        commandHistory.erase(commandHistory.begin());
    }
    commandHistory.push_back(cmd);
}

int main() {
    // Set up signal handlers
    signal(SIGTSTP, handleSigTSTP); // CTRL-Z
    signal(SIGINT, handleSigINT);   // CTRL-C

    // Load history from file
    loadHistory();

    // Get the username
    char* username = getlogin();
    if (!username) {
        cerr << "Error getting username" << endl;
        return 1;
    }

    // Get the hostname
    struct utsname systemInfo;
    if (uname(&systemInfo) == -1) {
        cerr << "Error getting system information" << endl;
        return 1;
    }

    // Initialize readline
    initializeReadline();

    while (true) {
        // Build and display the custom prompt
        char cwd[1024];
        if (!getcwd(cwd, sizeof(cwd))) {
            cerr << "Error getting current directory" << endl;
            return 1;
        }

        string prompt = string(username) + "@" + systemInfo.nodename + ":" + cwd + "$ ";
        char* input = readline(prompt.c_str());
        if (!input || strlen(input) == 0) {
            free(input);
            continue;
        }

        string inputStr(input);
        addToHistory(inputStr); // Add the command to custom history
        free(input);

        if (inputStr == "exit") {
            break;
        } else if (inputStr.substr(0, 4) == "echo") {
            string arg = inputStr.substr(5);
            echoCommand(arg);
        } else if (inputStr.substr(0, 2) == "cd") {
            string path = inputStr.substr(3);
            cdCommand(path);
        } else if (inputStr.substr(0, 2) == "ls") {
            istringstream iss(inputStr);
            vector<string> args;
            string arg;
            while (iss >> arg) {
                if (arg != "ls") {
                    args.push_back(arg);
                }
            }
            lsCommand(args);
        } else if (inputStr == "pwd") {
            pwdCommand();
        } else if (inputStr == "history") {
            historyCommand();
        } else {
            // Handle external commands
            pid_t pid = fork();
            if (pid == 0) {
                // Child process
                istringstream iss(inputStr);
                vector<string> args;
                string arg;
                while (iss >> arg) {
                    args.push_back(arg);
                }
                vector<char*> cargs;
                for (const auto& arg : args) {
                    cargs.push_back(const_cast<char*>(arg.c_str()));
                }
                cargs.push_back(nullptr);

                if (execvp(cargs[0], cargs.data()) == -1) {
                    perror("execvp");
                    exit(EXIT_FAILURE);
                }
            } else if (pid < 0) {
                // Fork failed
                perror("fork");
            } else {
                // Parent process
                fgPid = pid; // Set the foreground process ID
                int status;
                waitpid(pid, &status, WUNTRACED); // Wait for the process to complete or be stopped
                if (WIFSTOPPED(status)) {
                    fgPid = -1; // Reset the foreground process ID if the process was stopped
                } else {
                    fgPid = -1; // Reset the foreground process ID if the process finished
                }
            }
        }
    }

    // Save history to file
    saveHistory();

    return 0;
}

