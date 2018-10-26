## Write plugin - run your code easily with FFdynamic
---------

### Dehaze Plugin

Suppose we have developed a dehaze algorightm with openCV dependency, and we would like to use it in a transcoding pipeline, namely, do deahze after video decode, then encode the dehazed stream and publish it.  

In FFmpeg, we would make dehaze as a filter, do according coding and makefile changing, then re-compile the FFmpeg.
In FFdynamic, we could write dehaze as a plugin and then link FFdynamic library without modify existing code.

#### FFdynamic implementation register and coding

There are two things needed:
1. coding dehaze implementation (inherits from FFdynamic's DavImpl class and implement according interface)
2. register this implementation

For **coding** part, it is just a make dehaze fit the data flow requirement of FFdynamic, please refer to the [source](ffdynaDehazor.cpp)

For **register**, we have two ways, register it to an existing component category as an implementation, or create an new component category and register it as implementation to the newly created category.
* method 1: register to existing one

``` c++
DavImplRegister g_dehazeReg(DavWaveClassVideoFilter(), vector<string>({"pluginDehazor"}),
                            [](const DavWaveOption & options) -> unique_ptr<DavImpl> {
                                     unique_ptr<PluginDehazor> p(new PluginDehazor(options));
                                     return p;
                            });
```
Here, we register 'PluginDehazor' as an implementation of already existing component 'VideoFilter';

* method 2: create new category and do register

``` c++
/* create a new dehaze component category */
struct DavWaveClassDehaze : public DavWaveClassCategory {
    DavWaveClassDehaze () :
        DavWaveClassCategory(type_index(typeid(*this)), type_index(typeid(std::string)), "Dehaze") {}
};

/* Then register PluginDehazor as default dehazor (by provides 'auto') */
DavImplRegister g_dehazeReg(DavWaveClassVideoDehaze(), vector<string>({"auto", "dehaze"}),
                            [](const DavWaveOption & options) -> unique_ptr<DavImpl> {
                                unique_ptr<PluginDehazor> p(new PluginDehazor(options));
                                return p;
                            });

```

#### Define an static option (used when create the component)

For options passing, there are also two ways.  
* method 1: define derived clsss from DavOption (normally for component level options, namely common to all implementations)

``` c++
struct DavOptionDehazeFogFactor : public DavOption {
    DavOptionDehazeFogFactor() :
        DavOption(type_index(typeid(*this)), type_index(typeid(double)), "DehazeFogFactor") {}
};
// Then set/get it like this:
DavWaveOption videoDehazeOption((DavWaveClassDehaze()));
videoDehazeOption.setDouble(DavOptionDehazeFogFactor(), 0.94);
double fogFactor = 0.94;
videoDehazeOption.getDouble(DavOptionDehazeFogFactor(), fogFactor);
```

* method 2: use 'AVDictionary' (just the FFmpeg's way, flexiable but not type checked, usually for implementation level options)

``` c++
       videoDehazeOption.setDouble("FogFactor", 0.94);
       // then in dehaze component, we could:
       double fogFactor;
       int ret = options.getDouble("FogFactor", fogFactor);
       if (ret < 0)
       .......
```

So, the rule of thumb:
* defines derived DavOption class for component level options; such as: DavOptionInputUrl for all demuxer
* use AVDictionary for implementation level options. 

#### Register an dynamic event

We could register dynamic event to change the parameters settings during running.  
This is an example that we register an run time event, change 'FogFactor', send according request would change Dehaze's FogFacotr during process.

``` c++
    std::function<int (const FogFactorChangeEvent &)> f =
        [this] (const FogFactorChangeEvent & e) {return processFogFactorUpdate(e);};
    m_implEvent.registerEvent(f);
```

#### Test dehaze with visualization

Here is the test for our newly ceated 'dehaze' plugin, we make the test with the following pattern: mix dehazed and original video frame in one screen, then encode it as one file. This allows use see the original and dehazed video at the same time.

```
Demux |-> Audio Decode -> |-> Audio Encode ------------------------------------------> |
      |                                                                                | -> Muxer
      |                   |-> Dehaze Filter -> |                                       |
      |-> Video Decode -> |                    | Mix original and dehazed ->| Encode ->|
                          | -----------------> |
```

The result is this (the dehaze effect is not good, but it is not the point).
![dehazed mix image](../asset/dehaze.gif)


You can refer to the source file [here](ffdynaDehazor.cpp). (NOTE: the dehazed algorightm itself (dehazor.cpp) is from github, but miss the link).

#### Run the dehazed example

``` 
    Under pluginExample folder: (need opencv libraries installed)
    mkdir build && cmake ../ && make
    ./testDehazor theInputFile
```

