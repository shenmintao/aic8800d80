#!/bin/bash
#
# AIC8800D80 Bluetooth Diagnostic Script
# 用于诊断蓝牙问题
#
# 本分支 (bluetooth) 使用内核标准 btusb 驱动接管蓝牙，
# aic_load_fw 负责把蓝牙固件上传到芯片。
# 不应该再安装/加载 aic_btusb。
#

echo "=== AIC8800D80 蓝牙诊断 ==="
echo ""

# 1. 检查 USB 设备
echo "1. USB 设备状态:"
echo "   当前 AIC 设备:"
lsusb | grep -iE "a69c|368b|1111" || echo "   未找到 AIC 设备"
echo ""

# 2. 检查 USB 接口数量
echo "2. USB 接口详情:"
for dev in /sys/bus/usb/devices/*; do
    if [ -f "$dev/idVendor" ] && [ -f "$dev/idProduct" ]; then
        vid=$(cat "$dev/idVendor" 2>/dev/null)
        pid=$(cat "$dev/idProduct" 2>/dev/null)
        if [[ "$vid" == "a69c" || "$vid" == "368b" ]]; then
            echo "   设备: $vid:$pid"
            if [ -d "$dev" ]; then
                for intf in "$dev"/*:*; do
                    if [ -d "$intf" ]; then
                        class=$(cat "$intf/bInterfaceClass" 2>/dev/null)
                        subclass=$(cat "$intf/bInterfaceSubClass" 2>/dev/null)
                        protocol=$(cat "$intf/bInterfaceProtocol" 2>/dev/null)
                        driver=$(basename "$(readlink "$intf/driver" 2>/dev/null)" 2>/dev/null)
                        echo "     接口: $class/$subclass/$protocol -> 驱动: ${driver:-未绑定}"
                    fi
                done
            fi
        fi
    fi
done
echo ""

# 3. 检查已加载的模块
echo "3. 已加载的相关模块 (期望: aic_load_fw + btusb):"
lsmod | grep -E "aic_load_fw|aicwf|aic_btusb|btusb|bluetooth" || echo "   未找到相关模块"
echo ""

# 4. 检查 HCI 设备
echo "4. HCI 设备:"
hciconfig -a 2>/dev/null || echo "   无法获取 HCI 信息 (可能需要 root 权限)"
echo ""

# 5. 检查蓝牙固件加载日志
echo "5. 蓝牙固件加载日志 (最近 50 行):"
dmesg | grep -iE "fw_patch|fw_adid|aicbt|bluetooth|btusb|hci" | tail -50
echo ""

# 6. 检查 rfkill 状态
echo "6. RF-Kill 状态:"
rfkill list bluetooth 2>/dev/null || echo "   无法获取 rfkill 信息"
echo ""

# 7. 检查残留的旧 aic_btusb 配置 (issue #53)
echo "7. 残留旧配置检查:"
legacy_refs_found=false
mapfile -t legacy_modprobe_files < <(
    grep -RIlE '^[[:space:]]*(softdep|alias)[^#]*aic_btusb([[:space:]]|$)' \
        /etc/modprobe.d /run/modprobe.d /usr/local/lib/modprobe.d /usr/lib/modprobe.d /lib/modprobe.d \
        2>/dev/null || true
)
mapfile -t legacy_udev_files < <(
    grep -RIl 'aic_btusb/new_id' \
        /etc/udev/rules.d /run/udev/rules.d /usr/lib/udev/rules.d /lib/udev/rules.d \
        2>/dev/null || true
)

for legacy_file in "${legacy_modprobe_files[@]}"; do
    echo "   ⚠️  发现旧 modprobe 指令: $legacy_file"
    legacy_refs_found=true
done

for legacy_file in "${legacy_udev_files[@]}"; do
    echo "   ⚠️  发现旧 udev 绑定规则: $legacy_file"
    legacy_refs_found=true
done

if lsmod | awk '{print $1}' | grep -qx 'aic_btusb'; then
    echo "   ⚠️  旧 aic_btusb 模块当前仍已加载"
    legacy_refs_found=true
fi

if modinfo -n aic_btusb >/dev/null 2>&1; then
    echo "   ⚠️  系统中仍存在 aic_btusb 模块: $(modinfo -n aic_btusb 2>/dev/null)"
    legacy_refs_found=true
fi

if [ "$legacy_refs_found" = true ]; then
    echo "   这些残留会尝试加载已删除的模块, 并干扰标准 btusb 的自动加载/绑定顺序。"
    echo "   修复: 拉取最新 bluetooth 分支后重新运行 sudo ./install.sh"
    if command -v update-initramfs >/dev/null 2>&1; then
        echo "   然后执行: sudo update-initramfs -u"
    elif command -v dracut >/dev/null 2>&1; then
        echo "   然后执行: sudo dracut -f"
    elif command -v mkinitcpio >/dev/null 2>&1; then
        echo "   然后执行: sudo mkinitcpio -P"
    fi
else
    echo "   未发现活动的 aic_btusb 配置、规则或模块 (正常)"
fi
echo ""

# 8. 建议
echo "=== 诊断建议 ==="
echo ""

# 检查 aic_load_fw 是否加载 (它负责上传 WiFi + BT 固件)
if ! lsmod | grep -q "^aic_load_fw"; then
    echo "⚠️  aic_load_fw 未加载"
    echo "   该模块负责把蓝牙固件上传到芯片, 必须先于 btusb 工作"
    echo "   解决方案: sudo modprobe aic_load_fw"
    echo ""
fi

# 检查 HCI_Reset 超时 (典型症状: 固件未上传却让 btusb 接管)
if dmesg | grep -iE "hci0:.*command (0x|tx) timed out|hci0.*opcode.*0x0c03" | tail -5 | grep -q .; then
    echo "⚠️  检测到 HCI 命令超时 (HCI_Reset/-110)"
    echo "   常见原因:"
    echo "     a) 旧 aic_btusb 配置或 udev 规则残留 (见第 7 项)"
    echo "     b) aic_load_fw 没有先把蓝牙固件 patch 上传到芯片"
    echo "     c) usb_modeswitch 没把 1111:1111 切换到真实 VID:PID"
    echo "   排查: 看上面第 5 项日志, 应能看到 fw_patch_table_8800d80 / fw_adid 字样"
    echo "   临时缓解: 拔插一次设备 (软件复位/飞行模式切换通常无法救活)"
    echo ""
fi

# btusb 是否已接管 BT 接口 (这是正常预期)
if lsmod | grep -q "^btusb"; then
    echo "✅ btusb 已加载 (正常, 本分支由 btusb 接管蓝牙)"
fi

echo "=== 诊断完成 ==="
