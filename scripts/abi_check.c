#include <aurelian/boot.h>

int main(void)
{
    struct aurelion_boot_info boot_info = {0};
    boot_info.magic = AURELION_BOOT_MAGIC;
    boot_info.abi_version = AURELION_BOOT_ABI_VERSION;
    boot_info.struct_size = sizeof(boot_info);
    return boot_info.magic == 0;
}
