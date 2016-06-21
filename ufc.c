#include "ufc.h"

const uint64_t UFC_DEFAULT_SIZE = 5 * 1024 * 1024 * 1024LL;

ufc_options_t* ufc_options_create()
{
    ufc_options_t* options = (ufc_options_t*) malloc(sizeof(ufc_options_t));
    memset(options, 0, sizeof(ufc_options_t));
    options->create_ifnotexist = 1;
    options->max_file_size = UFC_DEFAULT_SIZE;
    options->user_meta_size = 0;
    options->ring_cache_size = 0;
    options->block_size = 4096;
    options->set_size = 512;
    options->num_sets = options->max_file_size / (options->block_size * options->set_size);
    return options;
}

void ufc_options_destroy(ufc_options_t* options)
{
    if (NULL != options)
    {
        free(options);
    }
}

static int open_cachedev(ufc_t* ufc)
{
    if (ufc->fd > 0) {
        return 0;
    }
    
    const char* dev = ufc->path;
    int mode = O_RDWR | O_DSYNC;
    int fd = open(dev, mode);
    if (fd < 0)
    {
        return -1;
    }
    struct stat file_st;
    memset(&file_st, 0, sizeof(file_st));
    fstat(fd, &file_st);
    lseek(fd, ufc->data_pos, SEEK_SET);
    ufc->fd = fd;
    return 0;
}

int ufc_open(const char* path, const ufc_options_t* options, ufc_t** tufc)
{

    ufc_t* ufc = (ufc_t*) malloc(sizeof(ufc_t));
    if (NULL == ufc) {
        return -1;  
    }
    memset(ufc, 0, sizeof(ufc_t));
    ufc->fd = -1;
    ufc->path = path;
    ufc->options = options;

    ufc->data_pos = 0; //TODO allow to store ondisk metadata
    ufc->smeta = (ufc_set_meta_t**) malloc(ufc->options->num_sets *sizeof(ufc_set_meta_t));
    
    int i, j;
    for (i = 0; i < ufc->options->num_sets ; i++) {
        ufc_set_meta_t* ufc_set_meta = (ufc_set_meta_t*) malloc(sizeof(ufc_set_meta_t));
        ufc_set_meta->emeta = (ufc_entry_meta_t**) malloc(ufc->options->set_size * sizeof(ufc_entry_meta_t));
        ufc_set_meta->free_bits = (uint8_t*) malloc(ufc->options->set_size * sizeof(uint8_t));
        for (j = 0; j < ufc->options->set_size ; j++) {
            ufc_entry_meta_t* ufc_entry_meta = (ufc_entry_meta_t*) malloc(sizeof(ufc_entry_meta_t));
            ufc_entry_meta->lba = -1;
            ufc_entry_meta->entry_size = 0;
            ufc_set_meta->emeta[j] = ufc_entry_meta;
            ufc_set_meta->free_bits[j] = 0;
        }
        ufc->smeta[i] = ufc_set_meta;
    }

    int ret = open_cachedev(ufc);
    *tufc = ufc;
    
    return ret;
}

int get_target_set(ufc_t* ufc, uint64_t lba)
{
    int set_id = (lba / ufc->options->block_size / ufc->options->set_size) % ufc->options->num_sets;
    return set_id;

}

int get_inset_offset(ufc_t* ufc, uint64_t lba)
{

    int offs = lba % (ufc->options->block_size * ufc->options->set_size);
    return offs;
}

int ufc_lookup(ufc_t* ufc, uint64_t lba)
{

    int set_id = get_target_set(ufc, lba);

    int t_offset = -1;
    int i;
    for (i = 0; i < ufc->options->set_size ; i++) {
        if (lba == ufc->smeta[set_id]->emeta[i]->lba) {
            // hit
            t_offset = set_id * (ufc->options->block_size * ufc->options->set_size) + ufc->options->block_size * i;
            break;
        }
    }

    return t_offset;
}

int ufc_alloc(ufc_t* ufc, uint64_t lba, int* free_slot)
{

    int set_id = get_target_set(ufc, lba);
    int t_offset = -1;
    int i = 0;
    pthread_mutex_lock(&(ufc->smeta[set_id]->mutex));
    for (i = 0; i < ufc->options->set_size; i++) {
        if (ufc->smeta[set_id]->free_bits[i] == 0) {
            t_offset = set_id * (ufc->options->block_size * ufc->options->set_size) + ufc->smeta[set_id]->free_bits[i] * ufc->options->block_size;
            *free_slot = i;
            break;
        }
    }
    pthread_mutex_unlock(&(ufc->smeta[set_id]->mutex));
    return t_offset;

/*
    if (-1 == t_offset) {
        //TODO ufc_remove_set();
        for (i = 0; i < ufc->options->set_size; i++ ) {
            ufc->smeta[set_id]->free_bits[i] = 0;
        }
    }
*/

}

int ufc_write(ufc_t* ufc, const void* log, size_t loglen, uint64_t lba)
{
    if (NULL == ufc) {
        printf("ufc null \n");
        return -1;
    }

    const char* p = (const char*) log;

    int t_offset = ufc_lookup(ufc, lba);

    int free_slot = -1;

    if (-1 == t_offset) {

        t_offset = ufc_alloc(ufc, lba, &free_slot);
    }

    if ((-1 == t_offset) || (-1 == free_slot) ) {

        //no free space
        return -1;
    }

    int set_id = get_target_set(ufc, lba);

    //printf("set_id = %d, t_offset=%d\n", set_id, t_offset);
    pthread_mutex_lock(&(ufc->mutex));
    lseek(ufc->fd, t_offset, SEEK_SET);
    int err = write(ufc->fd, p, loglen);
    pthread_mutex_unlock(&(ufc->mutex));
    if (err < 0) {
        printf("error when write\n");
        return err;
    }

    ufc->smeta[set_id]->emeta[ufc->smeta[set_id]->free_bits[free_slot]]->lba = lba;
    ufc->smeta[set_id]->emeta[ufc->smeta[set_id]->free_bits[free_slot]]->entry_size = loglen;

    return 0;
  
}

int ufc_read(ufc_t* ufc, char* data, size_t loglen, uint64_t lba)
{
    int set_id = get_target_set(ufc, lba);

    void* p = (void*) data;
    int t_offset = ufc_lookup(ufc, lba);

    if (-1 == t_offset) {
        return 0;
    }

    pthread_mutex_lock(&(ufc->mutex));
    lseek(ufc->fd, t_offset, SEEK_SET);
    size_t read_len = read(ufc->fd, p, loglen);
    pthread_mutex_unlock(&(ufc->mutex));

    if (read_len != loglen) {
        printf("error when read\n");
        return -1;
    }

    return 0;
}

int ufc_remove(ufc_t* ufc, uint64_t lba)
{
    int set_id = get_target_set(ufc, lba);
    int i;

    pthread_mutex_lock(&(ufc->smeta[set_id]->mutex));
    for(i = 0; i < ufc->options->set_size ; i++) {
        if (lba == ufc->smeta[set_id]->emeta[i]->lba) {
            // read hit
            ufc->smeta[set_id]->emeta[i]->lba = -1;
            ufc->smeta[set_id]->free_bits[i] = 0;
            break;
        }
    }

    pthread_mutex_unlock(&(ufc->smeta[set_id]->mutex));
    return 0;

}

int ufc_sync(ufc_t* ufc) 
{
    return fdatasync(ufc->fd);
}

int ufc_close(ufc_t* ufc)
{

    if (NULL == ufc) {
        return -1;
    }
    
    if (-1 != ufc->fd) {
        close(ufc->fd);
        ufc->fd = -1;
    }

    int i = 0;
    int j = 0;
    if (NULL != ufc->smeta) {
        for (; i < ufc->options->num_sets; i++) {
            for (; j < ufc->options->set_size; j++) {
                free(ufc->smeta[i]->emeta[j]);
            }
            free(ufc->smeta[i]->free_bits);
            free(ufc->smeta[i]->emeta);
            free(ufc->smeta[i]);
        }
    }
    
    free (ufc);
    return 0;
}
