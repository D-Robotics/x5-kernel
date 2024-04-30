#!/bin/bash

# This script gen stub OSAL APIs from OSAL Linux APIs
# The stub APIs are used for checking improper linux kernel API usage
# Also it's a way to compare OSAL APIs protypes with other OS

LINUX_INC=../linux/inc/
for f in ${LINUX_INC}/*;
do
	f=$(basename "$f")
	echo "--- $f ----"
	HEAD_TAG=`echo $f | tr '[:lower:]' '[:upper:]' | sed 's/\./_/'`
	HEAD_TAG=__${HEAD_TAG}__
	echo "--- $HEAD_TAG----"
	echo "#ifndef $HEAD_TAG" > $f
	echo "#define $HEAD_TAG" >> $f
	echo >> $f
	grep "typedef" ${LINUX_INC}$f | grep "{"
	if [ $? -eq 0 ];then
		if [ $f =  'osal_time.h' ];then
			echo "typedef int osal_timer_t;" >> $f
		fi

		if [ $f =  'osal_thread.h' ];then
			echo "typedef int osal_thread_t;" >> $f
			echo "typedef int osal_thread_attr_t;" >> $f
			echo "typedef int osal_thread_attr_type_t;" >> $f
			echo "typedef int32_t osal_thread_entry_t(void* data);" >> $f
			echo "//osal_thread_entry_t func, void *data) {return 0;}" >> $f
		fi
	else
		grep "typedef" ${LINUX_INC}$f >> $f
	fi

	lines=`grep "static inline" ${LINUX_INC}$f`

	echo "$lines" | while IFS= read -r line; do
		retval=`echo $line | awk '{print $3}'`
		if [ $retval = "void" ];then
			echo $line | sed 's/)/) {return;}/' >> $f
		else
			echo $line | sed 's/)/) {return 0;}/' >> $f
		fi
	done

	echo >> $f
	echo "#endif //$HEAD_TAG" >> $f
done

cp ${LINUX_INC}/osal.h osal.h

