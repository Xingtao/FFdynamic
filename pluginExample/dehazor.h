#pragma once

#include <string>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>

namespace dehaze {
using ::std::string;

class GuidedFilterImpl;
class GuidedFilter {
public:
    GuidedFilter(const cv::Mat &I, int r, double eps);
    ~GuidedFilter();
    cv::Mat filter(const cv::Mat &p, int depth = -1) const;
private:
    GuidedFilterImpl *impl_;
};
cv::Mat guidedFilter(const cv::Mat &I, const cv::Mat &p, int r, double eps, int depth = -1);

class Dehazor {
public:
    Dehazor() = default;
    ~Dehazor() = default;
    cv::Mat process(cv::Mat & image);

public:
    inline void setWindowsize(int px) noexcept {
        if (px<0)
            px = 0;
        windowsize = px;
    }

    inline void setFogFactor(double factor) noexcept {
        if (factor<0)
            factor=0;
        fog_reservation_factor= factor;
    }
    inline void setLocalWindowsize(int lpx) noexcept {
        if (lpx<0)
            lpx=0;
        localwindowsize= lpx;
    }
    inline void setEpsilon(float epsilon) noexcept {
        if (epsilon<0)
            epsilon=0;
        eps= epsilon;
    }
    inline cv::Mat & getRawMap() noexcept {return rawImage;}
    inline cv::Mat & getRefinedMap() noexcept {return refinedImage;}

private:
    // window size -General parameter
    int windowsize = 15;
    // fog reservation factor-General parameter
    double fog_reservation_factor = 0.95;
    // local window size -guided filter parameter
    int localwindowsize = 20;
    // regularization eps filter parameter
    float eps = 0.001;

    // output image
    cv::Mat output;
    // raw transmission map
    cv::Mat rawImage;
    // refined transmission map
    cv::Mat refinedImage;

private:
    cv::Mat boxfilter(cv::Mat & im, int r);
    cv::Mat guildedfilter_color(const cv::Mat & Img, cv::Mat & p, int r, float & epsi);
};

} // namespace dehaze
