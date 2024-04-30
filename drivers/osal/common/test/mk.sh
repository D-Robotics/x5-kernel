#!/bin/bash

${CROSS_COMPILE}gcc -Wall -O2 -o osal_test_fifo osal_common_test_fifo.c ../src/osal_common_fifo.c -I../inc -lpthread -DOSAL_POSIX_TEST -DCONFIG_64BIT
cp -f osal_test_fifo  ~/tftp/

${CROSS_COMPILE}gcc -Wall -O2 -o osal_test_atomic osal_common_test_atomic.c  -I../inc -lpthread -DOSAL_COMMON_TEST -DCONFIG_64BIT
cp -f osal_test_atomic  ~/tftp/

${CROSS_COMPILE}gcc -Wall -O2 -o osal_test_bitops osal_common_test_bitops.c  -I../inc -lpthread -DOSAL_COMMON_TEST -DCONFIG_64BIT

cp -f osal_test_bitops  ~/tftp/
