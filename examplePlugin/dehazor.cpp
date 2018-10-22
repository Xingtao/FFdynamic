#include "dehazor.h"

namespace dehaze {

//////////////////////////////////////////////////
// guide filter
static cv::Mat boxfilter(const cv::Mat &I, int r) {
    cv::Mat result;
    cv::blur(I, result, cv::Size(r, r));
    return result;
}

static cv::Mat convertTo(const cv::Mat &mat, int depth) {
    if (mat.depth() == depth)
        return mat;
    cv::Mat result;
    mat.convertTo(result, depth);
    return result;
}

class GuidedFilterImpl {
public:
    virtual ~GuidedFilterImpl() {}
    cv::Mat filter(const cv::Mat &p, int depth);
protected:
    int Idepth;
private:
    virtual cv::Mat filterSingleChannel(const cv::Mat &p) const = 0;
};

class GuidedFilterMono : public GuidedFilterImpl {
public:
    GuidedFilterMono(const cv::Mat &I, int r, double eps);
private:
    virtual cv::Mat filterSingleChannel(const cv::Mat &p) const;
private:
    int r;
    double eps;
    cv::Mat I, mean_I, var_I;
};

class GuidedFilterColor : public GuidedFilterImpl {
public:
    GuidedFilterColor(const cv::Mat &I, int r, double eps);
private:
    virtual cv::Mat filterSingleChannel(const cv::Mat &p) const;
private:
    std::vector<cv::Mat> Ichannels;
    int r;
    double eps;
    cv::Mat mean_I_r, mean_I_g, mean_I_b;
    cv::Mat invrr, invrg, invrb, invgg, invgb, invbb;
};

cv::Mat GuidedFilterImpl::filter(const cv::Mat &p, int depth) {
    cv::Mat p2 = convertTo(p, Idepth);
    cv::Mat result;
    if (p.channels() == 1) {
        result = filterSingleChannel(p2);
    } else {
        std::vector<cv::Mat> pc;
        cv::split(p2, pc);
        for (std::size_t i = 0; i < pc.size(); ++i)
            pc[i] = filterSingleChannel(pc[i]);
        cv::merge(pc, result);
    }
    return convertTo(result, depth == -1 ? p.depth() : depth);
}

GuidedFilterMono::
GuidedFilterMono(const cv::Mat &origI, int r, double eps) : r(r), eps(eps) {
    if (origI.depth() == CV_32F || origI.depth() == CV_64F)
        I = origI.clone();
    else
        I = convertTo(origI, CV_32F);
    Idepth = I.depth();
    mean_I = boxfilter(I, r);
    cv::Mat mean_II = boxfilter(I.mul(I), r);
    var_I = mean_II - mean_I.mul(mean_I);
}

cv::Mat GuidedFilterMono::filterSingleChannel(const cv::Mat &p) const {
    cv::Mat mean_p = boxfilter(p, r);
    cv::Mat mean_Ip = boxfilter(I.mul(p), r);
    // this is the covariance of (I, p) in each local patch.
    cv::Mat cov_Ip = mean_Ip - mean_I.mul(mean_p); 
    cv::Mat a = cov_Ip / (var_I + eps); // Eqn. (5) in the paper;
    cv::Mat b = mean_p - a.mul(mean_I); // Eqn. (6) in the paper;
    cv::Mat mean_a = boxfilter(a, r);
    cv::Mat mean_b = boxfilter(b, r);
    return mean_a.mul(I) + mean_b;
}

GuidedFilterColor::
GuidedFilterColor(const cv::Mat &origI, int r, double eps) : r(r), eps(eps) {
    cv::Mat I;
    if (origI.depth() == CV_32F || origI.depth() == CV_64F)
        I = origI.clone();
    else
        I = convertTo(origI, CV_32F);
    Idepth = I.depth();
    cv::split(I, Ichannels);
    mean_I_r = boxfilter(Ichannels[0], r);
    mean_I_g = boxfilter(Ichannels[1], r);
    mean_I_b = boxfilter(Ichannels[2], r);
    // variance of I in each local patch: the matrix Sigma in Eqn (14).
    // Note the variance in each local patch is a 3x3 symmetric matrix:
    //           rr, rg, rb
    //   Sigma = rg, gg, gb
    //           rb, gb, bb
    cv::Mat var_I_rr = boxfilter(Ichannels[0].mul(Ichannels[0]), r) - mean_I_r.mul(mean_I_r) + eps;
    cv::Mat var_I_rg = boxfilter(Ichannels[0].mul(Ichannels[1]), r) - mean_I_r.mul(mean_I_g);
    cv::Mat var_I_rb = boxfilter(Ichannels[0].mul(Ichannels[2]), r) - mean_I_r.mul(mean_I_b);
    cv::Mat var_I_gg = boxfilter(Ichannels[1].mul(Ichannels[1]), r) - mean_I_g.mul(mean_I_g) + eps;
    cv::Mat var_I_gb = boxfilter(Ichannels[1].mul(Ichannels[2]), r) - mean_I_g.mul(mean_I_b);
    cv::Mat var_I_bb = boxfilter(Ichannels[2].mul(Ichannels[2]), r) - mean_I_b.mul(mean_I_b) + eps;

    // Inverse of Sigma + eps * I
    invrr = var_I_gg.mul(var_I_bb) - var_I_gb.mul(var_I_gb);
    invrg = var_I_gb.mul(var_I_rb) - var_I_rg.mul(var_I_bb);
    invrb = var_I_rg.mul(var_I_gb) - var_I_gg.mul(var_I_rb);
    invgg = var_I_rr.mul(var_I_bb) - var_I_rb.mul(var_I_rb);
    invgb = var_I_rb.mul(var_I_rg) - var_I_rr.mul(var_I_gb);
    invbb = var_I_rr.mul(var_I_gg) - var_I_rg.mul(var_I_rg);

    cv::Mat covDet = invrr.mul(var_I_rr) + invrg.mul(var_I_rg) + invrb.mul(var_I_rb);

    invrr /= covDet;
    invrg /= covDet;
    invrb /= covDet;
    invgg /= covDet;
    invgb /= covDet;
    invbb /= covDet;
}

cv::Mat GuidedFilterColor::filterSingleChannel(const cv::Mat &p) const {
    cv::Mat mean_p = boxfilter(p, r);

    cv::Mat mean_Ip_r = boxfilter(Ichannels[0].mul(p), r);
    cv::Mat mean_Ip_g = boxfilter(Ichannels[1].mul(p), r);
    cv::Mat mean_Ip_b = boxfilter(Ichannels[2].mul(p), r);

    // covariance of (I, p) in each local patch.
    cv::Mat cov_Ip_r = mean_Ip_r - mean_I_r.mul(mean_p);
    cv::Mat cov_Ip_g = mean_Ip_g - mean_I_g.mul(mean_p);
    cv::Mat cov_Ip_b = mean_Ip_b - mean_I_b.mul(mean_p);

    cv::Mat a_r = invrr.mul(cov_Ip_r) + invrg.mul(cov_Ip_g) + invrb.mul(cov_Ip_b);
    cv::Mat a_g = invrg.mul(cov_Ip_r) + invgg.mul(cov_Ip_g) + invgb.mul(cov_Ip_b);
    cv::Mat a_b = invrb.mul(cov_Ip_r) + invgb.mul(cov_Ip_g) + invbb.mul(cov_Ip_b);

    cv::Mat b = mean_p - a_r.mul(mean_I_r) - a_g.mul(mean_I_g) - a_b.mul(mean_I_b); // Eqn. (15) in the paper;

    return (boxfilter(a_r, r).mul(Ichannels[0])
          + boxfilter(a_g, r).mul(Ichannels[1])
          + boxfilter(a_b, r).mul(Ichannels[2])
          + boxfilter(b, r));  // Eqn. (16) in the paper;
}

GuidedFilter::GuidedFilter(const cv::Mat &I, int r, double eps) {
    CV_Assert(I.channels() == 1 || I.channels() == 3);
    if (I.channels() == 1)
        impl_ = new GuidedFilterMono(I, 2 * r + 1, eps);
    else
        impl_ = new GuidedFilterColor(I, 2 * r + 1, eps);
}
GuidedFilter::~GuidedFilter() {
    delete impl_;
}
cv::Mat GuidedFilter::filter(const cv::Mat &p, int depth) const {
    return impl_->filter(p, depth);
}
cv::Mat guidedFilter(const cv::Mat &I, const cv::Mat &p, int r, double eps, int depth)
{
    return GuidedFilter(I, r, eps).filter(p, depth);
}

/////////////////////////////////////////////////
/////////// Dehaze
cv::Mat Dehazor::process(cv::Mat & image) {
    int dimr = image.rows;
    int dimc = image.cols;

    cv::Mat rawtemp;
    cv::Mat refinedImage_temp;
    cv::Mat output_b_temp(image.rows, image.cols,CV_32F);
    cv::Mat output_g_temp(image.rows, image.cols,CV_32F);
    cv::Mat output_r_temp(image.rows, image.cols,CV_32F);
    cv::Mat output_temp(dimr, dimc, image.type());
    rawImage.create(image.rows, image.cols, CV_8U);
    refinedImage_temp.create(image.rows, image.cols, CV_32F);
    rawtemp.create(image.rows, image.cols, CV_8U);

    float sumb=0;float sumg=0;float sumr=0;
    float Air_b;  float Air_g; float Air_r;
    cv::Point2i pt;

    int dimone=floor(dimr*dimc*0.001);
    cv::Mat quantile;
    quantile=cv::Mat::zeros(2,dimone,CV_8U);

    int dx=floor(windowsize/2);
    // cv::Mat imagetemp;
    // imagetemp.create(image.rows,image.cols,CV_32FC3);
    for(int j=0;j<dimr;j++){
        for(int i=0;i<dimc;i++){
            int min;
            image.at<cv::Vec3b>(j,i)[0] >= image.at<cv::Vec3b>(j,i)[1]?
               min=image.at<cv::Vec3b>(j,i)[1]:
               min=image.at<cv::Vec3b>(j,i)[0];
            if(min>=image.at<cv::Vec3b>(j,i)[2])
                min=image.at<cv::Vec3b>(j,i)[2];
            rawImage.at<uchar>(j,i)=min;
        }
    }

    for(int j=0;j<dimr;j++){
        for(int i=0;i<dimc;i++){
            int min=255;
            int jlow=j-dx;int jhigh=j+dx;
            int ilow=i-dx;int ihigh=i+dx;

            if(ilow<=0)
                ilow=0;
            if(ihigh>=dimc)
                ihigh=dimc-1;
            if(jlow<=0)
                jlow=0;
            if(jhigh>=dimr)
                jhigh=dimr-1;

            for(int m=jlow;m<=jhigh;m++){
                for(int n=ilow;n<=ihigh;n++){
                    if(min>=rawImage.at<uchar>(m,n))
                        min=rawImage.at<uchar>(m,n);
                }
            }
           rawtemp.at<uchar>(j,i)=min;
        }
    }

    for(int i=0;i<dimone;i++){
        cv::minMaxLoc(rawtemp,0,0,0,&pt);
        sumb+= image.at<cv::Vec3b>(pt.y,pt.x)[0];
        sumg+= image.at<cv::Vec3b>(pt.y,pt.x)[1];
        sumr+= image.at<cv::Vec3b>(pt.y,pt.x)[2];
        rawtemp.at<uchar>(pt.y,pt.x)=0;
    }
    Air_b=sumb/dimone;
    Air_g=sumg/dimone;
    Air_r=sumr/dimone;

    cv::Mat layb;  cv::Mat Im_b;
    cv::Mat layg;  cv::Mat Im_g;
    cv::Mat layr;  cv::Mat Im_r;

    // create vector of 3 images
    std::vector<cv::Mat> planes;
    // split 1 3-channel image into 3 1-channel images
    cv::split(image,planes);

    layb=planes[0];
    layg=planes[1];
    layr=planes[2];
    Im_b=planes[0];
    Im_g=planes[1];
    Im_r=planes[2];

    layb.convertTo(layb, CV_32F);
    layg.convertTo(layg, CV_32F);
    layr.convertTo(layr, CV_32F);

    Im_b.convertTo(Im_b, CV_32F);
    Im_g.convertTo(Im_g, CV_32F);
    Im_r.convertTo(Im_r, CV_32F);

    for (int j=0; j<dimr; j++) {
        for (int i=0; i<dimc; i++) {
        // process each pixel ---------------------
        layb.at<float>(j,i)=layb.at<float>(j,i)/Air_b;

        layg.at<float>(j,i)=layg.at<float>(j,i)/Air_g;

        layr.at<float>(j,i)=layr.at<float>(j,i)/Air_r;
        // end of pixel processing ----------------
        } // end of line
    }

    rawtemp.convertTo(rawtemp, CV_32F);

    for(int j=0;j<dimr;j++){
        for(int i=0;i<dimc;i++){
            float min;
            layb.at<float>(j,i) >= layg.at<float>(j,i)?
               min=layg.at<float>(j,i):
               min=layb.at<float>(j,i);
            if(min>=layr.at<float>(j,i))
                min=layr.at<float>(j,i);
            rawtemp.at<float>(j,i)=min;
        }
    }

    for(int j=0;j<dimr;j++){
        for(int i=0;i<dimc;i++){
            float min=1;

            int jlow=j-dx;int jhigh=j+dx;
            int ilow=i-dx;int ihigh=i+dx;

            if(ilow<=0)
                ilow=0;
            if(ihigh>=dimc)
                ihigh=dimc-1;
            if(jlow<=0)
                jlow=0;
            if(jhigh>=dimr)
                jhigh=dimr-1;

            for(int m=jlow;m<=jhigh;m++){
                for(int n=ilow;n<=ihigh;n++){
                    if(min>=rawtemp.at<float>(m,n))
                        min=rawtemp.at<float>(m,n);
                }
            }
           rawImage.at<uchar>(j,i)=(1-(float)fog_reservation_factor*min)*255;
        }
    }

    refinedImage_temp = guildedfilter_color(image, rawImage, localwindowsize, eps);
    for(int j=0;j<dimr;j++){
        for(int i=0;i<dimc;i++){
            if(refinedImage_temp.at<float>(j,i)<0.1)
                refinedImage_temp.at<float>(j,i)=0.1;
        }
    }

    cv::Mat onemat(dimr,dimc,CV_32F,cv::Scalar(1));

    cv::Mat air_bmat(dimr,dimc,CV_32F);
    cv::Mat air_gmat(dimr,dimc,CV_32F);
    cv::Mat air_rmat(dimr,dimc,CV_32F);

    cv::addWeighted(onemat,Air_b,onemat,0,0,air_bmat);
    cv::addWeighted(onemat,Air_g,onemat,0,0,air_gmat);
    cv::addWeighted(onemat,Air_r,onemat,0,0,air_rmat);

    output_b_temp=Im_b-air_bmat;
    output_g_temp=Im_g-air_gmat;
    output_r_temp=Im_r-air_rmat;

    output_b_temp=output_b_temp.mul(1/refinedImage_temp,1)+air_bmat;
    output_g_temp=output_g_temp.mul(1/refinedImage_temp,1)+air_gmat;
    output_r_temp=output_r_temp.mul(1/refinedImage_temp,1)+air_rmat;

    output_b_temp.convertTo(output_b_temp,CV_8U);
    output_g_temp.convertTo(output_g_temp,CV_8U);
    output_r_temp.convertTo(output_r_temp,CV_8U);

    for(int j=0;j<dimr;j++){
        for(int i=0;i<dimc;i++){
            output_temp.at<cv::Vec3b>(j,i)[0]=output_b_temp.at<uchar>(j,i);
            output_temp.at<cv::Vec3b>(j,i)[1]=output_g_temp.at<uchar>(j,i);
            output_temp.at<cv::Vec3b>(j,i)[2]=output_r_temp.at<uchar>(j,i);
        }
    }
    cv::Mat nom_255(dimr,dimc,CV_32F,cv::Scalar(255));
    cv::Mat ref_temp(dimr,dimc,CV_32F);
    ref_temp = refinedImage_temp;
    ref_temp = ref_temp.mul(nom_255, 1);
    ref_temp.convertTo(refinedImage, CV_8U);
    return output_temp;
}

cv::Mat Dehazor::boxfilter(cv::Mat &im, int r) {
    //im is a CV_32F type mat [0,1] (normalized)
    //output is the same size to im;

    int hei=im.rows;
    int wid=im.cols;
    cv::Mat imDst;
    cv::Mat imCum;

    imDst=cv::Mat::zeros(hei,wid,CV_32F);
    imCum.create(hei,wid,CV_32F);

    //cumulative sum over Y axis
    for(int i=0;i<wid;i++){
        for(int j=0;j<hei;j++){
            if(j==0)
                imCum.at<float>(j,i)=im.at<float>(j,i);
            else
                imCum.at<float>(j,i)=im.at<float>(j,i)+imCum.at<float>(j-1,i);
        }
    }

    //difference over Y axis
    for(int j=0;j<=r;j++){
        for(int i=0;i<wid;i++){
            imDst.at<float>(j,i)=imCum.at<float>(j+r,i);
        }
    }
    for(int j=r+1;j<=hei-r-1;j++){
        for(int i=0;i<wid;i++){
            imDst.at<float>(j,i)=imCum.at<float>(j+r,i)-imCum.at<float>(j-r-1,i);
        }
    }

    for(int j=hei-r;j<hei;j++){
        for(int i=0;i<wid;i++){
            imDst.at<float>(j,i)=imCum.at<float>(hei-1,i)-imCum.at<float>(j-r-1,i);
        }
    }

    //cumulative sum over X axis
    for(int j=0;j<hei;j++){
        for(int i=0;i<wid;i++){
            if(i==0)
                imCum.at<float>(j,i)=imDst.at<float>(j,i);
            else
                imCum.at<float>(j,i)=imDst.at<float>(j,i)+imCum.at<float>(j,i-1);
        }
    }
    //difference over X axis
    for(int j=0;j<hei;j++){
        for(int i=0;i<=r;i++){
            imDst.at<float>(j,i)=imCum.at<float>(j,i+r);
        }
    }
    for(int j=0;j<hei;j++){
        for(int i=r+1;i<=wid-r-1;i++){
            imDst.at<float>(j,i)=imCum.at<float>(j,i+r)-imCum.at<float>(j,i-r-1);
        }
    }
    for(int j=0;j<hei;j++){
        for(int i=wid-r;i<wid;i++){
            imDst.at<float>(j,i)=imCum.at<float>(j,wid-1)-imCum.at<float>(j,i-r-1);
        }
    }

    return imDst;
}

cv::Mat Dehazor::guildedfilter_color(const cv::Mat &Img, cv::Mat &p, int r, float &epsi) {

    int hei=p.rows;
    int wid=p.cols;
    cv::Mat matOne(hei,wid, CV_32F, cv::Scalar(1));
    cv::Mat N;
    N = boxfilter(matOne,r);
    cv::Mat mean_I_b(hei,wid,CV_32F);
    cv::Mat mean_I_g(hei,wid,CV_32F);
    cv::Mat mean_I_r(hei,wid,CV_32F);
    cv::Mat mean_p(hei,wid,CV_32F);

    cv::Mat Ip_b(hei,wid,CV_32F);
    cv::Mat Ip_g(hei,wid,CV_32F);
    cv::Mat Ip_r(hei,wid,CV_32F);
    cv::Mat mean_Ip_b(hei,wid,CV_32F);
    cv::Mat mean_Ip_g(hei,wid,CV_32F);
    cv::Mat mean_Ip_r(hei,wid,CV_32F);
    cv::Mat cov_Ip_b(hei,wid,CV_32F);
    cv::Mat cov_Ip_g(hei,wid,CV_32F);
    cv::Mat cov_Ip_r(hei,wid,CV_32F);

    cv::Mat II_bb(hei,wid,CV_32F);
    cv::Mat II_gg(hei,wid,CV_32F);
    cv::Mat II_rr(hei,wid,CV_32F);
    cv::Mat II_bg(hei,wid,CV_32F);
    cv::Mat II_br(hei,wid,CV_32F);
    cv::Mat II_gr(hei,wid,CV_32F);

    cv::Mat var_I_bb(hei,wid,CV_32F);
    cv::Mat var_I_gg(hei,wid,CV_32F);
    cv::Mat var_I_rr(hei,wid,CV_32F);
    cv::Mat var_I_bg(hei,wid,CV_32F);
    cv::Mat var_I_br(hei,wid,CV_32F);
    cv::Mat var_I_gr(hei,wid,CV_32F);

    cv::Mat layb;
    cv::Mat layg;
    cv::Mat layr;
    cv::Mat P_32;

    // create vector of 3 images
    std::vector<cv::Mat> planes;
    // split 1 3-channel image into 3 1-channel images
    cv::split(Img,planes);
    
    layb=planes[0];
    layg=planes[1];
    layr=planes[2];

    layb.convertTo(layb, CV_32F);
    layg.convertTo(layg, CV_32F);
    layr.convertTo(layr, CV_32F);

    p.convertTo(P_32,CV_32F);
    cv::Mat nom_255(hei,wid,CV_32F,cv::Scalar(255));

    layb=layb.mul(1/nom_255,1);
    layg=layg.mul(1/nom_255,1);
    layr=layr.mul(1/nom_255,1);
    P_32=P_32.mul(1/nom_255,1);

    cv::Mat mean_I_b_temp=boxfilter(layb,r);
    cv::Mat mean_I_g_temp=boxfilter(layg,r);
    cv::Mat mean_I_r_temp=boxfilter(layr,r);
    cv::Mat mean_p_temp=boxfilter(P_32,r);

    mean_I_b=mean_I_b_temp.mul(1/N,1);
    mean_I_g=mean_I_g_temp.mul(1/N,1);
    mean_I_r=mean_I_r_temp.mul(1/N,1);
    mean_p=mean_p_temp.mul(1/N,1);

    Ip_b=layb.mul(P_32,1);
    Ip_g=layg.mul(P_32,1);
    Ip_r=layr.mul(P_32,1);

    cv::Mat mean_Ip_b_temp=boxfilter(Ip_b,r);
    cv::Mat mean_Ip_g_temp=boxfilter(Ip_g,r);
    cv::Mat mean_Ip_r_temp=boxfilter(Ip_r,r);

    mean_Ip_b=mean_Ip_b_temp.mul(1/N,1);
    mean_Ip_g=mean_Ip_g_temp.mul(1/N,1);
    mean_Ip_r=mean_Ip_r_temp.mul(1/N,1);

    cov_Ip_b=mean_Ip_b-mean_I_b.mul(mean_p,1);
    cov_Ip_g=mean_Ip_g-mean_I_g.mul(mean_p,1);
    cov_Ip_r=mean_Ip_r-mean_I_r.mul(mean_p,1);
//     variance of I in each local patch: the matrix Sigma in Eqn (14).
//     Note the variance in each local patch is a 3x3 symmetric matrix:
//                   bb, bg, br
//           Sigma = bg, gg, gr
//                   br, gr, rr
    II_bb=layb.mul(layb,1);
    II_gg=layg.mul(layg,1);
    II_rr=layr.mul(layr,1);
    II_bg=layb.mul(layg,1);
    II_br=layb.mul(layr,1);
    II_gr=layg.mul(layr,1);

    cv::Mat bb_box=boxfilter(II_bb,r);
    cv::Mat gg_box=boxfilter(II_gg,r);
    cv::Mat rr_box=boxfilter(II_rr,r);
    cv::Mat bg_box=boxfilter(II_bg,r);
    cv::Mat br_box=boxfilter(II_br,r);
    cv::Mat gr_box=boxfilter(II_gr,r);

    var_I_bb=bb_box.mul(1/N,1)-mean_I_b.mul(mean_I_b);
    var_I_gg=gg_box.mul(1/N,1)-mean_I_g.mul(mean_I_g);
    var_I_rr=rr_box.mul(1/N,1)-mean_I_r.mul(mean_I_r);
    var_I_bg=bg_box.mul(1/N,1)-mean_I_b.mul(mean_I_g);
    var_I_br=br_box.mul(1/N,1)-mean_I_b.mul(mean_I_r);
    var_I_gr=gr_box.mul(1/N,1)-mean_I_g.mul(mean_I_r);

    cv::Mat a_b(hei,wid,CV_32F);
    cv::Mat a_g(hei,wid,CV_32F);
    cv::Mat a_r(hei,wid,CV_32F);

    cv::Mat b(hei,wid,CV_32F);
    cv::Mat sigma(3,3,CV_32F,cv::Scalar(0));
    cv::Mat inv_sigma(3,3,CV_32F);

    for(int j=0;j<hei;j++){
        for(int i=0;i<wid;i++){
            sigma.at<float>(0,0)=var_I_rr.at<float>(j,i)+epsi;
            sigma.at<float>(0,1)=var_I_gr.at<float>(j,i);
            sigma.at<float>(0,2)=var_I_br.at<float>(j,i);
            sigma.at<float>(1,0)=var_I_gr.at<float>(j,i);
            sigma.at<float>(2,0)=var_I_br.at<float>(j,i);
            sigma.at<float>(1,1)=var_I_gg.at<float>(j,i)+epsi;
            sigma.at<float>(2,2)=var_I_bb.at<float>(j,i)+epsi;
            sigma.at<float>(1,2)=var_I_bg.at<float>(j,i);
            sigma.at<float>(2,1)=var_I_bg.at<float>(j,i);
            inv_sigma=sigma.inv(cv::DECOMP_LU);

            a_r.at<float>(j,i)=cov_Ip_r.at<float>(j,i)*inv_sigma.at<float>(0,0)+
                               cov_Ip_g.at<float>(j,i)*inv_sigma.at<float>(1,0)+
                               cov_Ip_b.at<float>(j,i)*inv_sigma.at<float>(2,0);
            a_g.at<float>(j,i)=cov_Ip_r.at<float>(j,i)*inv_sigma.at<float>(0,1)+
                               cov_Ip_g.at<float>(j,i)*inv_sigma.at<float>(1,1)+
                               cov_Ip_b.at<float>(j,i)*inv_sigma.at<float>(2,1);
            a_b.at<float>(j,i)=cov_Ip_r.at<float>(j,i)*inv_sigma.at<float>(0,2)+
                               cov_Ip_g.at<float>(j,i)*inv_sigma.at<float>(1,2)+
                               cov_Ip_b.at<float>(j,i)*inv_sigma.at<float>(2,2);
        }
    }

    b=mean_p-a_b.mul(mean_I_b,1)-a_g.mul(mean_I_g,1)-a_r.mul(mean_I_r,1);
    cv::Mat box_ab=boxfilter(a_b,r);
    cv::Mat box_ag=boxfilter(a_g,r);
    cv::Mat box_ar=boxfilter(a_r,r);
    cv::Mat box_b=boxfilter(b,r);
    cv::Mat q(hei,wid,CV_32F);

    q=box_ab.mul(layb,1)+box_ag.mul(layg,1)+box_ar.mul(layr,1)+box_b;
    q=q.mul(1/N,1);

    return q;

}
} // namespace dehaze
