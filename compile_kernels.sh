#!/bin/bash

export ARCH=arm
export CROSS_COMPILE=arm-none-linux-gnueabi-
COMMIT=$(git rev-parse --short HEAD)

echo 'Kernel commit: ' $COMMIT
if [ "$1" = "configs" ]; then
  while true; do
	make menuconfig
	read -p "enter config extension ('Exit' to start compilation) " ext
	case $ext in
		Exit ) break;;
		* ) cp .config configs/test_config_${ext};;
	esac
  done
fi

echo "Compiling Kernels"
configs=$(ls configs/test_config_*)
RESULT=""
for config in $configs
do
	echo $config
	cp $config .config
	ext=$(echo `basename $config` | sed -e 's/test_config_//g')
	OUTPUT=vmlinux_${COMMIT}_${ext}
	echo $OUTPUT
	echo
	if [ ! -f $OUTPUT ]; then
		make -j16 >/dev/null && cp vmlinux $OUTPUT
		if [ $? -ne 0 ]; then
			RESULT+="`echo -e $OUTPUT 'FAILED\n'`"
		fi
	else
		echo ${OUTPUT} exists!
	fi
done

echo $RESULT
