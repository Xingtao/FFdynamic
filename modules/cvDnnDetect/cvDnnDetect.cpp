#include "ffdynaDehazor.h"
#include "fmtScale.h"

namespace ff_dynamic {

# if 0

#include <fstream>
#include <sstream>
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

const char* keys =
    "{ help  h     | | Print help message. }"
    "{ input i     | | Path to input image or video file. Skip this argument to capture frames from a camera.}"
    "{ model m     | | Path to a binary file of model contains trained weights. "
                      "It could be a file with extensions .caffemodel (Caffe), "
                      ".pb (TensorFlow), .t7 or .net (Torch), .weights (Darknet) }"
    "{ config c    | | Path to a text file of model contains network configuration. "
                      "It could be a file with extensions .prototxt (Caffe), .pbtxt (TensorFlow), .cfg (Darknet) }"
    "{ framework f | | Optional name of an origin framework of the model. Detect it automatically if it does not set. }"
    "{ classes     | | Optional path to a text file with names of classes. }"
    "{ mean        | | Preprocess input image by subtracting mean values. Mean values should be in BGR order and delimited by spaces. }"
    "{ scale       | 1 | Preprocess input image by multiplying on a scale factor. }"
    "{ width       |   | Preprocess input image by resizing to a specific width. }"
    "{ height      |   | Preprocess input image by resizing to a specific height. }"
    "{ rgb         |   | Indicate that model works with RGB input images instead BGR ones. }"
    "{ backend     | 0 | Choose one of computation backends: "
                        "0: automatically (by default), "
                        "1: Halide language (http://halide-lang.org/), "
                        "2: Intel's Deep Learning Inference Engine (https://software.intel.com/openvino-toolkit), "
                        "3: OpenCV implementation }"
    "{ target      | 0 | Choose one of target computation devices: "
                        "0: CPU target (by default), "
                        "1: OpenCL, "
                        "2: OpenCL fp16 (half-float precision), "
                        "3: VPU }";

using namespace cv;
using namespace dnn;

std::vector<std::string> classes;

int main(int argc, char** argv)
{
    CommandLineParser parser(argc, argv, keys);
    parser.about("Use this script to run classification deep learning networks using OpenCV.");
    if (argc == 1 || parser.has("help"))
    {
        parser.printMessage();
        return 0;
    }

    float scale = parser.get<float>("scale");
    Scalar mean = parser.get<Scalar>("mean");
    bool swapRB = parser.get<bool>("rgb");
    int inpWidth = parser.get<int>("width");
    int inpHeight = parser.get<int>("height");
    String model = parser.get<String>("model");
    String config = parser.get<String>("config");
    String framework = parser.get<String>("framework");
    int backendId = parser.get<int>("backend");
    int targetId = parser.get<int>("target");

    // Open file with classes names.
    if (parser.has("classes"))
    {
        std::string file = parser.get<String>("classes");
        std::ifstream ifs(file.c_str());
        if (!ifs.is_open())
            CV_Error(Error::StsError, "File " + file + " not found");
        std::string line;
        while (std::getline(ifs, line))
        {
            classes.push_back(line);
        }
    }

    if (!parser.check())
    {
        parser.printErrors();
        return 1;
    }
    CV_Assert(!model.empty());

    //! [Read and initialize network]
    Net net = readNet(model, config, framework);
    net.setPreferableBackend(backendId);
    net.setPreferableTarget(targetId);
    //! [Read and initialize network]

    // Create a window
    static const std::string kWinName = "Deep learning image classification in OpenCV";
    namedWindow(kWinName, WINDOW_NORMAL);

    //! [Open a video file or an image file or a camera stream]
    VideoCapture cap;
    if (parser.has("input"))
        cap.open(parser.get<String>("input"));
    else
        cap.open(0);
    //! [Open a video file or an image file or a camera stream]

    // Process frames.
    Mat frame, blob;
    while (waitKey(1) < 0)
    {
        cap >> frame;
        if (frame.empty())
        {
            waitKey();
            break;
        }

        //! [Create a 4D blob from a frame]
        blobFromImage(frame, blob, scale, Size(inpWidth, inpHeight), mean, swapRB, false);
        //! [Create a 4D blob from a frame]

        //! [Set input blob]
        net.setInput(blob);
        //! [Set input blob]
        //! [Make forward pass]
        Mat prob = net.forward();
        //! [Make forward pass]

        //! [Get a class with a highest score]
        Point classIdPoint;
        double confidence;
        minMaxLoc(prob.reshape(1, 1), 0, &confidence, 0, &classIdPoint);
        int classId = classIdPoint.x;
        //! [Get a class with a highest score]

        // Put efficiency information.
        std::vector<double> layersTimes;
        double freq = getTickFrequency() / 1000;
        double t = net.getPerfProfile(layersTimes) / freq;
        std::string label = format("Inference time: %.2f ms", t);
        putText(frame, label, Point(0, 15), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 255, 0));

        // Print predicted class.
        label = format("%s: %.4f", (classes.empty() ? format("Class #%d", classId).c_str() :
                                                      classes[classId].c_str()),
                                   confidence);
        putText(frame, label, Point(0, 40), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 255, 0));

        imshow(kWinName, frame);
    }
    return 0;
}
#end
////////////////////////////////////
//  [initialization]

int YoloROI::onDynamicallyInitializeViaTravelStatic(DavProcCtx & ctx) {
    CHECK(m_inputTravelStatic.size() == ctx.m_froms.size() && ctx.m_froms.size() == 1);
    DavImplTravel::TravelStatic & in = m_inputTravelStatic.at(ctx.m_froms[0]);
    if (!in.m_codecpar && (in.m_pixfmt == AV_PIX_FMT_NONE)) {
        ERRORIT(DAV_ERROR_TRAVEL_STATIC_INVALID_CODECPAR,
                m_logtag + "dehaze cannot get valid codecpar or videopar");
        return DAV_ERROR_TRAVEL_STATIC_INVALID_CODECPAR;
    }

    /* Ok, get input parameters, for some implementations may need those paramters do init */
    m_dehazor.reset(new Dehazor());
    /* dehaze's options: if get fail, will use default one */
    double fogFactor = 0.95;
    m_options.getDouble(DavOptionDehazeFogFactor(), fogFactor);
    m_dehazor->setFogFactor(fogFactor);

    /* set output infos */
    m_timestampMgr.clear();
    m_outputTravelStatic.clear();
    DavImplTravel::TravelStatic out(in); /* use the same timebase with the input */
    LOG(INFO) << m_logtag << "travel static input/output for dehaze is the same: " << in;
    m_timestampMgr.insert(std::make_pair(ctx.m_froms[0], DavImplTimestamp(in.m_timebase, in.m_timebase)));
    /* output only one stream: dehazed video stream */
    m_outputTravelStatic.insert(std::make_pair(IMPL_SINGLE_OUTPUT_STREAM_INDEX, out));

    /* ok, no other works needed */
    /* must set this one after init done */
    m_bDynamicallyInitialized = true;
    LOG(INFO) << m_logtag << "dynamically create dehazor done.\nin static: " << in << ", \nout: " << out;
    return 0;
}

////////////////////////////////////
//  [event process]
int YoloROI::processFogFactorUpdate(const FogFactorChangeEvent & e) {
    /* event process is in the same thread with data process thread, so no lock need here */
    if (m_dehazor)
        m_dehazor->setFogFactor(e.m_newFogFactor);
    return 0;
}
////////////////////////////////////
//  [construct - destruct - process]
int YoloROI::onConstruct() {
    /* construct phrase could do nothing, and put all iniitialization work in onDynamicallyInitializeViaTravelStatic,
       for some implementations rely on input peer's parameters to do the iniitialization */
    LOG(INFO) << m_logtag << "will open YoloROI after first frame " << m_options.dump();

    /* register events: also could put this one in dynamicallyInit part.
       Let's say we require dynamically change some dehaze's parameters, 'FogFactor':
       We can register an handler to process this.
     */
    std::function<int (const FogFactorChangeEvent &)> f =
        [this] (const FogFactorChangeEvent & e) {return processFogFactorUpdate(e);};
    m_implEvent.registerEvent(f);

    /* tells we only output one video stream */
    m_outputMediaMap.insert(std::make_pair(IMPL_SINGLE_OUTPUT_STREAM_INDEX, AVMEDIA_TYPE_VIDEO));
    return 0;
}

int YoloROI::onDestruct() {
    if (m_dehazor) {
        m_dehazor.reset();
    }
    INFOIT(0, m_logtag + "Plugin Dehazor Destruct");
    return 0;
}

/* here is the data process */
int YoloROI::onProcess(DavProcCtx & ctx) {
    /* for most of the cases, the process just wants one input data to process;
       few compplicate implementations may require data from certain peer, then can setup this one */
    ctx.m_expect.m_expectOrder = {EDavExpect::eDavExpectAnyOne};
    if (!m_dehazor)
        return 0;
    if (!ctx.m_inBuf) {
        ERRORIT(DAV_ERROR_IMPL_UNEXPECT_EMPTY_INBUF, "video dehaze should always have input");
        return DAV_ERROR_IMPL_UNEXPECT_EMPTY_INBUF;
    }

    int ret = 0;
    /* ref frame is a frame ref to original frame (data shared),
       but timestamp is convert to current impl's timebase */
    auto inFrame = ctx.m_inRefFrame;
    if (!inFrame) {
        LOG(INFO) << m_logtag << "video dehaze reciving flush frame";
        ctx.m_bInputFlush = true;
        /* no implementation flush needed, so just return EOF */
        return AVERROR_EOF;
    }
    /* const DavProcFrom & from = ctx.m_inBuf->getAddress(); */

    // convert this frame to opencv Mat
    CHECK((enum AVPixelFormat)inFrame->format == AV_PIX_FMT_YUV420P);
    cv::Mat yuvMat;
    yuvMat.create(inFrame->height * 3 / 2, inFrame->width, CV_8UC1);
    for (int k=0; k < inFrame->height; k++)
        memcpy(yuvMat.data + k * inFrame->width, inFrame->data[0] + k * inFrame->linesize[0], inFrame->width);
    const auto u = yuvMat.data + inFrame->width * inFrame->height;
    const auto v = yuvMat.data + inFrame->width * inFrame->height * 5 / 4 ;
    for (int k=0; k < inFrame->height/2; k++) {
        memcpy(u + k * inFrame->width/2, inFrame->data[1] + k * inFrame->linesize[1], inFrame->width/2);
        memcpy(v + k * inFrame->width/2, inFrame->data[2] + k * inFrame->linesize[2], inFrame->width/2);
    }

    cv::Mat bgrMat;
    cv::cvtColor(yuvMat, bgrMat, CV_YUV2BGR_I420);
    cv::Mat dehazedMat = m_dehazor->process(bgrMat); /* do dehaze */
    cv::cvtColor(dehazedMat, yuvMat, CV_BGR2YUV_I420);

    /* then convert back to YUV420p (because our output travel static says so);
       if we state it is AV_PIX_FMT_BGR24, then no convertion needed here. */
    auto outBuf = make_shared<DavProcBuf>();
    auto outFrame = outBuf->mkAVFrame();
    outFrame->width = inFrame->width;
    outFrame->height = inFrame->height;
    outFrame->format = inFrame->format;
    av_frame_get_buffer(outFrame, 16);
    for (int k=0; k < outFrame->height; k++)
        memcpy(outFrame->data[0] + k * outFrame->linesize[0], yuvMat.data + k * outFrame->width, outFrame->width);
    for (int k=0; k < outFrame->height/2; k++) {
        memcpy(outFrame->data[1] + k * outFrame->linesize[1], u + k * outFrame->width/2, outFrame->width/2);
        memcpy(outFrame->data[2] + k * outFrame->linesize[2], v + k * outFrame->width/2, outFrame->width/2);
    }

    /* prepare output */
    outFrame->pts = inFrame->pts;
    outBuf->m_travel.m_static = m_outputTravelStatic.at(IMPL_SINGLE_OUTPUT_STREAM_INDEX);
    ctx.m_outBufs.push_back(outBuf);
    return 0;
}

///////////////////////////////////////
// [Register - auto, dehaze]: this will create YoloROI instance for dehaze
DavImplRegister g_dehazeReg(DavWaveClassDehaze(), vector<string>({"auto", "dehaze"}),
                            [](const DavWaveOption & options) -> unique_ptr<DavImpl> {
                                unique_ptr<YoloROI> p(new YoloROI(options));
                                return p;
                            });

/* Register Explaination */
/* If we have another dehaze implementation, let's say DehazorImpl2, we could register it as:
DavImplRegister g_regDehazeP2(DavWaveClassVideoDehaze(), vector<string>({"dehazeImpl2"}),
                             [](const DavWaveOption & options) -> unique_ptr<DavImpl> {
                                 unique_ptr<DehazorImpl2> p(new DehazorImpl2(options));
                                 return p;
                             });
   And, we could select them by set options:
       options..setDavWaveCategory((DavWaveClassDehaze()));
       options.set(EDavWaveOption::eImplType, "dehazeImpl2");
  */

} // namespace ff_dynamic
