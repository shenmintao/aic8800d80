#!/bin/bash
#
# AIC8800D80 Bluetooth Diagnostic Script
# 用于诊断蓝牙问题
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
echo "3. 已加载的 AIC 模块:"
lsmod | grep -E "aic|btusb" || echo "   未找到相关模块"
echo ""

# 4. 检查 HCI 设备
echo "4. HCI 设备:"
hciconfig -a 2>/dev/null || echo "   无法获取 HCI 信息 (可能需要 root 权限)"
echo ""

# 5. 检查蓝牙固件加载日志
echo "5. 蓝牙固件加载日志 (最近 50 行):"
dmesg | grep -iE "fw_patch|fw_adid|aicbt|bluetooth|btusb|aic_btusb|hci" | tail -50
echo ""

# 6. 检查 rfkill 状态
echo "6. RF-Kill 状态:"
rfkill list bluetooth 2>/dev/null || echo "   无法获取 rfkill 信息"
echo ""

# 7. 检查 modprobe 配置
echo "7. Modprobe 配置:"
if [ -f /etc/modprobe.d/aic8800-bt.conf ]; then
    echo "   /etc/modprobe.d/aic8800-bt.conf 存在:"
    cat /etc/modprobe.d/aic8800-bt.conf
else
    echo "   /etc/modprobe.d/aic8800-bt.conf 不存在!"
fi
echo ""

# 8. 建议
echo "=== 诊断建议 ==="
echo ""

# 检查是否有 3 个接口
intf_count=$(lsusb -t 2>/dev/null | grep -c "Class=Wireless" || echo "0")
if [ "$intf_count" -lt 2 ]; then
    echo "⚠️  蓝牙接口数量不足 (应该有 2 个 Wireless 接口)"
    echo "   可能原因: aic_load_fw 没有加载蓝牙固件"
    echo "   解决方案: 检查 dmesg 中是否有 'fw_patch_table_8800d80' 相关日志"
    echo ""
fi

# 检查 btusb 是否抢占
if lsmod | grep -q "^btusb"; then
    echo "⚠️  btusb 模块已加载"
    echo "   如果蓝牙不工作，可能是 btusb 抢占了设备"
    echo "   解决方案: sudo rmmod btusb && sudo modprobe aic_btusb"
    echo ""
fi

# 检查 aic_btusb 是否加载
if ! lsmod | grep -q "aic_btusb"; then
    echo "⚠️  aic_btusb 模块未加载"
    echo "   解决方案: sudo modprobe aic_btusb"
    echo ""
fi

echo "=== 诊断完成 ==="
