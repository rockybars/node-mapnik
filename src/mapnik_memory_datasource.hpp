#ifndef __NODE_MAPNIK_MEMORY_DATASOURCE_H__
#define __NODE_MAPNIK_MEMORY_DATASOURCE_H__

#include <nan.h>
#include "mapnik3x_compatibility.hpp"
#include <boost/scoped_ptr.hpp>
#include <mapnik/memory_datasource.hpp>

using namespace v8;

class MemoryDatasource: public node::ObjectWrap {
public:
    static Persistent<FunctionTemplate> constructor;
    static void Initialize(Handle<Object> target);
    static NAN_METHOD(New);
    static Handle<Value> New(mapnik::datasource_ptr ds_ptr);
    static NAN_METHOD(parameters);
    static NAN_METHOD(describe);
    static NAN_METHOD(features);
    static NAN_METHOD(featureset);
    static NAN_METHOD(add);

    MemoryDatasource();
    inline mapnik::datasource_ptr get() { return datasource_; }

private:
    ~MemoryDatasource();
    mapnik::datasource_ptr datasource_;
    unsigned int feature_id_;
    boost::scoped_ptr<mapnik::transcoder> tr_;
};

#endif
