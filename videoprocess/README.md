# Video Processing feature test suites


## Introduction

This folder collect a set of VP examples to exercise VA-API in
accordance with the libva project. 
vaapp is the basic app which you can use to check several features(CSC,scaling,
denoise, sharpness,DeInterlace,skin tone enhancement,HSBC).
The other sample app is the specific app just for one or two features tested.
You can get the info from the app name, such as CSC,scaling, denoise, sharpness,
chromasitting, 1:N output, usrptr, etc.

## Building

1. In upper folder run autogen.sh, will produce the Makefile.
```
$ ./autogen.sh
```
2. In upper folder, make the sample.
```
$ make
```
3. The VP related sample will be produced in the videoprocess directory

## How to run the sample

1. The app para should be defined in the corresponding *.cfg file. Each app will have
one related *.cfg.template for your reference,and the detailed para meaning is described in
the file. you can create your *.cfg according to your usage.

2. Copy the app and cfg file to the target machine and run.
```
$ ./vavpp process.cfg
