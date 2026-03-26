/* Implementation details according to Dawson:
Uses opendir to open the current directory and scan all entries using readdir.
For any entry that has the suffix we are looking for, uses stat or fstat (man 2 stat) to get the stat structure associated with that entry and check the modification time (s.st_mtime).
If the modification time is more recent than the last change in the directory, do a fork-execvp of the command-line arguments passed to cmd-watch.
Have the parent process use waitpid to wait for the child to finish and also get its status. The parent will print if the child exited with an error.
*/

#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

const char *TARGET_SUFFIXES[] = {".c", ".h", ".S", ".s"};


// actually executes the compile and waits for child to finish. Only called after a change has been made.
void execute(int argc, char *argv[]) {
    /*
     * 1. Call fork
     * 2. If in child (pid == 0) call execvp to execute given command
     * 3. Parent waits for child to finish, then returns
    */ 
    pid_t pid = fork();
    if (pid == 0) {
        if (execvp(argv[1], argv + 1) == -1) {
            fprintf(stderr, "error in execvp: %s\n", strerror(errno));
        }
    } else {
        int status;
        waitpid(pid, &status, 0);
        // TODO: also consider success reporting here
        if (!WIFEXITED(status)) {
            printf("Child crashed during execvp. Signal = %d\n", WTERMSIG(status));
        }
    }

}

// main worker function which handles all logic. Returns >0 if an entry was changed, and -1 if error
int watch(int argc, char *argv[], struct timespec *dir_last_mod) {
    int changed = 0;
    DIR *d = opendir(".");
    if (d == NULL) {
        printf("cmd_watch: could not open current directory\n");
        return -1;
    }
    if (dir_last_mod->tv_sec == 0) {
        struct stat dir_buf;
        if (stat(".", &dir_buf) == -1) {
            fprintf(stderr, "error in stat(): %s\n", strerror(errno));
            return -1;
        }
        dir_last_mod->tv_sec = dir_buf.st_mtimespec.tv_sec;
        dir_last_mod->tv_nsec = dir_buf.st_mtimespec.tv_nsec;
    }
    struct dirent *cur_d = readdir(d);
    struct stat file_buf;
    while (cur_d != NULL) {
        for (int i = 0; i < (sizeof(TARGET_SUFFIXES) / sizeof(char*)); i++) {
            // check if suffix matches
            if (strstr(cur_d->d_name, TARGET_SUFFIXES[i]) != NULL && strcmp(cur_d->d_name, ".history") != 0) {
                // check if it has been edited more recently than the last change in the dir
                // printf("found name with matched suffix: %s\n", cur_d->d_name);
                if (stat(cur_d->d_name, &file_buf) == -1) {
                    fprintf(stderr, "error in stat(): %s\n", strerror(errno));
                    return -1;
                }
                // printf("file_buf time = %ld. dir mod time = %ld\n", file_buf.st_mtimespec.tv_sec, dir_buf.st_mtimespec.tv_sec);
                if ((file_buf.st_mtimespec.tv_sec > dir_last_mod->tv_sec) || (file_buf.st_mtimespec.tv_sec == dir_last_mod->tv_sec && file_buf.st_mtimespec.tv_nsec > dir_last_mod->tv_nsec)) {
                    dir_last_mod->tv_sec = file_buf.st_mtimespec.tv_sec;
                    dir_last_mod->tv_nsec = file_buf.st_mtimespec.tv_nsec;
                    printf("Detected change in %s.\n", cur_d->d_name);
                    execute(argc, argv); 
                    changed += 1;
                }
                break;
            }
        }
        cur_d = readdir(d);
    }
    closedir(d);
    return changed;
}


int main(int argc, char *argv[]) {
    struct timespec dir_last_mod = {0, 0};
    while (1) {
        // technically Dawson doesn't recommend this, but I think it is good to sleep here
        sleep(1);
        int status = watch(argc, argv, &dir_last_mod);
        if (status == -1) {
            printf("status from watch = -1\n");
            return -1;
        } else if (status > 0) {
            sleep(1);
        }
    }
}

