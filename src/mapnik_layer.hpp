#ifndef __NODE_MAPNIK_LAYER_H__
#define __NODE_MAPNIK_LAYER_H__

#include <nan.h>
#include "mapnik3x_compatibility.hpp"
// stl
#include <string>

// boost
#include MAPNIK_SHARED_INCLUDE

using namespace v8;

namespace mapnik { class layer; }
typedef MAPNIK_SHARED_PTR<mapnik::layer> layer_ptr;

class Layer: public node::ObjectWrap {
public:
    static Persistent<FunctionTemplate> constructor;
    static void Initialize(Handle<Object> target);
    static NAN_METHOD(New);

    static Handle<Value> New(mapnik::layer const& lay_ref);
    static NAN_METHOD(describe);

    static NAN_GETTER(get_prop);
    static NAN_SETTER(set_prop);

    Layer(std::string const& name);
    Layer(std::string const& name, std::string const& srs);
    Layer();
    inline layer_ptr get() { return layer_; }

private:
    ~Layer();
    layer_ptr layer_;
};

#endif
