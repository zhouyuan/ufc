#include "ufc.h"

static const char alphanum[] =
"0123456789"
"!@#$%^&*"
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz";

int stringLength = sizeof(alphanum) - 1;

char genRandom()  // Random string generator function.
{

    return alphanum[rand() % stringLength];
}

const char* getString()
{
  char str[4096];
  int i = 0;
  for(i = 0; i < 4096; ++i)
  {
      char _t = genRandom();
      str[i] = _t;
  }

  return str;
}



int main()
{
    ufc_options_t* options = ufc_options_create();
    ufc_t* ufc = NULL;
    int ret = ufc_open("/dev/sdb", options, &ufc);
    int i;
    for(i = 0; i < 1024*4096; i++)
    {
        const char* tmp = getString();;
        int len = 4096;
        ufc_write(ufc, tmp, len, i*4096);
        //ufc_sync(ufc);
    }
    //ufc_sync(ufc);
    ufc_close(ufc);
    ufc_options_destroy(options);
    return 0;
}
