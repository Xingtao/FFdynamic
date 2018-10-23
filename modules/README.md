## Modules - a set of extra useful components 
---------


* ### **faceDetect **
  *faceDetect* use opencv's face detector (which in turn load resnet_ssd) do face detection, which then emit a face roi event to encoder. Encoder will give more bitrate to this area.  
  Dependency: opencv > 3.4, using Yolo do object detect.   
  [running source](moduleCLI/focusFaceEncode.cpp)

- ### **yoloROI**
  'yoloROI' try find interested object, then emit a roi event to encoder, it is similar widh *faceDetect*, just have more interested objects.  
  Dependency: opencv > 3.4, using Yolo do object detect.   
  [running source](moduleCLI/focusYoloROIEncode.cpp)

