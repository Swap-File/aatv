# aatv building for Raspbian Buster Lite
```
git clone git://anongit.freedesktop.org/git/gstreamer/gst-plugins-good
cd gst-plugins-good
git checkout d88d1b0
```
Put files in /home/pi/gst-plugins-good/ext/aalib/
```
./autogen.sh --disable-gtk-doc
make
sudo cp /home/pi/gst-plugins-good/ext/aalib/.libs/libgstaasink.so /usr/lib/arm-linux-gnueabihf/gstreamer-1.0/libgstaasink.so
```
