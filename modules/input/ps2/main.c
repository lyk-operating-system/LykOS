#include "mod/module.h"
#include "ps2.h"

void __module_install()
{
    ps2kb_setup();
}

void __module_destroy()
{

}

MODULE_NAME("PS/2 Driver")
MODULE_VERSION("0.1")
MODULE_DESCRIPTION("Driver for PS/2 keyboard and mouse.")
MODULE_AUTHOR("Matei Lupu")
