#include "mapnik_datasource.hpp"
#include "mapnik_featureset.hpp"
#include "utils.hpp"
#include "ds_emitter.hpp"

// mapnik
#include <mapnik/attribute_descriptor.hpp>  // for attribute_descriptor
#include <mapnik/box2d.hpp>             // for box2d
#include <mapnik/datasource.hpp>        // for datasource, datasource_ptr, etc
#include <mapnik/datasource_cache.hpp>  // for datasource_cache
#include <mapnik/feature_layer_desc.hpp>  // for layer_descriptor
#include <mapnik/params.hpp>            // for parameters
#include <mapnik/query.hpp>             // for query
#include <mapnik/version.hpp>           // for MAPNIK_VERSION

// stl
#include <exception>
#include <vector>

Persistent<FunctionTemplate> Datasource::constructor;

void Datasource::Initialize(Handle<Object> target) {

    NanScope();

    Local<FunctionTemplate> lcons = NanNew<FunctionTemplate>(Datasource::New);
    lcons->InstanceTemplate()->SetInternalFieldCount(1);
    lcons->SetClassName(NanNew("Datasource"));

    // methods
    NODE_SET_PROTOTYPE_METHOD(lcons, "parameters", parameters);
    NODE_SET_PROTOTYPE_METHOD(lcons, "describe", describe);
    NODE_SET_PROTOTYPE_METHOD(lcons, "features", features);
    NODE_SET_PROTOTYPE_METHOD(lcons, "featureset", featureset);
    NODE_SET_PROTOTYPE_METHOD(lcons, "extent", extent);

    target->Set(NanNew("Datasource"), lcons->GetFunction());
    NanAssignPersistent(constructor, lcons);
}

Datasource::Datasource() :
    ObjectWrap(),
    datasource_() {}

Datasource::~Datasource()
{
}

NAN_METHOD(Datasource::New)
{
    NanScope();

    if (!args.IsConstructCall())
    {
        NanThrowError("Cannot call constructor as function, you need to use 'new' keyword");
        NanReturnUndefined();
    }

    if (args[0]->IsExternal())
    {
        //std::clog << "external!\n";
        Local<External> ext = args[0].As<External>();
        void* ptr = ext->Value();
        Datasource* d =  static_cast<Datasource*>(ptr);
        if (d->datasource_->type() == mapnik::datasource::Raster)
        {
            args.This()->Set(NanNew("type"),
                             NanNew("raster"),
                             static_cast<v8::PropertyAttribute>(v8::ReadOnly|v8::DontDelete));
        }
        else
        {
            args.This()->Set(NanNew("type"),
                             NanNew("vector"),
                             static_cast<v8::PropertyAttribute>(v8::ReadOnly|v8::DontDelete));
        }
        d->Wrap(args.This());
        NanReturnValue(args.This());
    }
    if (args.Length() != 1)
    {
        NanThrowTypeError("accepts only one argument, an object of key:value datasource options");
        NanReturnUndefined();
    }

    if (!args[0]->IsObject())
    {
        NanThrowTypeError("Must provide an object, eg {type: 'shape', file : 'world.shp'}");
        NanReturnUndefined();
    }

    Local<Object> options = args[0].As<Object>();

    mapnik::parameters params;
    Local<Array> names = options->GetPropertyNames();
    unsigned int i = 0;
    unsigned int a_length = names->Length();
    while (i < a_length) {
        Local<Value> name = names->Get(i)->ToString();
        Local<Value> value = options->Get(name);
        // TODO - don't treat everything as strings
        params[TOSTR(name)] = TOSTR(value);
        i++;
    }

    mapnik::datasource_ptr ds;
    try
    {
#if MAPNIK_VERSION >= 200200
        ds = mapnik::datasource_cache::instance().create(params);
#else
        ds = mapnik::datasource_cache::instance()->create(params);
#endif
    }
    catch (std::exception const& ex)
    {
        NanThrowError(ex.what());
        NanReturnUndefined();
    }

    if (ds)
    {
        if (ds->type() == mapnik::datasource::Raster)
        {
            args.This()->Set(NanNew("type"),
                             NanNew("raster"),
                             static_cast<v8::PropertyAttribute>(v8::ReadOnly|v8::DontDelete));
        }
        else
        {
            args.This()->Set(NanNew("type"),
                             NanNew("vector"),
                             static_cast<v8::PropertyAttribute>(v8::ReadOnly|v8::DontDelete));
        }
        Datasource* d = new Datasource();
        d->Wrap(args.This());
        d->datasource_ = ds;
        NanReturnValue(args.This());
    }
    NanReturnUndefined();
}

Handle<Value> Datasource::New(mapnik::datasource_ptr ds_ptr) {
    NanEscapableScope();
    Datasource* d = new Datasource();
    d->datasource_ = ds_ptr;
    Handle<Value> ext = NanNew<External>(d);
    Handle<Object> obj = NanNew(constructor)->GetFunction()->NewInstance(1, &ext);
    return NanEscapeScope(obj);
}

NAN_METHOD(Datasource::parameters)
{
    NanScope();
    Datasource* d = node::ObjectWrap::Unwrap<Datasource>(args.This());
    Local<Object> ds = NanNew<Object>();
    mapnik::parameters::const_iterator it = d->datasource_->params().begin();
    mapnik::parameters::const_iterator end = d->datasource_->params().end();
    for (; it != end; ++it)
    {
        node_mapnik::params_to_object serializer( ds , it->first);
        boost::apply_visitor( serializer, it->second );
    }
    NanReturnValue(ds);
}

NAN_METHOD(Datasource::extent)
{
    NanScope();
    Datasource* d = node::ObjectWrap::Unwrap<Datasource>(args.Holder());
    mapnik::box2d<double> e;
    try
    {
        e = d->datasource_->envelope();
    }
    catch (std::exception const& ex)
    {
        NanThrowError(ex.what());
        NanReturnUndefined();
    }

    Local<Array> a = NanNew<Array>(4);
    a->Set(0, NanNew<Number>(e.minx()));
    a->Set(1, NanNew<Number>(e.miny()));
    a->Set(2, NanNew<Number>(e.maxx()));
    a->Set(3, NanNew<Number>(e.maxy()));
    NanReturnValue(a);
}

NAN_METHOD(Datasource::describe)
{
    NanScope();
    Datasource* d = node::ObjectWrap::Unwrap<Datasource>(args.Holder());
    Local<Object> description = NanNew<Object>();
    try
    {
        node_mapnik::describe_datasource(description,d->datasource_);
    }
    catch (std::exception const& ex)
    {
        NanThrowError(ex.what());
        NanReturnUndefined();
    }

    NanReturnValue(description);
}

NAN_METHOD(Datasource::features)
{

    std::clog << "Datasource.features() is deprecated and will be removed at node-mapnik 1.3.x (please use Datasource.featureset instead)\n";
    NanScope();

    unsigned first = 0;
    unsigned last = 0;
    // we are slicing
    if (args.Length() == 2)
    {
        if (!args[0]->IsNumber() || !args[1]->IsNumber())
        {
            NanThrowTypeError("Index of 'first' and 'last' feature must be an integer");
            NanReturnUndefined();
        }
        first = args[0]->IntegerValue();
        last = args[1]->IntegerValue();
    }

    Datasource* d = node::ObjectWrap::Unwrap<Datasource>(args.Holder());

    // TODO - we don't know features.length at this point
    Local<Array> a = NanNew<Array>(0);
    try
    {
        node_mapnik::datasource_features(a,d->datasource_,first,last);
    }
    catch (std::exception const& ex)
    {
        NanThrowError(ex.what());
        NanReturnUndefined();
    }

    NanReturnValue(a);
}

NAN_METHOD(Datasource::featureset)
{

    NanScope();

    Datasource* ds = node::ObjectWrap::Unwrap<Datasource>(args.Holder());

    mapnik::featureset_ptr fs;
    try
    {
        mapnik::query q(ds->datasource_->envelope());
        mapnik::layer_descriptor ld = ds->datasource_->get_descriptor();
        std::vector<mapnik::attribute_descriptor> const& desc = ld.get_descriptors();
        std::vector<mapnik::attribute_descriptor>::const_iterator itr = desc.begin();
        std::vector<mapnik::attribute_descriptor>::const_iterator end = desc.end();
        while (itr != end)
        {
            q.add_property_name(itr->get_name());
            ++itr;
        }

        fs = ds->datasource_->features(q);
    }
    catch (std::exception const& ex)
    {
        NanThrowError(ex.what());
        NanReturnUndefined();
    }

    if (fs)
    {
        NanReturnValue(Featureset::New(fs));
    }

    NanReturnUndefined();
}
