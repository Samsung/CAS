/**
 * @file worker.cpp
 *  Compile: clang++ -o worker worker.cpp
 * 
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


#define __PACKED    __attribute__((packed))

/* Private structures used for (un)packing data sent
 *  through pipes
 */
struct arr_entry {
    unsigned int size;
    char data[0];
} __PACKED;
struct req_packet {
    unsigned int cwdSize;
    unsigned int inputSize;
    unsigned int cmdLen;
    char data[0];
} __PACKED;

struct resp_packet {
    unsigned int size;
    int return_code;
    unsigned short internal_error;
    char data[0];
} __PACKED;


/* Helper exception class to handle both error message and
 *  errno message in one go
 */
class ExecutionWorkerException : public std::exception {
    std::string msg;
public:
    ExecutionWorkerException(const char* text, bool storeErrno = false) {
        msg = "ExecutionWorker exception: ";
        msg += text;

        if(storeErrno) {
            msg += "(errno: ";
            msg += strerror(errno);
            msg += ")";
        }
    }

    virtual const char* what(void) const throw() {
        return msg.data();
    }
};


/* Main executor class responsible for handling pipes
 *  communication and process creation
 */
class ExecutionWorker {
    int readFD, writeFD, retCode;
    std::vector<char> cwd, output, input;
    std::vector<std::vector<char>> cmd;

    void full_read(int fd, void* buffer, size_t len);
    void full_write(int fd, void* buffer, size_t len);

    void recv(void);
    void send(bool sentError = false);

    void execute(void);

public:
    ExecutionWorker(int _readFD, int _writeFD) 
        : readFD(_readFD), writeFD(_writeFD), retCode(-1) {};

    void processSingleRequest(void);
    void sendError(const char* errorStr);
};


void ExecutionWorker::full_read(int fd, void* buffer, size_t len) {
    size_t offset = 0;
    ssize_t ret;

    while(offset < len) {
        ret = read(fd, (char*)buffer + offset, len - offset);
        if(ret <= 0)
            throw ExecutionWorkerException("encountered error during read operation", true);
        offset += ret;
    }
}

void ExecutionWorker::full_write(int fd, void* buffer, size_t len) {
    size_t offset = 0;
    ssize_t ret;
    
    while(offset < len) {
        ret = write(fd, (char*)buffer + offset, len - offset);
        if(ret <= 0)
            throw ExecutionWorkerException("encountered error during write operation", true);
        offset += ret;
    }
}


void ExecutionWorker::recv(void) {
    struct req_packet packet;

    full_read(readFD, &packet, sizeof(packet));
    cmd.clear();
    cwd.clear();
    input.clear();

    // Read current working dir (CWD)
    cwd.resize(packet.cwdSize + 1);
    full_read(readFD, cwd.data(), packet.cwdSize);
    cwd[packet.cwdSize] = '\0';

    // Read input for process
    input.resize(packet.inputSize);
    full_read(readFD, input.data(), packet.inputSize);

    // Read array of program arguments
    for(unsigned int i = 0; i < packet.cmdLen; i++) {
        std::vector<char> arg;
        struct arr_entry entry;

        full_read(readFD, &entry, sizeof(entry));

        arg.resize(entry.size + 1);
        full_read(readFD, arg.data(), entry.size);
        arg[entry.size] = '\0';

        cmd.push_back(arg);
    }
}

void ExecutionWorker::send(bool sentError) {
    struct resp_packet packet;

    packet.size = output.size();
    packet.internal_error = sentError;
    packet.return_code = retCode;
    full_write(writeFD, &packet, sizeof(packet));
    
    full_write(writeFD, output.data(), output.size());
}


void ExecutionWorker::execute(void) {
    int fd[2], fdRead[2];
    char slice[256];
    int status;
    int retries = 0;
    size_t offset = 0;
    pid_t writerPid = -1;

    output.clear();

    int ret = pipe(fd);
    if(ret < 0)
        throw ExecutionWorkerException("cannot create output pipe to subprocess", true);

    ret = pipe(fdRead);
    if(ret < 0)
        throw ExecutionWorkerException("cannot create input pipe to subprocess", true);

    pid_t pid = fork();
    if(pid > 0) {
        // Parent
        close(fd[1]);
        close(fdRead[0]);

        // If there's any input to write, spawn new writer process
        if(input.size() > 0) {
            writerPid = fork();
            if(writerPid == 0) {
                // Children
                ssize_t ret;
                size_t writeOffset = 0;
                while((ret = write(fdRead[1], 
                        input.data() + writeOffset, 
                        input.size() - writeOffset)) > 0)
                    writeOffset += ret;

                close(fdRead[1]);
                exit(0);
            } else if(writerPid < 0)
                throw ExecutionWorkerException("cannot spawn subprocess for generating input");
        }
        close(fdRead[1]);

        while(retries < 100) {
            ssize_t ret = read(fd[0], slice, sizeof(slice));
            if(ret < 0) {
                kill(pid, SIGKILL);
                throw ExecutionWorkerException("cannot read from children pipe", true);
            } else if (ret == 0 && waitpid(pid, &status, WNOHANG) == pid) {
                break;
            } else if(ret == 0)
                retries++;
            
            output.resize(output.size() + ret);
            memcpy(output.data() + offset, slice, ret);
            offset += ret;
            retries = 0;
        }

        // Make sure our child is cold dead
        kill(pid, SIGKILL);
        waitpid(pid, &status, WNOHANG);

        // Kill writer if it exists
        if(writerPid > 0) {
            kill(writerPid, SIGKILL);
            waitpid(writerPid, nullptr, WNOHANG);
        }

        // Save return code
        if(WIFEXITED(status))
            retCode = WEXITSTATUS(status);
        else
            retCode = -1;

    } else if(pid == 0) {
        // Child
        dup2(fdRead[0], STDIN_FILENO);
        dup2(fd[1], STDOUT_FILENO);
        dup2(fd[1], STDERR_FILENO);
        close(fd[1]);
        close(fd[0]);
        close(fdRead[0]);
        close(fdRead[1]);

        if(cwd.size() > 0 && cwd[0] != '\0') {
            ret = chdir(cwd.data());
            if(ret)
                throw ExecutionWorkerException("cannot change children directory", true);
        }

        std::vector<char*> argv;
        for(size_t i = 0; i < cmd.size(); i++)
            argv.push_back(cmd[i].data());
        argv.push_back(nullptr);

        ret = execv(cmd[0].data(), argv.data());
        throw ExecutionWorkerException("failed to spawn target subprocess", true);
    } else {
        // This is an error
        throw ExecutionWorkerException("fork failed", true);
    }

    close(fd[0]);
}


void ExecutionWorker::processSingleRequest(void) {
    recv();
    execute();
    send();
}

void ExecutionWorker::sendError(const char* errorStr) {
    size_t errorLen = strlen(errorStr);

    output.clear();
    output.resize(errorLen + 1);
    memcpy(output.data(), errorStr, errorLen);
    output[errorLen] = '\0';

    // true - indicate to host that this is an error message
    send(true);
}


/* 
 * Application entry-point
 */
int main(int argc, char** argv) {
    // TODO: Switch to getopt
    if(argc < 3) {
        fprintf(stderr, "Usage: %s <read_fd> <write_fd>\n", argv[0]);
        return 1;
    }
    int read_fd = atoi(argv[1]);
    int write_fd = atoi(argv[2]);


    ExecutionWorker worker(read_fd, write_fd);
    while(1) {
        try {
            worker.processSingleRequest();
        } catch(ExecutionWorkerException& ex) {
            // Send error message to host
            worker.sendError(ex.what());

            // TODO: Maybe we can resume?
            exit(1);
        }
    }
}
