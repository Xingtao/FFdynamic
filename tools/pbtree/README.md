## One single header that convert between protobuf object and json string
=============================

#### TODO: This project is incomplete. Right now use proto3's direcotly serialize-deserialize from json string,
Will support proto2 <---> json string convertion later.

### Internally it uses boost ptree as the intermedia representation, namely

    protobuf object -> ptree -> json
    protobuf object <- ptree <- json
   
    Proto3 and boost ptree are required and it is just a convinient wrapper, forget about performance.


### Usage
   mkdir build && cd build && cmake .. && make

   1. probuf object to json string
   ```
   YourProtoMessage pbmsg;
   int ret = PbTree::pbToJsonString(pbmsg, outstr);
   if (ret < 0)
      std::cout << PbTree::errToString(ret) << std::endl;
   ```
   2. json string to protobuf object
   ```
   YourProtoMessage pbmsg;
   int ret = PbTree::pbFromJsonString(pbmsg, instr);
   if (ret < 0)
      std::cout << PbTree::errToString(ret) << std::endl;
   ```
### Others:
   If you would like to reserve the intermedia ptree object; use the following APIs:
   ```
   int PbTree::pbToPtree(const pb::Message & pbmsg, ptree & t);
   int PbTree::pbFromPtree(pb::Message & pbmsg, const ptree & t);
   ```
