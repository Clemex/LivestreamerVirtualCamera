# Livestreamer Virtual Camera

## Depends on:
 * libzmq 4.0.4 http://zeromq.org/distro:microsoft-windows
 * opencv 3.1.0 http://opencv.org/releases.html

## Register the project:
 * cd into \<project root\>/x64/Debug
 * ensure opencv_world310.dll and libzmq-v120-mt-gd-4_0_4.dll are in the folder
 * regsvr32 LivestreamerVirtualCamera

## Run the virtual camera
 * restart Chrome if it was already open during the installation
 * go to chrome://settings/content/camera and ensure "Livestreamer Virtual Camera" is allowed on the site
 ![image](https://user-images.githubusercontent.com/64457/27405495-c1bc7bea-569f-11e7-8470-73bfa3c9b83d.png)
 * start the Livestreamer
 * navigate to a site to test it
 ![image](https://user-images.githubusercontent.com/64457/27405590-1ede4434-56a0-11e7-9c30-bcce99fb4edb.png)
