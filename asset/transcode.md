## **Getting start with simple application Transcoding** 

We start with a simple scenario, transcoding, to take a flavor of FFdynamic.

### **Full feature Transcoding in a dozen lines of code**

```c++
   /* create demux, decode, encode, mux options */
    DavWaveOption demuxOption((DavWaveClassDemux()));
    demuxOption.set(DavOptionInputUrl(), argv[1]);

    DavWaveOption videoDecodeOption((DavWaveClassVideoDecode()));
    DavWaveOption audioDecodeOption((DavWaveClassAudioDecode()));

    DavWaveOption videoEncodeOption((DavWaveClassVideoEncode()));
    videoEncodeOption.setVideoSize(1280, 720);
    videoEncodeOption.setAVRational("framerate", {30000, 1001});
    DavWaveOption audioEncodeOption((DavWaveClassAudioEncode()));

    DavWaveOption muxOption((DavWaveClassMux()));
    muxOption.set(DavOptionOutputUrl(), "test-transcode.flv");

    /* build instances */
    DavDefaultInputStreamletBuilder inputBuilder;
    DavDefaultOutputStreamletBuilder outputBuilder;
    DavStreamletOption inputOption;
    inputOption.setInt(DavOptionBufLimitNum(), 20);
    auto streamletInput = inputBuilder.build({demuxOption, videoDecodeOption, audioDecodeOption},
                                             DavDefaultInputStreamletTag("test_input"), inputOption);
    auto streamletOutput = outputBuilder.build({videoEncodeOption, audioEncodeOption, muxOption},
                                               DavDefaultOutputStreamletTag("test_output"));
    CHECK(streamletInput != nullptr && streamletOutput != nullptr);

    /* connect streamlets and start */
    streamletInput >> streamletOutput;
    DavRiver river({streamletInput, streamletOutput});
    river.start();
    ......
```
The source code is [here](FFdynamic/davTests/simpleTranscode.cpp).

This example read an input (local or online stream) then encode it to the format you required (it is just what  transcoding does).

First, we create components by create its option:
``` c++
    DavWaveOption demuxOption((DavWaveClassDemux()));
    demuxOption.set(DavOptionInputUrl, argv[1]);
```
This will create a demux option, and set input url. If we choose FFmpeg's demuxer (also other libav* components, encode, decode, fitler, muxer), we set other demuxer specific options through AVDictionary just the same as FFmpeg, for example, 

```  c++
    demuxOption.set("probesize", "2000000");   // this option will pass to libavformat via AVDictionary
```

Then, no surprising, we create audio/video decode, audio/video encode, and muxer. Each components set desired options.
At this point, no real component is really created, just their options.

``` c++
    DavDefaultInputStreamletBuilder inputBuilder;
    DavStreamletOption inputOption;
    inputOption.setInt(DavOptionBufLimitNum, 20);
    auto streamletInput = inputBuilder.build({demuxOption, videoDecodeOption, audioDecodeOption},
                                             DavDefaultInputStreamletTag("test_input"), inputOption);
```

Here, we use a predefined input streamlet builder to build all components. A streamlet is a set of components, with convinient components' manage functionalities (start, stop, pause, etc.. and input/output data interface).

After build input/output streamlet, connect them by:

``` c++
    streamletInput >> streamletOutput;
```

Finally, we start do the transcoding job by:
``` c++
    DavRiver river({streamletInput, streamletOutput});
    river.start();
```
A *river* is just like a *streamlet* to 'DavWave' component, that it manages streamlet's state.

That is all for transcoding. If one wants more control over the process, dynamically change output bitrate, add outputs streams etc.., you can refer to 'Interactive Live' application. But before that, let's tatke a little bit more about 'Transcoding'.

### **Transcoding in parallel**

We can modify above code a little bit to do multiple resolutions and bitrates output as follow:

``` c++
    DavWaveOption videoEncodeOption1((DavWaveClassVideoEncode()));
    videoEncodeOption.setVideoSize(1280, 720);
    videoEncodeOption.setAVRational("framerate", {30000, 1001});
    DavWaveOption audioEncodeOption1((DavWaveClassAudioEncode()));
    DavWaveOption muxOption1((DavWaveClassMux()));
    muxOption.set(DavOptionOutputUrl, "test-transcode.flv");

    DavWaveOption videoEncodeOption2((DavWaveClassVideoEncode()));
    videoEncodeOption.setVideoSize(1920, 1080);
    videoEncodeOption.set('b', "6000k");
    DavWaveOption audioEncodeOption2((DavWaveClassAudioEncode()));
    DavWaveOption muxOption2((DavWaveClassMux()));
    muxOption.set(DavOptionOutputUrl, "test-transcode-1080p.mp4");

    ////////////////////////////////////////////////////////////////////////////
    DavDefaultOutputStreamletBuilder outputBuilder1;
    DavDefaultOutputStreamletBuilder outputBuilder2;
    auto streamletOutput1 = outputBuilder.build({videoEncodeOption1, audioEncodeOption1, muxOption1},
                                               DavDefaultOutputStreamletTag("test_output_1"));
    auto streamletOutput2 = outputBuilder.build({videoEncodeOption2, audioEncodeOption2, muxOption2},
                                               DavDefaultOutputStreamletTag("test_output_2"));
    CHECK(streamletOutput1 != nullptr && streamletOutput2 != nullptr);
    /* connect streamlets */
    streamletInput >> streamletOutput1;
    streamletInput >> streamletOutput2;

    DavRiver river({streamletInput, streamletOutput1, streamletOutput2});
    river.start();
```
Above code shows one input and two outputs case (useful for adaptive bitrate transcoding), and the code size is still samll. It scales 'linear'.
For performance, each component runs on its own thread, so the whole task is running in parallel.  
On the contrary, FFmpeg do multiple output transcoding in parial parallel:

``` c  FFmpeg.c
static int reap_filters(int flush)
{
    AVFrame *filtered_frame = NULL;
    int i;

    /* Reap all buffers present in the buffer sinks */
    for (i = 0; i < nb_output_streams; i++) {
        OutputStream *ost = output_streams[i];
        OutputFile    *of = output_files[ost->file_index];
        int ret = 0;
        ......
        while (1) {
            double float_pts = AV_NOPTS_VALUE; // this is identical to filtered_frame.pts but with higher precision
            ret = av_buffersink_get_frame_flags(filter, filtered_frame,
                                               AV_BUFFERSINK_FLAG_NO_REQUEST);
            ......
            ......
            switch (av_buffersink_get_type(filter)) {
            case AVMEDIA_TYPE_VIDEO:
                .......
                do_video_out(of, ost, filtered_frame, float_pts);
                break;
```

As shown, if we have 4 outputs (which is normal in live broadcast field, output 1080p30, 720p, 540p, 320p for diffrent devices), FFmpeg will do encode one by one (takes more time, cpu not fully used). Of cause, this is because FFmpeg not targeting this scenario.
