// engler, cs140e: your code to find the tty-usb device on your laptop.
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "demand.h"
#include "libunix.h"
#include <sys/stat.h>

#define _SVID_SOURCE
#include <dirent.h>
static const char *ttyusb_prefixes[] = {
    "ttyUSB",	// linux
    "ttyACM",   // linux
    "cu.SLAB_USB", // mac os
    "cu.usbserial", // mac os
    // if your system uses another name, add it.
	//0
};

static int filter(const struct dirent *d) {
    // scan through the prefixes, returning 1 when you find a match.
    // 0 if there is no match.
    for (int i = 0; i < sizeof(ttyusb_prefixes) / sizeof(char *); i++) {
        if (strstr(d->d_name, ttyusb_prefixes[i]) != NULL) {
            return 1;
        }
    }
    return 0;
}

// find the TTY-usb device (if any) by using <scandir> to search for
// a device with a prefix given by <ttyusb_prefixes> in /dev
// returns:
//  - device name.
// error: panic's if 0 or more than 1 devices.
char *find_ttyusb(void) {
    // use <alphasort> in <scandir>
    // return a malloc'd name so doesn't corrupt.
    struct dirent **namelist;
    int matches = scandir("/dev", &namelist, *filter, *alphasort);
    if (!matches) {
        panic("no matches found by find_ttyusb!");
    } else if (matches >= 2){
        panic("multiple matches found by find_ttyusb");
    } else {
        char *out = strdupf("/dev/%s", (namelist[0]->d_name));
        return out;
    }
}

// return the most recently mounted ttyusb (the one
// mounted last).  use the modification time 
// returned by state.
char *find_ttyusb_last(void) {
    struct dirent **namelist;
    int matches = scandir("/dev", &namelist, *filter, *alphasort);
    if (!matches) {
        panic("no matches found by find_ttyusb_last!");
    } else {
        struct stat stat_buf;
        struct timespec newest_access;
        char *most_recent_name;
        // a bit janky, assuming 512 is enough space
        char path[512];
        for (int i = 0; i < matches; i++) {
            sprintf(path, "/dev/%s", namelist[i]->d_name);
            if (stat(path, &stat_buf) == -1)  {
                panic("failed to read stat!");
            }
            if (i == 0 || (stat_buf.st_mtimespec.tv_sec > newest_access.tv_sec) || (stat_buf.st_mtimespec.tv_sec == newest_access.tv_sec && stat_buf.st_mtimespec.tv_nsec > newest_access.tv_nsec)) {
                newest_access = stat_buf.st_mtimespec;
                most_recent_name = namelist[i]->d_name;
            }
        }
        char *out = strdupf("/dev/%s", most_recent_name);
        return out;
    }
}

// return the oldest mounted ttyusb (the one mounted
// "first") --- use the modification returned by
// stat()
char *find_ttyusb_first(void) {
    struct dirent **namelist;
    int matches = scandir("/dev", &namelist, *filter, *alphasort);
    if (!matches) {
        panic("no matches found by find_ttyusb_first!");
    } else {
        struct stat stat_buf;
        struct timespec oldest_access;
        char *most_recent_name;
        // a bit janky, assuming 256 is enough space
        char path[512];
        for (int i = 0; i < matches; i++) {
            sprintf(path, "/dev/%s", namelist[i]->d_name);
            if (stat(path, &stat_buf) == -1)  {
                panic("failed to read stat!");
            }
            if (i == 0 || (stat_buf.st_mtimespec.tv_sec < oldest_access.tv_sec) || (stat_buf.st_mtimespec.tv_sec == oldest_access.tv_sec && stat_buf.st_mtimespec.tv_nsec < oldest_access.tv_nsec)) {
                oldest_access = stat_buf.st_mtimespec;
                most_recent_name = namelist[i]->d_name;
            }
        }
        char *out = strdupf("/dev/%s", most_recent_name);
        return out;
    }
}
