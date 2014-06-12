#ifndef __NODE_MAPNIK_DATASOURCE_H__
#define __NODE_MAPNIK_DATASOURCE_H__

#include <nan.h>
#include "mapnik3x_compatibility.hpp"

#include MAPNIK_SHARED_INCLUDE

using namespace v8;

namespace mapnik { class datasource; }

typedef MAPNIK_SHARED_PTR<mapnik::datasource> datasource_ptr;

class Datasource: public node::ObjectWrap {
public:
    static Persistent<FunctionTemplate> constructor;
    static void Initialize(Handle<Object> target);
    static NAN_METHOD(New);
    static Handle<Value> New(datasource_ptr ds_ptr);

    static NAN_METHOD(parameters);
    static NAN_METHOD(describe);
    static NAN_METHOD(features);
    static NAN_METHOD(featureset);
    static NAN_METHOD(extent);

    Datasource();
    inline datasource_ptr get() { return datasource_; }

private:
    ~Datasource();
    datasource_ptr datasource_;
};

#endif
