#!/bin/bash

# darknet - yolo3-416 - coco
yolo3weight="yolov3.weights"
if [ -f "$yolo3weight" ]
then
    echo "$yolo3weight found."
else
    echo "downloading $yolo3weight ..."
    wget https://pjreddie.com/media/files/yolov3.weights
    wget https://raw.githubusercontent.com/pjreddie/darknet/master/cfg/yolov3.cfg
fi

# caffe vggnet ssd - coco

# tensorflow mobilenet ssd - coco

wget http://download.tensorflow.org/models/object_detection/ssd_mobilenet_v2_coco_2018_03_29.tar.gz

coconames="coco.names"
if [ -f "$coconames" ]
then
    echo "$coconames found."
else
    echo "downloading $coconames ..."
    wget https://raw.githubusercontent.com/pjreddie/darknet/master/data/coco.names
fi
