# Intro

Hello.

Some instructions how to build rpm file and then install rpm file on Bazzite.

I saw video on Youtube about Asrock AMD BC-250, then I bought this motherboard.

On printables I found a case and recomendations about WIFI BT stick.

There is only one problem - Bazzite has read-only filesystem. 

# How to build from spec file.


Install the build tools, the development package for the running kernel, and
the runtime mode-switch tools. Reboot once so the layered packages are active.

~~~bash
sudo rpm-ostree install rpm-build rpmdevtools gcc make "kernel-devel-$(uname -r)" usb_modeswitch util-linux
sudo systemctl reboot
~~~

Prepare rpmbuild folder

~~~bash
cd $HOME
rpmdev-setuptree
~~~

Copy aic8800d80.spec to rpmbuild/SPECS

~~~bash
cd $HOME/rpmbuild/SPECS
curl -LO -s https://raw.githubusercontent.com/shenmintao/aic8800d80/refs/heads/main/bazzite/aic8800d80.spec
~~~

Prepare and download required files.

~~~bash
spectool -g -R $HOME/rpmbuild/SPECS/aic8800d80.spec
rpmbuild -bs $HOME/rpmbuild/SPECS/aic8800d80.spec
~~~

Build SRPM package

~~~bash
rpmbuild --define "kver $(uname -r)" -bb $HOME/rpmbuild/SPECS/aic8800d80.spec
~~~

Install RPM package

~~~bash
rpm_path=$(find "$HOME/rpmbuild/RPMS/$(uname -m)" -maxdepth 1 -name 'aic8800d80-*.rpm' -printf '%T@ %p\n' | sort -nr | head -1 | cut -d' ' -f2-)
sudo rpm-ostree install "$rpm_path"
sudo systemctl reboot
~~~

After reboot, the Wi-Fi module and firmware loader will be available. On combo
adapters, Bluetooth is handled by the standard `btusb` kernel module after
firmware initialization. USB device `368b:8d81` also autoloads the packaged
`aic_zlp_quirk` companion module for its required Bluetooth ACL bulk TX zero
packet behavior; the distribution's `btusb` module is not replaced.

This RPM is built for the kernel reported by `uname -r`. Rebuild and reinstall
it after a Bazzite kernel upgrade.

