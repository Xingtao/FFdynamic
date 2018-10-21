#include <string>

#include "pbtree.h"
#include "test.pb.h"
#include "test2.pb.h"

using ::std::string;
using namespace test;
using namespace pb_tree;

int main(int argc, char **argv) {

    DavSetVideoMixLayout vl;
    vl.set_layout(EVideoMixLayout::eHorizonal_2);
    for (int k=0; k < 3; k++) {
        auto cell = vl.add_cells();
        cell->set_x(10);
        cell->set_y(-10);
        cell->set_w(20);
        cell->set_h(-20);
        cell->set_layer(k);
    }

    /* pb to strings */
    int ret = 0;
    string jsonstr ;
    std::cout << "pb to json -->\n";
    ret = PbTree::pbToJsonString(vl, jsonstr);
    if (ret < 0)
        std::cerr << PbTree::errToString(ret) << std::endl;
    else
        std::cout << jsonstr << std::endl;

    /* pb from strings */
    DavSetVideoMixLayout jsonvl;
    std::cout << "pb from json <--\n";
    ret = PbTree::pbFromJsonString(jsonvl, jsonstr);
    if (ret < 0)
        std::cerr << PbTree::errToString(ret) << std::endl;
    else
        std::cout << "pass json test: "
                  << (vl.layout() == jsonvl.layout() && vl.cells().size() == jsonvl.cells().size())
                  << std::endl;

    /* Testing for dump out */
    DavWaveSetting::DemuxSetting ds;
    string dsstr;
    ret = PbTree::pbToJsonString(ds, dsstr);
    std::cout << dsstr << std::endl;

    DavWaveSetting::VideoFilterSetting vfs;
    string vfsstr;
    ret = PbTree::pbToJsonString(vfs, vfsstr);
    std::cout << vfsstr << std::endl;

    DavWaveSetting::AudioFilterSetting afs;
    string afsstr;
    ret = PbTree::pbToJsonString(afs, afsstr);
    std::cout << afsstr << std::endl;

    DavWaveSetting::VideoMixSetting vms;
    string vmsstr;
    DavWaveSetting::VideoMixLayoutInfo layoutInfo;
    layoutInfo.set_layout(DavWaveSetting::EVideoMixLayout::eLayoutAuto);
    vms.set_allocated_layout_info(&layoutInfo);
    ret = PbTree::pbToJsonString(vms, vmsstr);
    std::cout << vmsstr << std::endl;

    DavWaveSetting::VideoEncodeSetting ves;
    string vesstr;
    ret = PbTree::pbToJsonString(ves, vesstr);
    std::cout << vesstr << std::endl;

    DavWaveSetting::MuxSetting ms;
    string msstr;
    ret = PbTree::pbToJsonString(ms, msstr);
    std::cout << msstr << std::endl;

    return 0;
}
