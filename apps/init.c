#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>

int main(void)
{
    int ret = mkdir("testdir", 0755);
    if (ret < 0)
    {
        perror("mkdir");
        return 1;
    }

    puts("mkdir succeeded");
    return 0;
}
