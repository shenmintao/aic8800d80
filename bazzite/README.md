# Intro

Hello.

Some instructions how to build rpm file and then install rpm file on Bazzite.

I saw video on Youtube about Asrock AMD BC-250, then I bought this motherboard.

On printables I found a case and recomendations about WIFI BT stick.

There is only one problem - Bazzite has read-only filesystem. 

# How to build from spec file.


Install required packages and then reboot after install to apply changes.

~~~bash
sudo rpm-ostree install rpm-build rpmdevtools
sudo systemctl reboot
~~~

Prepare rpmbuild folder

~~~bash
cd $HOME
rpmdev-setuptree
~~~

Copy aic8800d80.spec to rpmbuild/SPECS

~~~bash
cp aic8800d80.spec $HOME/rpmbuild/SPECS
~~~

Prepare and download required files.

~~~bash
spectool -g -R $HOME/rpmbuild/SPECS/aic8800d80.spec
rpmbuild -bs $HOME/rpmbuild/SPECS/aic8800d80.spec
~~~

Enable user overlay

~~~bash
sudo rpm-ostree usroverlay
~~~


Build SRPM package

~~~bash
rpmbuild --define "uname $(uname -r)" -bb $HOME/rpmbuild/SPECS/aic8800d80.spec
~~~

Install RPM package

~~~bash
sudo rpm-ostree install $HOME/rpmbuild/RPMS/x86_64/aic8800d80-b0787d9-4.fc43.x86_64.rpm
sudo systemctl reboot
~~~

After reboot wifi module will load automatically.

