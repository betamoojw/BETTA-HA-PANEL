// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723
#include "knx_config.h"
#include "registry_fixture.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <iostream>
#include <cJSON.h>
static bool parse(const std::string& text) {
    std::vector<KnxCommunicationObject> objects; std::string canonical, error;
    return KnxParseConfiguration(text,objects,canonical,error);
}
int main(int argc, char** argv) {
    assert(parse(kKnxDefaultConfiguration));
    std::string base=kKnxDefaultConfiguration;
    auto reject=[&](const std::string& old, const std::string& replacement) {
        std::string changed=base; auto p=changed.find(old); assert(p!=std::string::npos);
        changed.replace(p,old.size(),replacement); assert(!parse(changed));
    };
    reject("224.0.23.12","192.168.1.1");
    reject("224.0.23.12","224.0.23.256");
    reject("routing","tunneling"); reject("false","true");
    reject("3671","0"); reject("3671","65536"); reject("3671","3671.5");
    reject("15.15.199","16.1.1");
    reject("\"nat\":false","\"nat\":false,\"nat\":false");
    reject("\"nat\":false","\"nat\":false,\"bad\":0");
    reject("\"media_type\":\"knx_ip\"","\"media_type\":\"knx_ip\",\"media_type\":\"knx_ip\"");
    reject("KNX-IP-Interface","bad\\u0000ID");
    assert(!parse(base+"{}")); assert(!parse("[]"));
    std::string padded = base;
    padded.resize(KNX_MAX_CONFIG_BYTES, ' ');
    assert(parse(padded)); assert(!parse(padded + " "));
    assert(!parse(std::string(1000,'[')+"0"+std::string(1000,']')));
    const char* o=R"({"id":"switch","name":"客厅灯","group_address":"1/0/1","datapoint_type":"DPT-1.001","readable":true,"writable":true})";
    base.replace(base.find("[]"),2,std::string("[")+o+"]"); assert(parse(base));
    reject("\"writable\":true","\"writable\":1");
    reject("\"readable\":true,\"writable\":true","\"readable\":false,\"writable\":false");
    reject("DPT-1.001","DPT-99.001"); reject("DPT-1.001","DPT-1.1");
    reject("1/0/1","32/0/1"); reject("1/0/1","1/8/1"); reject("1/0/1","1/0/256");
    reject("\"id\":\"switch\"","\"id\":\"switch\",\"id\":\"other\"");
    reject("\"id\":\"switch\"","\"id\":\"switch\",\"unknown\":true");
    for (const char* invalid : {"-1", "7", "1.5", "true"})
        reject("\"writable\":true", std::string("\"writable\":true,\"display_precision\":") + invalid);
    std::string precise = base;
    precise.insert(precise.rfind("true}") + 4, ",\"display_precision\":2");
    assert(parse(precise));
    std::string twice=base; twice.insert(twice.rfind(']'), std::string(",")+o); assert(!parse(twice));
    std::vector<KnxCommunicationObject> objects; std::string canonical,error;
    assert(KnxParseConfiguration(base,objects,canonical,error));
    std::string aliases=twice;
    auto second=aliases.rfind("\"switch\"");
    aliases.replace(second, 8, "\"alias\"");
    assert(KnxParseConfiguration(aliases,objects,canonical,error) && objects.size()==2);
    aliases.replace(aliases.rfind("DPT-1.001"),9,"DPT-9.001");
    assert(!KnxParseConfiguration(aliases,objects,canonical,error));
    assert(KnxParseConfiguration(base,objects,canonical,error));
    assert(objects.size()==1 && objects[0].name=="客厅灯" && !objects[0].valid);
    for (unsigned count : {76,128,129,352,512}) {
        const auto registry = registry_fixture(count);
        KnxRoutingParameters route;
        assert(KnxParseConfiguration(registry,objects,canonical,error,&route));
        assert(objects.size()==count && canonical==registry && objects.back().id=="point-"+std::to_string(count-1));
        assert(route.port==3671 && route.multicast_address=="224.0.23.12" && route.physical_address==0xffc7);
    }
    assert(!KnxParseConfiguration(registry_fixture(513),objects,canonical,error));
    assert(objects.empty() && canonical.empty());
    for (int i=1;i<argc;++i) {
        std::ifstream file(argv[i],std::ios::binary); assert(file);
        const std::string registry{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
        assert(KnxParseConfiguration(registry,objects,canonical,error));
        std::cout<<"Saved registry parsed: "<<objects.size()<<" objects\n";
    }
    std::cout<<"KNX strict configuration, addresses, permissions and limits passed\n";
}
