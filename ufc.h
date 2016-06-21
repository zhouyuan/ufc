#ifndef _UFC_H_
#define _UFC_H_

#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

#if defined(__cplusplus)
extern "C"
{
#endif


  typedef struct ufc_options_t
  {
          int create_ifnotexist;
          uint64_t max_file_size;
          size_t user_meta_size;
          size_t ring_cache_size;
          const char* log_prefix;
          int block_size;
          int set_size;
          uint64_t num_sets;
  } ufc_options_t;
  
  typedef struct ufc_entry_meta_t {
/*
      int entry_id;
      int inset_offset;
      int dirty;
*/
      uint64_t lba;
      int entry_size;
  } ufc_entry_meta_t;
  
  typedef struct ufc_set_meta_t {
      ufc_entry_meta_t **emeta;
      uint8_t* free_bits;
      pthread_mutex_t mutex;
  } ufc_set_meta_t;
  
  typedef struct ufc_t {
      int fd;
      const char* path;
      int data_pos;
      const ufc_options_t* options;
      ufc_set_meta_t** smeta;
      pthread_mutex_t mutex;
  } ufc_t;
  
  ufc_options_t* ufc_options_create();
  void ufc_options_destroy(ufc_options_t* options);
  
  int ufc_open(const char* dir, const ufc_options_t* options, ufc_t** tufc);
  int ufc_lookup(ufc_t* ufc, uint64_t lba);
  int ufc_write(ufc_t* ufc, const void* log, size_t loglen, uint64_t lba);
  int ufc_read(ufc_t* ufc, char* data, size_t loglen, uint64_t lba);
  int ufc_sync(ufc_t* ufc);
  int ufc_remove(ufc_t* ufc, uint64_t lba);
  int ufc_alloc(ufc_t* ufc, uint64_t lba, int* free_slot);
  /*
  int ufc_meta_read(ufc_t* ufc, const int key);
  int ufc_meta_write(ufc_t* ufc, const int key, const int addr);
  int ufc_sync_meta(ufc_t* ufc);
  */
  
  int ufc_close(ufc_t* ufc);
#if defined(__cplusplus)
}
#endif


#endif
