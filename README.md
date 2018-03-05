# moodbar
An upload of moodbar sources (https://userbase.kde.org/Amarok/Manual/Various/Moodbar) and port to gstreamer-1.0
moodbar-0.1.2.tar.gz acquired following https://aur.archlinux.org/cgit/aur.git/tree/PKGBUILD?h=moodbar from 
http://pkgs.fedoraproject.org/repo/pkgs/moodbar/moodbar-0.1.2.tar.gz/28c8eb65e83b30f71b84be4fab949360/moodbar-0.1.2.tar.gz 
and .tar.gz sha256sum confirmed to be identical to one listed in https://aur.archlinux.org/cgit/aur.git/tree/PKGBUILD?h=moodbar
(3d53627c3d979636e98bbe1e745ed79e98f1238148ba4f8379819f9083b3d9c4) before extraction and upload

### Documentation
Most of the documentation regarding installation, usage and troubleshooting at https://userbase.kde.org/Amarok/Manual/Various/Moodbar should still apply, just with any instances of gstreamer-0.10 replaced with gstreamer-1.0.
The basic moodfile generation functionality can be tested with `moodbar -o test.mood [audiofile]`, and an image file can be generated for example by command
`gst-launch-1.0 filesrc location=[audiofile] ! decodebin ! audioconvert ! fftwspectrum ! moodbar height=50 max-width=300 ! pngenc ! filesink location=mood.png`

For actual usage with complete music libraries, the Moodbar File Generation Script ( available on the userbase page) or similar is recommended.

### Installation

Compiling and installing from a github checkout (or a .tar.gz) should work with command series:

``./autogen.sh  --prefix=`pkg-config --variable=prefix gstreamer-1.0` ``

`make`

`sudo make install`
