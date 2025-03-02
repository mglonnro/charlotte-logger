#include "hash.h"
#include <curl/curl.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define SERVER "https://community.nakedsailor.blog/api.beta"
// #define SERVER "http://localhost:3100"

static size_t read_callback(void *ptr, size_t size, size_t nmemb,
                            void *stream) {
  size_t retcode;
  curl_off_t nread;

  retcode = fread(ptr, size, nmemb, stream);

  nread = (curl_off_t)retcode;

  fprintf(stderr, "*** We read %" CURL_FORMAT_CURL_OFF_T " bytes from file\n",
          nread);

  return retcode;
}

int fileupload(char *boatId, char *fullname, char *basename) {
  CURL *curl;
  CURLcode res;
  FILE *hd_src;
  struct stat file_info;
  char hash[2048];
  memset(hash, 0, 2048);
  int retvalue = 1;

  /* get hash for file */
  if (hashfile(fullname, hash) == NULL) {
    return 1;
  }

  printf("Hash: %s\n", hash);

  char realurl[2048];
  memset(realurl, 0, 2048);
  sprintf(realurl, "%s/boat/%s/chunk?hash=%s&filename=%s", SERVER, boatId, hash,
          basename);

  printf("realurl: %s\n", realurl);

  /* get the file size of the local file */
  stat(fullname, &file_info);

  /* get a FILE * of the same file, could also be made with
     fdopen() from the previous descriptor, but hey this is just
     an example! */
  hd_src = fopen(fullname, "rb");

  /* In windows, this will init the winsock stuff */
  curl_global_init(CURL_GLOBAL_ALL);

  /* get a curl handle */
  curl = curl_easy_init();
  if (curl) {
    /* we want to use our own read function */
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);

    /* enable uploading */
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

    /* HTTP PUT please */
    curl_easy_setopt(curl, CURLOPT_PUT, 1L);

    /* specify target URL, and note that this URL should include a file
       name, not only a directory */
    curl_easy_setopt(curl, CURLOPT_URL, realurl);

    /* now specify which file to upload */
    curl_easy_setopt(curl, CURLOPT_READDATA, hd_src);

    /* provide the size of the upload, we specicially typecast the value
       to curl_off_t since we must be sure to use the correct data size */
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE,
                     (curl_off_t)file_info.st_size);

    /* Now run off and do what you've been told! */
    fprintf(stderr, "Starting CURL send.\n");
    res = curl_easy_perform(curl);
    fprintf(stderr, "Back from CURL send.\n");
    /* Check for errors */
    if (res != CURLE_OK) {
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));
      retvalue = 1;
    } else {
      fprintf(stderr, "No CURL errors.\n");

      long response_code;
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
      
      if (response_code != 200) {
 	fprintf(stderr, "Response code error: %ld\n", response_code);	
	retvalue = 1;
      } else {
        retvalue = 0;
      }
    }

    /* always cleanup */
    curl_easy_cleanup(curl);
    fprintf(stderr, "Done curl_easy_cleanup\n");
  } 
  fclose(hd_src); /* close the local file */

  curl_global_cleanup();
  fprintf(stderr, "Done curl_global_clanup\n");
  return retvalue;
}
