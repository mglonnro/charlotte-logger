#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h> 
#include <fcntl.h>

#include "fileupload.h"

#define BUFSIZE 2048
#define TMP_BUFSIZE (BUFSIZE*3)
#define PATHSIZE 256
#define ROTATEINTERVAL 300
#define ROTATELINES    50000

// Gzip and upload a specific log file
void process_log(char *fname, char *fname_withpath);
void process_old();
void usage(char *bin);
void housekeeper();

static int verbose_flag, retry_minutes = -1, purge_days = -1, json_flag = 0;
static char logdir[PATHSIZE], upload_to[PATHSIZE];
static char fname[PATHSIZE], fname_withpath[PATHSIZE];

int main(int argc, char **argv) {

  memset(logdir, 0, PATHSIZE);
  memset(upload_to, 0, PATHSIZE);

  signal(SIGPIPE, SIG_IGN);

  while (1) {
    static struct option long_options[] = {
        /* These options set a flag. */
        {"verbose", no_argument, &verbose_flag, 1},
        {"retry", required_argument, 0, 'r'},
        {"purge-done", required_argument, 0, 'p'}, 
        {"upload-to", required_argument, 0, 'u'},
	{"json", no_argument, &json_flag, 1},
        {0, 0, 0, 0}};

    int option_index = 0;
    int c;

    c = getopt_long(argc, argv, "p:r:u:", long_options, &option_index);

    /* Detect the end of the options. */
    if (c == -1)
      break;

    switch (c) {
    case 0:
      /* If this option set a flag, do nothing else now. */
      if (long_options[option_index].flag != 0)
        break;
      printf("option %s", long_options[option_index].name);
      if (optarg)
        printf(" with arg %s", optarg);
      printf("\n");
      break;

    case 'r':
      retry_minutes = atoi(optarg);
      break;

    case 'p':
      purge_days = atoi(optarg);
      break;

    case 'u':
      strcpy(upload_to, optarg);
      break;

    default:
      usage(argv[0]);
      exit(1);
    }
  }

  int too_many = 0;

  if (optind < argc) {
    while (optind < argc) {
      if (!logdir[0]) {
        strcpy(logdir, argv[optind++]);
      } else {
        too_many = 1;
        optind++;
      }
    }
  }

  if (!logdir[0] || too_many) {
    usage(argv[0]);
    exit(1);
  }

  // Make stdout non-blocking for continued logging even if charlotte-client crashes.
  int flags = fcntl(STDOUT_FILENO, F_GETFL);
  fcntl(STDOUT_FILENO, F_SETFL, flags | O_NONBLOCK);

  struct stat st = {0};

  if (stat(logdir, &st) == -1) {
    mkdir(logdir, 0700);
  }

  char tmp[TMP_BUFSIZE];
  memset(tmp, 0, TMP_BUFSIZE);
  sprintf(tmp, "%s/done", logdir);

  if (stat(tmp, &st) == -1) {
    mkdir(tmp, 0700);
  }

  char str[BUFSIZE];
  memset(str, 0, BUFSIZE);
  FILE *f = NULL;
  char prevfile[PATHSIZE];
  memset(prevfile, 0, PATHSIZE);
  time_t prevtime = 0;
  int line_count = 0;

  memset(fname, 0, PATHSIZE);
  memset(fname_withpath, 0, PATHSIZE);

  // Background worker to take care of compressind and (re)sending old files, 
  // as well as purging already sent logs.
  housekeeper();

  while (fgets(str, sizeof str, stdin) != NULL) {
    // File handling
    line_count ++;

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    if (!f || t > (prevtime + ROTATEINTERVAL) || (line_count >= ROTATELINES)) {
      // Clear zombies
      int status;
      while (waitpid(-1, &status, WNOHANG) > 0)
        ;

      line_count = 0;

      prevtime = t;

      // Changing file
      if (f) {
        fclose(f);
	char tmp_fname[PATHSIZE], tmp_fname_withpath[PATHSIZE];
	memset(tmp_fname, 0, PATHSIZE);
	memset(tmp_fname_withpath, 0, PATHSIZE);
	strcpy(tmp_fname, fname);
	strcpy(tmp_fname_withpath, fname_withpath);
	
        // Process log in new forked thread.
	if (fork() == 0) {
          process_log(tmp_fname, tmp_fname_withpath);
	  exit(0);
	}
      }

      sprintf(fname, "%s-%d%02d%02d-%02d%02d%02d.%s", "nmealog",
              tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
              tm.tm_min, tm.tm_sec, (json_flag == 1 ? "json.log" : "log") );
      sprintf(tmp, "%s/%s", logdir, fname);
      strncpy(fname_withpath, tmp, PATHSIZE);

      f = fopen(fname_withpath, "a");
      if (!f) {
        fprintf(stderr, "FATAL: Could not open file %s for appending.",
                fname_withpath);
        return 1;
      }
    }

    // Print to stdout for piping
    fprintf(stdout, "%s", str);
    fflush(stdout);

    // Save to log
    fprintf(f, "%s", str);
    fflush(f);

    str[strlen(str) - 1] = 0;
  }
  fclose(f);

  // Process final log before exiting
  process_log(fname, fname_withpath);
  wait(NULL);

  return 0;
}

// Gzip and upload a specific log file
void process_log(char *fname, char *fname_withpath) {
  char tmp[TMP_BUFSIZE];
  memset(tmp, 0, TMP_BUFSIZE);

  char transfer_fname[PATHSIZE], transfer_fname_withpath[PATHSIZE];
  memset(transfer_fname, 0, PATHSIZE);
  memset(transfer_fname_withpath, 0, PATHSIZE);

  strcpy(transfer_fname, fname);
  strcpy(transfer_fname_withpath, fname_withpath);

    char cmd[2048];
    memset(cmd, 0, 2048);

    // Zip it, if it isn't
    if (strstr(transfer_fname_withpath, ".gz") == NULL) {
      sprintf(cmd, "nice -n 10 gzip %s", transfer_fname_withpath);
      printf("%s\n", cmd);
      int status = system(cmd);
	
      if (status == -1 || status != 0) {
        fprintf(stderr, "gzip of log %s failed\n", transfer_fname_withpath);
	return;
      }

      sprintf(tmp, "%s.gz", transfer_fname_withpath);
      strncpy(transfer_fname_withpath, tmp, PATHSIZE);

      sprintf(tmp, "%s.gz", transfer_fname);
      strncpy(transfer_fname, tmp, PATHSIZE);
    }

    if (upload_to[0]) {
      int retries = 3, done = 0;
      while (!done && retries > 0) {
        // Upload it

        int ret =
            fileupload(upload_to, transfer_fname_withpath, transfer_fname);

        if (!ret) {
          // Move to done
          sprintf(cmd, "nice -n 10 mv %s %s/done", transfer_fname_withpath,
                  logdir);
          system(cmd);
          done = 1;
        } else {
          retries--;
	  sleep(3);
        }
      }
    }
}

// Go through the log directory and: 
// a) Compress all uncompressed logs that are != the current one. 
// b) Attempt to send them. 

void process_old() {
  DIR *d;
  struct dirent *dir;
  d = opendir(logdir);
  if (d) {
    while ((dir = readdir(d)) != NULL) {
      if (fname[0] && strstr(dir->d_name, fname)) {
	// Currently open log
	continue;
      }

      if (strstr(dir->d_name, ".log")) {

  	char name[PATHSIZE], fullname[PATHSIZE];
	memset(name, 0, PATHSIZE);
	memset(fullname, 0, PATHSIZE);
	strcpy(name, dir->d_name);

        char tmp[TMP_BUFSIZE];
        memset(tmp, 0, TMP_BUFSIZE);
   
 	sprintf(tmp, "%s/%s", logdir, name);	
	strncpy(fullname, tmp, PATHSIZE);

	struct stat sb;
	if (stat(fullname, &sb) != -1) {
	  long now;
	  time(&now);

	  // Improve this with locks
	  if ( (now - sb.st_mtime) > (retry_minutes * 60L)) {
            process_log(name, fullname);
	  }
	}
      }

    }
    closedir(d);
  }
}

void purge_old() {
  DIR *d;
  struct dirent *dir;
  char donedir[PATHSIZE];
  memset(donedir, 0, PATHSIZE);

  char tmp[TMP_BUFSIZE];
  memset(tmp, 0, TMP_BUFSIZE);

  sprintf(tmp, "%s/done", logdir);
  strncpy(donedir, tmp, PATHSIZE);

  d = opendir(donedir);

  if (d) {
    while ((dir = readdir(d)) != NULL) {
      if (strstr(dir->d_name, ".log.gz")) {
        char name[PATHSIZE], fullname[PATHSIZE];
        memset(name, 0, PATHSIZE);
        memset(fullname, 0, PATHSIZE);
        strcpy(name, dir->d_name);

        sprintf(tmp, "%s/%s", donedir, name);
        strncpy(fullname, tmp, PATHSIZE);

        struct stat sb;
        if (stat(fullname, &sb) != -1) {
          long now;
          time(&now);

          // Improve this with locks
          if ( (now - sb.st_mtime) > (purge_days * 24L * 3600L)) {
	    printf("Purging %s\n", fullname);
          }
        }
      }
    }
    closedir(d);
  }
}


void
housekeeper() {
  if (retry_minutes == -1 && purge_days == -1) {
    return;
  }

  if (fork() == 0) {
    while (1) {
      if (retry_minutes != -1) {
        process_old();
      }

      if (purge_days != -1) {
	purge_old();
      }

      sleep(10);
    }
  }
}

void usage(char *bin) {
  printf("charlotte-logger v0.0.5\n");
  printf("usage: %s [--upload-to boatid] [--purge-done days] [--retry minutes] DIRECTORY\n",
         bin);
  printf("\n");
  printf("--upload-to boatid          Upload logs to the cloud.\n");
  /* printf("--purge-done days           Delete uploaded log files at least 'days' old\n"); */
  printf("                            that havee been successfully uploaded.\n");
  printf("--retry minutes             Retry processing logs older than 'minutes'.\n");
  printf("DIRECTORY                   Directory to store the logs.\n");
}
