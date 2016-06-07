#include "ufc.h"

int main()
{
    ufc_options_t* options = ufc_options_create();
    ufc_t* wal = NULL;
    int ret = ufc_open("/dev/loop2", options, &wal);
    int i;
    for(i = 0; i < 1024*4096; i++)
    {
        char tmp[1024];
        int len = sprintf(tmp, "set key%010d val%010d", i, i);
        ufc_write(wal, tmp, len, i*4096);
        //ufc_sync(wal);
    }
    ufc_close(wal);
    ufc_options_destroy(options);
    return 0;
}
