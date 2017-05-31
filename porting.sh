#!/bin/bash

if [ -z $ANDROID_BUILD_TOP ];then
	echo "Please setup your Android build env firstly!"
fi

selinux_set_path=$ANDROID_BUILD_TOP/device/mediatek/common/sepolicy/basic/
echo "Please confirm your SELinux set path: ${selinux_set_path}, then press anykey to continue!"
read -n 1

# add permission to file_contexts
echo "#PPD Atrace monitor" >> ${selinux_set_path}file_contexts
echo "/data/trlog(/.*)? u:object_r:trlog_data_file:s0" >> ${selinux_set_path}file_contexts
echo "/system/bin/atrace_monitor u:object_r:atrace_monitor_exec:s0" >> ${selinux_set_path}file_contexts

# add permission to file.te
echo "#ppd wisen add for trlog" >> ${selinux_set_path}file.te
echo "type trlog_data_file, file_type, data_file_type;" >> ${selinux_set_path}file.te

# add permission to dumpstate.te
echo "# ppd atrace_monitor" >> ${selinux_set_path}dumpstate.te
echo "allow dumpstate trlog_data_file:dir rw_dir_perms;" >> ${selinux_set_path}dumpstate.te
echo "allow dumpstate trlog_data_file:file create_file_perms;" >> ${selinux_set_path}dumpstate.te

echo "allow dumpstate storage_file:lnk_file { read };" >> ${selinux_set_path}dumpstate.te
echo "allow dumpstate mnt_user_file:lnk_file { read };" >> ${selinux_set_path}dumpstate.te
echo "allow dumpstate mnt_user_file:dir rw_dir_perms;" >> ${selinux_set_path}dumpstate.te
echo "allow dumpstate sdcardfs:dir rw_dir_perms;" >> ${selinux_set_path}dumpstate.te
echo "allow dumpstate sdcardfs:file create_file_perms;" >> ${selinux_set_path}dumpstate.te
echo "allow dumpstate media_rw_data_file:dir rw_dir_perms;" >> ${selinux_set_path}dumpstate.te
echo "allow dumpstate media_rw_data_file:file create_file_perms;" >> ${selinux_set_path}dumpstate.te

echo "allow dumpstate debugfs_wakeup_sources:file { read open };" >> ${selinux_set_path}dumpstate.te
echo "allow dumpstate persist_mtk_main_attach:file { getattr open };" >> ${selinux_set_path}dumpstate.te
echo "allow dumpstate mtk_em_ril_apnchange_prop:file { getattr open };" >> ${selinux_set_path}dumpstate.te
echo "allow dumpstate safemode_prop:file { getattr open };" >> ${selinux_set_path}dumpstate.te
echo "allow dumpstate mmc_prop:file { getattr open };" >> ${selinux_set_path}dumpstate.te
echo "allow dumpstate device_logging_prop:file { getattr open };" >> ${selinux_set_path}dumpstate.te

echo "allow dumpstate protect_f_data_file:dir { getattr };" >> ${selinux_set_path}dumpstate.te
echo "allow dumpstate protect_s_data_file:dir { getattr };" >> ${selinux_set_path}dumpstate.te
echo "allow dumpstate system_block_device:blk_file { getattr };" >> ${selinux_set_path}dumpstate.te
echo "allow dumpstate userdata_block_device:blk_file { getattr };" >> ${selinux_set_path}dumpstate.te
echo "allow dumpstate protect1_block_device:blk_file { getattr };" >> ${selinux_set_path}dumpstate.te
echo "allow dumpstate protect2_block_device:blk_file { getattr };" >> ${selinux_set_path}dumpstate.te
echo "allow dumpstate nvdata_device:blk_file { getattr };" >> ${selinux_set_path}dumpstate.te
echo "allow dumpstate nvdata_file:dir { getattr };" >> ${selinux_set_path}dumpstate.te

echo "allow dumpstate sys_lcd_brightness_file:file { read open };" >> ${selinux_set_path}dumpstate.te
echo "allow dumpstate debugfs_binder:file { read open };" >> ${selinux_set_path}dumpstate.te

# add permission to system_app.te
echo "#ppd test atrace_monitor" >> ${selinux_set_path}system_app.te
echo "allow system_app socket_device:sock_file { write };" >> ${selinux_set_path}system_app.te
echo "allow system_app atrace_monitor:unix_stream_socket { connectto };" >> ${selinux_set_path}system_app.te

# copy atrace_monitor.te
te_file="atrace_monitor.te"
echo "Please confirm your te file: ${te_file}, the press anykey to continue!"
read -n 1
cp -av ./prebuild/${te_file} ${selinux_set_path}
