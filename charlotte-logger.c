#include <stdio.h>
#include <string.h>
#include <time.h> 
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "fileupload.h"

#define BUFSIZE 	2048
//#define ROTATEINTERVAL	300
#define ROTATEINTERVAL	10

int
main(int argc, char **argv)
{

  if ((argc != 3 && argc != 5) || (argc == 5 && strcmp(argv[3], "--upload-to") != 0)) {
    printf("usage: %s <log directory> <basename> [--upload-to <boatId>]\n", argv[0]);
    return 1;
  }

  struct stat st = {0};

  if (stat(argv[1], &st) == -1) {
    mkdir(argv[1], 0700);
  }

  char tmp[2048];
  memset(tmp, 0, 2048);
  sprintf(tmp, "%s/done", argv[1]);

  if (stat(tmp, &st) == -1) {
    mkdir(tmp, 0700);
  }

  char str[BUFSIZE];
  FILE *f = NULL;
  char prevfile[256];
  memset(prevfile, 0, 256);
  time_t prevtime = 0;

  while (fgets(str, sizeof str, stdin) != NULL) {
    // File handling
    char fname[256], fullname[256], transfername[256], transferfullname[256];
    memset(fname, 0, 256);
    memset(fullname, 0, 256),
    memset(transfername, 0, 256);
    memset(transferfullname, 0, 256);
    
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    
    if (!f || t > (prevtime + ROTATEINTERVAL)) {
      // Clear zombies
      int status;
      while (waitpid(-1, &status, WNOHANG) > 0)
	;

      prevtime = t;

      // Changing file
      if (f) {
	fclose(f);
  	strcpy(transferfullname, fullname);
	strcpy(transfername, fname);
	
	if (fork() == 0) {
          char cmd[2048];
	  memset(cmd, 0, 2048);

	  // Zip it
          sprintf(cmd, "nice -n 10 gzip %s", transferfullname);
          system(cmd);

	  if (argc == 5) {

	    int retries = 3, done = 0;

	    while (!done && retries > 0) {
	    // Upload it

	    sprintf(transferfullname, "%s.gz", transferfullname);
	    sprintf(transfername, "%s.gz", transfername);

	    int ret = fileupload(argv[4], transferfullname, transfername);

	    if (!ret) {
	      // Move to done
	      sprintf(cmd, "nice -n 10 mv %s %s/done", transferfullname, argv[1]); 
	      system(cmd);
	      done = 1;
	    } else {
	      retries --;
	    } 
	    }
	  }

	  exit(0);
	} else {
	  //wait(NULL);
	}
      }

	
      sprintf(fname, "%s-%d%02d%02d-%02d%02d%02d.log", argv[2], tm.tm_year + 1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
      sprintf(fullname, "%s/%s", argv[1], fname);

      f = fopen(fullname, "a"); 
      if (!f) {
        fprintf(stderr, "FATAL: Could not open file %s for appending.", fullname);
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
  return 0;
}
