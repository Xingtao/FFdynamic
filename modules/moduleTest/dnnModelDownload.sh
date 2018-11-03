#!/bin/bash

# yolo3 - 416
yolo3weight="yolov3.weights"
if [ -f "$yolo3weight" ]
then
    echo "$yolo3weight found."
else
    echo "downloading $yolo3weight ..."
    wget https://pjreddie.com/media/files/yolov3.weights
    wget https://raw.githubusercontent.com/pjreddie/darknet/master/cfg/yolov3.cfg
fi

coconames="coco.names"
if [ -f "$coconames" ]
then
    echo "$coconames found."
else
    echo "downloading $coconames ..."
    wget https://raw.githubusercontent.com/pjreddie/darknet/master/data/coco.names
fi

# ssd

# vgg16
