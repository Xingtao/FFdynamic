#include "cvDnnDetector.h"

namespace ff_dynamic {
/////////////////
// [Register - auto, dehaze]
static DavImplRegister s_cvDnnDetectReg(DavWaveClassCvDnnDetect(), vector<string>({"auto", "cvDnnDetect"}),
                                        [](const DavWaveOption & options) -> unique_ptr<DavImpl> {
                                            unique_ptr<YoloROI> p(new CvDnnDetect(options));
                                            return p;
                                        });
const DavRegisterProperties & CvDnnDetect::getRegisterProperties() const noexcept {
    return s_cvDnnDetectReg.m_properties;
}

////////////////////////////////////
//  [initialization]
int CvDnnDetect::onDynamicallyInitializeViaTravelStatic(DavProcCtx & ctx) {
    CHECK(m_inputTravelStatic.size() == ctx.m_froms.size() && ctx.m_froms.size() == 1);
    DavImplTravel::TravelStatic & in = m_inputTravelStatic.at(ctx.m_froms[0]);
    if (!in.m_codecpar && (in.m_pixfmt == AV_PIX_FMT_NONE)) {
        ERRORIT(DAV_ERROR_TRAVEL_STATIC_INVALID_CODECPAR,
                m_logtag + "dehaze cannot get valid codecpar or videopar");
        return DAV_ERROR_TRAVEL_STATIC_INVALID_CODECPAR;
    }

    /* Ok, get input parameters, for some implementations may need those paramters do init */
    m_net.reset(new Dehazor());
    /* dehaze's options: if get fail, will use default one */
    double fogFactor = 0.95;
    m_options.getDouble(DavOptionDehazeFogFactor(), fogFactor);
    m_net->setFogFactor(fogFactor);

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
int CvDnnDetect::processChangeConfidenceThreshold(const CvDynaEventChangeConfThreshold & e) {
    m_confidenceThreshold = e.m_confidenceThreshold;
    return 0;
}

////////////////////////////////////
//  [construct - destruct - process]
int CvDnnDetect::onConstruct() {
    LOG(INFO) << m_logtag << "Creating open CvDnnDetect " << m_options.dump();
    /* register events: also could put this one in dynamicallyInit part.
       Let's say we require dynamically change some dehaze's parameters, 'FogFactor':
       We can register an handler to process this.
    */
    std::function<int (const CvDynaEventChangeConfThreshold &)> f =
        [this] (const CvDynaEventChangeConfThreshold & e) {return processChangeConfidenceThreshold(e);};
    m_implEvent.registerEvent(f);

    = m_options.getDouble("thr", confThreshold);
    nmsThreshold = parser.get<float>("nms");
    float scale = parser.get<float>("scale");
    Scalar mean = parser.get<Scalar>("mean");
    bool swapRB = parser.get<bool>("rgb");
    int inpWidth = parser.get<int>("width");
    int inpHeight = parser.get<int>("height");

    /* there is no data output but event (detect results) */
    return 0;
}

int CvDnnDetect::onDestruct() {
    if (m_net) {
        m_net.reset();
    }
    INFOIT(0, m_logtag + "Plugin Dehazor Destruct");
    return 0;
}

/* here is the data process */
int CvDnnDetect::onProcess(DavProcCtx & ctx) {
    /* for most of the cases, the process just wants one input data to process;
       few compplicate implementations may require data from certain peer, then can setup this one */
    ctx.m_expect.m_expectOrder = {EDavExpect::eDavExpectAnyOne};
    if (!m_net)
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
    cv::Mat dehazedMat = m_net->process(bgrMat); /* do dehaze */
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

} // namespace ff_dynamic


# if 0 // obj detect . . . . . . .
const char* keys =
    "{ help  h     | | Print help message. }"
    "{ device      |  0 | camera device number. }"
    "{ input i     | | Path to input image or video file. Skip this argument to capture frames from a camera. }"
    "{ model m     | | Path to a binary file of model contains trained weights. "
                      "It could be a file with extensions .caffemodel (Caffe), "
                      ".pb (TensorFlow), .t7 or .net (Torch), .weights (Darknet).}"
    "{ config c    | | Path to a text file of model contains network configuration. "
                      "It could be a file with extensions .prototxt (Caffe), .pbtxt (TensorFlow), .cfg (Darknet).}"
    "{ framework f | | Optional name of an origin framework of the model. Detect it automatically if it does not set. }"
    "{ classes     | | Optional path to a text file with names of classes to label detected objects. }"
    "{ mean        | | Preprocess input image by subtracting mean values. Mean values should be in BGR order and delimited by spaces. }"
    "{ scale       |  1 | Preprocess input image by multiplying on a scale factor. }"
    "{ width       | -1 | Preprocess input image by resizing to a specific width. }"
    "{ height      | -1 | Preprocess input image by resizing to a specific height. }"
    "{ rgb         |    | Indicate that model works with RGB input images instead BGR ones. }"
    "{ thr         | .5 | Confidence threshold. }"
    "{ nms         | .4 | Non-maximum suppression threshold. }"
    "{ backend     |  0 | Choose one of computation backends: "
                         "0: automatically (by default), "
                         "1: Halide language (http://halide-lang.org/), "
                         "2: Intel's Deep Learning Inference Engine (https://software.intel.com/openvino-toolkit), "
                         "3: OpenCV implementation }"
    "{ target      | 0 | Choose one of target computation devices: "
                         "0: CPU target (by default), "
                         "1: OpenCL, "
                         "2: OpenCL fp16 (half-float precision), "
                         "3: VPU }";

message DynaDetectorSetting {
    string detector_type = 1;
    string detector_framework_tag = 2;
    string model_path = 3;
    string config_path = 4;
    string class_name_path = 5;
    int32 target_id = 10;
    float scale = 11;
    repeated double means;
    bool swap_rb = 13;
    int32 width = 14;
    int32 height = 15;
}

using namespace cv;
using namespace dnn;

float confThreshold, nmsThreshold;
std::vector<std::string> classes;

void postprocess(Mat& frame, const std::vector<Mat>& out, Net& net);

void drawPred(int classId, float conf, int left, int top, int right, int bottom, Mat& frame);

void callback(int pos, void* userdata);

std::vector<String> getOutputsNames(const Net& net);

int main(int argc, char** argv)
{
    CommandLineParser parser(argc, argv, keys);
    parser.about("Use this script to run object detection deep learning networks using OpenCV.");
    if (argc == 1 || parser.has("help"))
    {
        parser.printMessage();
        return 0;
    }

    confThreshold = parser.get<float>("thr");
    nmsThreshold = parser.get<float>("nms");
    float scale = parser.get<float>("scale");
    Scalar mean = parser.get<Scalar>("mean");
    bool swapRB = parser.get<bool>("rgb");
    int inpWidth = parser.get<int>("width");
    int inpHeight = parser.get<int>("height");

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

    // Load a model.
    CV_Assert(parser.has("model"));
    Net net = readNet(parser.get<String>("model"), parser.get<String>("config"), parser.get<String>("framework"));
    net.setPreferableBackend(parser.get<int>("backend"));
    net.setPreferableTarget(parser.get<int>("target"));

    // Create a window
    static const std::string kWinName = "Deep learning object detection in OpenCV";
    namedWindow(kWinName, WINDOW_NORMAL);
    int initialConf = (int)(confThreshold * 100);
    createTrackbar("Confidence threshold, %", kWinName, &initialConf, 99, callback);

    // Open a video file or an image file or a camera stream.
    VideoCapture cap;
    if (parser.has("input"))
        cap.open(parser.get<String>("input"));
    else
        cap.open(parser.get<int>("device"));

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

        // Create a 4D blob from a frame.
        Size inpSize(inpWidth > 0 ? inpWidth : frame.cols,
                     inpHeight > 0 ? inpHeight : frame.rows);
        blobFromImage(frame, blob, scale, inpSize, mean, swapRB, false);

        // Run a model.
        net.setInput(blob);
        if (net.getLayer(0)->outputNameToIndex("im_info") != -1)  // Faster-RCNN or R-FCN
        {
            resize(frame, frame, inpSize);
            Mat imInfo = (Mat_<float>(1, 3) << inpSize.height, inpSize.width, 1.6f);
            net.setInput(imInfo, "im_info");
        }
        std::vector<Mat> outs;
        net.forward(outs, getOutputsNames(net));

        postprocess(frame, outs, net);

        // Put efficiency information.
        std::vector<double> layersTimes;
        double freq = getTickFrequency() / 1000;
        double t = net.getPerfProfile(layersTimes) / freq;
        std::string label = format("Inference time: %.2f ms", t);
        putText(frame, label, Point(0, 15), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 255, 0));

        imshow(kWinName, frame);
    }
    return 0;
}

void postprocess(Mat& frame, const std::vector<Mat>& outs, Net& net)
{
    static std::vector<int> outLayers = net.getUnconnectedOutLayers();
    static std::string outLayerType = net.getLayer(outLayers[0])->type;

    std::vector<int> classIds;
    std::vector<float> confidences;
    std::vector<Rect> boxes;
    if (net.getLayer(0)->outputNameToIndex("im_info") != -1)  // Faster-RCNN or R-FCN
    {
        // Network produces output blob with a shape 1x1xNx7 where N is a number of
        // detections and an every detection is a vector of values
        // [batchId, classId, confidence, left, top, right, bottom]
        CV_Assert(outs.size() == 1);
        float* data = (float*)outs[0].data;
        for (size_t i = 0; i < outs[0].total(); i += 7)
        {
            float confidence = data[i + 2];
            if (confidence > confThreshold)
            {
                int left = (int)data[i + 3];
                int top = (int)data[i + 4];
                int right = (int)data[i + 5];
                int bottom = (int)data[i + 6];
                int width = right - left + 1;
                int height = bottom - top + 1;
                classIds.push_back((int)(data[i + 1]) - 1);  // Skip 0th background class id.
                boxes.push_back(Rect(left, top, width, height));
                confidences.push_back(confidence);
            }
        }
    }
    else if (outLayerType == "DetectionOutput")
    {
        // Network produces output blob with a shape 1x1xNx7 where N is a number of
        // detections and an every detection is a vector of values
        // [batchId, classId, confidence, left, top, right, bottom]
        CV_Assert(outs.size() == 1);
        float* data = (float*)outs[0].data;
        for (size_t i = 0; i < outs[0].total(); i += 7)
        {
            float confidence = data[i + 2];
            if (confidence > confThreshold)
            {
                int left = (int)(data[i + 3] * frame.cols);
                int top = (int)(data[i + 4] * frame.rows);
                int right = (int)(data[i + 5] * frame.cols);
                int bottom = (int)(data[i + 6] * frame.rows);
                int width = right - left + 1;
                int height = bottom - top + 1;
                classIds.push_back((int)(data[i + 1]) - 1);  // Skip 0th background class id.
                boxes.push_back(Rect(left, top, width, height));
                confidences.push_back(confidence);
            }
        }
    }
    else if (outLayerType == "Region")
    {
        for (size_t i = 0; i < outs.size(); ++i)
        {
            // Network produces output blob with a shape NxC where N is a number of
            // detected objects and C is a number of classes + 4 where the first 4
            // numbers are [center_x, center_y, width, height]
            float* data = (float*)outs[i].data;
            for (int j = 0; j < outs[i].rows; ++j, data += outs[i].cols)
            {
                Mat scores = outs[i].row(j).colRange(5, outs[i].cols);
                Point classIdPoint;
                double confidence;
                minMaxLoc(scores, 0, &confidence, 0, &classIdPoint);
                if (confidence > confThreshold)
                {
                    int centerX = (int)(data[0] * frame.cols);
                    int centerY = (int)(data[1] * frame.rows);
                    int width = (int)(data[2] * frame.cols);
                    int height = (int)(data[3] * frame.rows);
                    int left = centerX - width / 2;
                    int top = centerY - height / 2;

                    classIds.push_back(classIdPoint.x);
                    confidences.push_back((float)confidence);
                    boxes.push_back(Rect(left, top, width, height));
                }
            }
        }
    }
    else
        CV_Error(Error::StsNotImplemented, "Unknown output layer type: " + outLayerType);

    std::vector<int> indices;
    NMSBoxes(boxes, confidences, confThreshold, nmsThreshold, indices);
    for (size_t i = 0; i < indices.size(); ++i)
    {
        int idx = indices[i];
        Rect box = boxes[idx];
        drawPred(classIds[idx], confidences[idx], box.x, box.y,
                 box.x + box.width, box.y + box.height, frame);
    }
}

void drawPred(int classId, float conf, int left, int top, int right, int bottom, Mat& frame)
{
    rectangle(frame, Point(left, top), Point(right, bottom), Scalar(0, 255, 0));

    std::string label = format("%.2f", conf);
    if (!classes.empty())
    {
        CV_Assert(classId < (int)classes.size());
        label = classes[classId] + ": " + label;
    }

    int baseLine;
    Size labelSize = getTextSize(label, FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);

    top = max(top, labelSize.height);
    rectangle(frame, Point(left, top - labelSize.height),
              Point(left + labelSize.width, top + baseLine), Scalar::all(255), FILLED);
    putText(frame, label, Point(left, top), FONT_HERSHEY_SIMPLEX, 0.5, Scalar());
}

void callback(int pos, void*)
{
    confThreshold = pos * 0.01f;
}

std::vector<String> getOutputsNames(const Net& net)
{
    static std::vector<String> names;
    if (names.empty())
    {
        std::vector<int> outLayers = net.getUnconnectedOutLayers();
        std::vector<String> layersNames = net.getLayerNames();
        names.resize(outLayers.size());
        for (size_t i = 0; i < outLayers.size(); ++i)
            names[i] = layersNames[outLayers[i] - 1];
    }
    return names;
}
#endif
