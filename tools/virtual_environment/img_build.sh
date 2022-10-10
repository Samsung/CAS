#!/usr/bin/env bash
IMG_URL=https://cloud-images.ubuntu.com/minimal/releases/focal/release/ubuntu-20.04-minimal-cloudimg-amd64.img
IMG_RESIZE_SIZE=20G
MEM=$(free --mega | grep "Mem:" | awk '{printf "%.0f",($2 * 0.8)}')
PROC=$(nproc | awk '{printf "%.0f",($1 * 0.8)}')

SOCKETS=$(lscpu -J | jq -r '.lscpu[]| select(.field=="Socket(s):").data')
CORES=$(lscpu -J | jq -r '.lscpu[]| select(.field=="Core(s) per socket:").data')
TPERCORE=$(lscpu -J | jq -r '.lscpu[]| select(.field=="Thread(s) per core:").data')

IMG=system_base.img
if [ ! -f "${IMG}" ]; then
    echo "Downloading image from ${IMG_URL} to ${IMG}"
    curl -s -o ${IMG} "${IMG_URL}" || { echo "Download fail"; rm ${IMG}; exit 2; }
fi

BOOT_IMG=$(cat /proc/cmdline | awk '{split($1,x,"="); print x[2]}')
if [ ! -r "/boot/${BOOT_IMG#/}" ]; then
  if [ ! -f "${BOOT_IMG#/}" ] ; then
    echo "**************************************************************"
    echo "Can't read booted kernel file. Probably you are running Ubuntu (where access to kernel image is blocked for user)."
    echo "Copying /boot/${BOOT_IMG#/} to current dir."
    echo "**************************************************************"
    sudo cp /boot/${BOOT_IMG#/} .
    sudo chown $USER:$USER ${BOOT_IMG#/}
  fi
  export SUPERMIN_KERNEL=$(pwd)/${BOOT_IMG#/}
fi

IMG_RESIZED=system_resized.img
if [ ! -f "$IMG_RESIZED" ]; then
    echo "Resizing image"
    qemu-img create -f qcow2 ${IMG_RESIZED} ${IMG_RESIZE_SIZE} || { echo "Create resized fail"; exit 2; }
    virt-resize --format=qcow2 --expand /dev/sda1 ${IMG} ${IMG_RESIZED} | tee /tmp/resize_info || { echo "Resize fail"; rm ${IMG_RESIZED}; exit 2; }
    EXPANDED_PARTITION=$(grep "Expanding" /tmp/resize_info | sed -r 's/.*now (\/dev\/.+)\).*/\1/g')
    rm /tmp/resize_info
    echo "Reinstalling grub on ${EXPANDED_PARTITION}"
    virt-customize -a ${IMG_RESIZED} -smp ${PROC} -m ${MEM} \
      --run-command "mkdir -p /mnt && mount ${EXPANDED_PARTITION} /mnt && mount --bind /dev /mnt/dev && mount --bind /proc /mnt/proc && mount --bind /sys /mnt/sys && chroot /mnt && grub-install /dev/sda" || { echo "Grub install fail"; rm ${IMG_RESIZED}; exit 2; }
fi

IMG_WORK=cas_tracer_env.img
if [ ! -f "${IMG_WORK}" ]; then
  cp "${IMG_RESIZED}" "${IMG_WORK}"

  echo "Customizing image"
  echo " -installing packages"
  virt-customize -a ${IMG_WORK} -smp ${PROC} -m ${MEM} \
    --root-password password:pass \
    --install "build-essential,linux-headers-generic,git,cmake,llvm,clang,clang-10,libclang-dev,python3-dev,gcc-9-plugin-dev,rsync,clang-11,flex,bison,lld-11,libssl-dev,file,python2,python2.7-numpy,python-futures" || { echo "Customizing fail"; rm ${IMG_WORK}; exit 2; }

  echo " -cloning and building CAS packages"
  virt-customize -a ${IMG_WORK} -smp ${PROC} -m ${MEM} \
    --mkdir /opt/ \
    --run-command 'rm -rf /opt/cas/ && git clone https://github.com/Samsung/CAS.git /opt/cas/' \
    --run-command 'cd /opt/cas/tracer/ && make -C /lib/modules/$(ls /lib/modules/ | grep kvm)/build M=$PWD && make -C /lib/modules/$(ls /lib/modules/ | grep kvm)/build M=$PWD modules_install' \
    --run-command 'cd /opt/cas/bas/ && cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ .. && make && make install' \
    --run-command 'cd /opt/cas/ && ./etrace_install.sh' \
    --run-command 'echo "\nexport PATH=\"/opt/cas/:$PATH"\" >> /root/.bashrc' \
    --firstboot-command "depmod -a" || { echo "Customizing fail"; rm ${IMG_WORK}; exit 2; }

  echo " -enable network and ssh"
  virt-customize -a ${IMG_WORK} -smp ${PROC} -m ${MEM} \
    --run-command "sed -i 's/GRUB_CMDLINE_LINUX=/GRUB_CMDLINE_LINUX=\"net.ifnames=0 biosdevname=0\"/g' /etc/default/grub" \
    --run-command "update-grub" \
    --run-command "echo '
  network:
    version: 2
    renderer: networkd
    ethernets:
      eth0:
        dhcp4: true
        optional: true
  ' > /etc/netplan/01-netcfg.yaml" \
    --firstboot-command "netplan generate && netplan apply" \
    --firstboot-command "dpkg-reconfigure openssh-server" || { echo "Customizing fail"; rm ${IMG_WORK} exit 2; }

  echo "Image ${IMG_WORK} customized."
else
  echo "Image ${IMG_WORK} already built."
fi

echo "\nExample next steps"

echo -e "\n* Create source image"
echo "virt-make-fs --format=qcow2 --size=+10G /some/source/dir/ src.img"

echo -e "\n* Create a work layer - it will make src.img immutable."
echo "qemu-img create -f qcow2 -b src.img -F qcow2 work.qcow2"

echo -e "\n* Run Qemu"
echo "qemu-system-x86_64 --enable-kvm -nographic -cpu host -m ${MEM} -smp cores=${CORES},threads=${TPERCORE},sockets=${SOCKETS} -drive file=cas_tracer_env.img -drive file=work.qcow2 -device virtio-net,netdev=vmnic -netdev user,id=vmnic"
