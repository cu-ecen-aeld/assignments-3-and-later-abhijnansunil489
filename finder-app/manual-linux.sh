#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    echo "Deep cleaning the kernel build tree"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    echo "Creating a default config file"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    echo "Building kernel image using 8 threads"
    make -j8 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir rootfs && cd rootfs
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

ROOTFS=$OUTDIR/rootfs

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
else
    cd busybox
fi

# TODO: Make and install busybox
echo "installing busybox"
make distclean
make defconfig
make -j8 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make -j8 CONFIG_PREFIX=${ROOTFS} ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

echo "Library dependencies"
SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)
PROG_INTERP=$(${CROSS_COMPILE}readelf -a ./busybox | grep "program interpreter" | awk -F '/' '{print $NF}' | tr -d ']')
SHRD_LIB=$(${CROSS_COMPILE}readelf -a ./busybox | grep "Shared library" | awk -F '[' '{print $2}'| tr -d ']')
# TODO: Add library dependencies to rootfs

cd "$OUTDIR"
if [ -n "$PROG_INTERP" ]
then
	INTERP_PATH=$(find $SYSROOT -name $PROG_INTERP)
	if [ -n "$INTERP_PATH" ]
	then
		cp $INTERP_PATH $ROOTFS/lib
		echo "copied $PROG_INTERP to rootfs"
	fi
fi

if [ -n "$SHRD_LIB" ]
then
	for S_LIB in $SHRD_LIB;
	do
		SLIB_PATH=$(find $SYSROOT -name $S_LIB)
		if [ -n "$SLIB_PATH" ]
		then
			cp $SLIB_PATH $ROOTFS/lib64
			echo "copied $S_LIB to rootfs"
		fi
		SLIB_PATH=""
	done
fi

# TODO: Make device nodes
sudo mknod -m 666 $ROOTFS/dev/null c 1 3
sudo mknod -m 666 $ROOTFS/dev/console c 5 1
# TODO: Clean and build the writer utility
make -j8 -C  $FINDER_APP_DIR clean
make -j8 -C $FINDER_APP_DIR CROSS_COMPILE=$CROSS_COMPILE

cd $ROOTFS
mkdir home/conf
# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp $FINDER_APP_DIR/writer $FINDER_APP_DIR/finder.sh $FINDER_APP_DIR/finder-test.sh $FINDER_APP_DIR/autorun-qemu.sh home/
cp $FINDER_APP_DIR/conf/username.txt $FINDER_APP_DIR/conf/assignment.txt home/conf


# TODO: Chown the root directory
sudo chown -R root:root .
find . | cpio -H newc -ov --owner root:root  > ${OUTDIR}/initramfs.cpio
# TODO: Create initramfs.cpio.gz
cd $OUTDIR
gzip -f initramfs.cpio
