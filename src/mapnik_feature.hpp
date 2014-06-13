#ifndef __NODE_MAPNIK_FEATURE_H__
#define __NODE_MAPNIK_FEATURE_H__

#include <nan.h>
#include "mapnik3x_compatibility.hpp"

// mapnik
#include <mapnik/version.hpp>
#include <mapnik/feature.hpp>
#include <mapnik/datasource.hpp> // feature_ptr and featureset_ptr

// boost
#include MAPNIK_SHARED_INCLUDE

using namespace v8;

class Feature: public node::ObjectWrap {
public:
    static Persistent<FunctionTemplate> constructor;
    static void Initialize(Handle<Object> target);
    static NAN_METHOD(New);
    static Handle<Value> New(mapnik::feature_ptr f_ptr);
    static NAN_METHOD(id);
    static NAN_METHOD(extent);
    static NAN_METHOD(attributes);
    static NAN_METHOD(addGeometry);
    static NAN_METHOD(addAttributes);
    static NAN_METHOD(numGeometries);
    static NAN_METHOD(toString);
    static NAN_METHOD(toJSON);
    static NAN_METHOD(toWKB);
    static NAN_METHOD(toWKT);

    // todo
    // how to allow altering of attributes
    // expose get_geometry
    Feature(mapnik::feature_ptr f);
    Feature(int id);
    inline mapnik::feature_ptr get() { return this_; }

private:
    ~Feature();
    mapnik::feature_ptr this_;
#if MAPNIK_VERSION >= 200100
    mapnik::context_ptr ctx_;
#endif
};

#endif
