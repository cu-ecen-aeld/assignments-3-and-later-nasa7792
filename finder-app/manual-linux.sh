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
    #cleanup
    make -j6 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    # prepare defconfig
    make -j6 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    #compile and build
    make -j6 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} 
    #build kernel modules
    make -j6 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    #build device tree
    make -j6 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs   

fi
#copy Image file from boot folder post build to OUTDIR
echo "Adding the Image in outdir"
    cp "${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image" "${OUTDIR}/"

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir -p "${OUTDIR}/rootfs"
cd "${OUTDIR}/rootfs"
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
 make -j6 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} 
 make -j6 CONFIG_PREFIX="${OUTDIR}/rootfs" ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}  install 

echo "Library dependencies"
cd "${OUTDIR}/rootfs"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs 
#get path for sys root of host 
HOST_SYS_ROOT=$(aarch64-none-linux-gnu-gcc  -print-sysroot)
cp "$HOST_SYS_ROOT/lib/ld-linux-aarch64.so.1" "${OUTDIR}/rootfs/lib/"
cp "$HOST_SYS_ROOT/lib64/libc.so.6" "${OUTDIR}/rootfs/lib64/"
cp "$HOST_SYS_ROOT/lib64/libresolv.so.2" "${OUTDIR}/rootfs/lib64/"
cp "$HOST_SYS_ROOT/lib64/libm.so.6" "${OUTDIR}/rootfs/lib64/"

# TODO: Make device nodes to interact on the qemu
sudo mknod -m 666 dev/null c 1 3 
sudo mknod -m 666 dev/console c 5 1 

# TODO: Clean and build the writer utility

echo $FINDER_APP_DIR
cd ${FINDER_APP_DIR}
make clean 
make CROSS_COMPILE=${CROSS_COMPILE}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs

mkdir -p "${OUTDIR}/rootfs/home/conf"
cp writer.sh finder.sh writer start-qemu-app.sh finder-test.sh autorun-qemu.sh "${OUTDIR}/rootfs/home/"
cp conf/username.txt "${OUTDIR}/rootfs/home/conf"
cp conf/assignment.txt "${OUTDIR}/rootfs/home/conf"

# TODO: Chown the root directory
cd "${OUTDIR}/rootfs"
sudo chown -R root:root *

# TODO: Create initramfs.cpio.gz
find . | cpio -H newc -ov --owner root:root >${OUTDIR}/initramfs.cpio
cd ${OUTDIR} 
gzip -f initramfs.cpio
