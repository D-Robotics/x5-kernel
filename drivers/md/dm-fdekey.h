#include <linux/err.h>
#include <linux/key-type.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/tee_drv.h>
#include <linux/uuid.h>
#include <linux/types.h>

int get_fde_key_from_efuse(unsigned char *key, char *dr_key);
