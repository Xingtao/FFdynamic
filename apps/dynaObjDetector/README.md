##  A play case - Change Object Detectors at run time with live streams
---------

This project provides a play groud one can change object detector types at run time while reading live streams (also local files). Those detectors are opencv supported one, yolo, ssd, face detector, etc.. or your own ones, which needs write a plugin wrapper. Here is an output stream gif, which run 2 detecors in parallle, draw boxes and texts when they find objects intereted.

### Chnage detectors at run time

We can *add a detector* with a curl request while program is running:

``` shell
    curl -v http://ip:port/api1/detectors/add -d 
    
    {
        "detector_module" : "opencv", /* only opencv for now */
        "detector_type" : "yolo3",
        "post_action" : ["draw_box", "draw_text"]
    }
```

We can *add a detector* with a curl request while program is running:

``` shell
    curl -v http://ip:port/api1/detectors/delete -d 
    
    {
        "detector_module" : "opencv", /* only opencv for now */
        "detector_type" : "yolo3"
    }
```

### Program Running pattern

```
VideoStreamDemux |-> AudioDecode -> |-> AudioEncode ------------------------------------> |
                 |                                                                        |
                 |                  |-> ObjDector 1 -> |    | Detector |                  |
                 |-> VideoDecode -> |-> ObjDector 2 -> | -> | Post     | -> VideoEncode ->| -> Mux stream out
                                    |-> ObjDector 3 -> |    | Action   |                  |
                                        ..........
```

### Run the program

``` 
    under build folder:  ./dynaObjDetect dynaObjDetect.json inputStream outputStream
```

### Build the program

Depends on OpenCV (3.4.0 or above), glog, protobuf3.

For mac: 

