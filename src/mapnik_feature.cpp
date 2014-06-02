#include "utils.hpp"
#include "mapnik_feature.hpp"
#include "mapnik_geometry.hpp"

// mapnik
#include <mapnik/unicode.hpp>
#include <mapnik/feature_factory.hpp>
#include <mapnik/value_types.hpp>

// boost
#include <boost/version.hpp>
#include <boost/scoped_ptr.hpp>
#include MAPNIK_MAKE_SHARED_INCLUDE

#if MAPNIK_VERSION >= 200100
#include <mapnik/json/geojson_generator.hpp>
#include <mapnik/util/geometry_to_wkt.hpp>
#include <mapnik/util/geometry_to_wkb.hpp>
static mapnik::json::feature_generator generator;
#endif

Persistent<FunctionTemplate> Feature::constructor;

void Feature::Initialize(Handle<Object> target) {

    NanScope();

    Local<FunctionTemplate> lcons = NanNew<FunctionTemplate>(Feature::New);
    lcons->InstanceTemplate()->SetInternalFieldCount(1);
    lcons->SetClassName(NanNew("Feature"));

    NODE_SET_PROTOTYPE_METHOD(lcons, "id", id);
    NODE_SET_PROTOTYPE_METHOD(lcons, "extent", extent);
    NODE_SET_PROTOTYPE_METHOD(lcons, "attributes", attributes);
    NODE_SET_PROTOTYPE_METHOD(lcons, "addGeometry", addGeometry);
    NODE_SET_PROTOTYPE_METHOD(lcons, "addAttributes", addAttributes);
    NODE_SET_PROTOTYPE_METHOD(lcons, "numGeometries", numGeometries);
    NODE_SET_PROTOTYPE_METHOD(lcons, "toString", toString);
    NODE_SET_PROTOTYPE_METHOD(lcons, "toJSON", toJSON);
    NODE_SET_PROTOTYPE_METHOD(lcons, "toWKB", toWKB);
    NODE_SET_PROTOTYPE_METHOD(lcons, "toWKT", toWKT);
    target->Set(NanNew("Feature"),lcons->GetFunction());
    NanAssignPersistent(constructor, lcons);
}

Feature::Feature(mapnik::feature_ptr f) :
    ObjectWrap(),
    this_(f) {}

Feature::Feature(int id) :
    ObjectWrap(),
    this_() {
#if MAPNIK_VERSION >= 200100
    // TODO - accept/require context object to reused
    ctx_ = MAPNIK_MAKE_SHARED<mapnik::context_type>();
    this_ = mapnik::feature_factory::create(ctx_,id);
#else
    this_ = mapnik::feature_factory::create(id);
#endif
}

Feature::~Feature()
{
}

NAN_METHOD(Feature::New)
{
    NanScope();

    if (!args.IsConstructCall())
    {
        NanThrowError("Cannot call constructor as function, you need to use 'new' keyword");
        NanReturnUndefined();
    }

    if (args[0]->IsExternal())
    {
        Local<External> ext = args[0].As<External>();
        void* ptr = ext->Value();
        Feature* f =  static_cast<Feature*>(ptr);
        f->Wrap(args.This());
        NanReturnValue(args.This());
    }

    // TODO - expose mapnik.Context

    if (args.Length() > 1 || args.Length() < 1 || !args[0]->IsNumber()) {
        NanThrowTypeError("requires one argument: an integer feature id");
        NanReturnUndefined();
    }

    Feature* f = new Feature(args[0]->IntegerValue());
    f->Wrap(args.This());
    NanReturnValue(args.This());
}

Handle<Value> Feature::New(mapnik::feature_ptr f_ptr)
{
    NanEscapableScope();
    Feature* f = new Feature(f_ptr);
    Handle<Value> ext = NanNew<External>(f);
    Handle<Object> obj = NanNew(constructor)->GetFunction()->NewInstance(1, &ext);
    return NanEscapeScope(obj);
}

NAN_METHOD(Feature::id)
{
    NanScope();

    Feature* fp = node::ObjectWrap::Unwrap<Feature>(args.Holder());
    NanReturnValue(NanNew<Number>(fp->get()->id()));
}

NAN_METHOD(Feature::extent)
{
    NanScope();

    Feature* fp = node::ObjectWrap::Unwrap<Feature>(args.Holder());

    Local<Array> a = NanNew<Array>(4);
    mapnik::box2d<double> const& e = fp->get()->envelope();
    a->Set(0, NanNew<Number>(e.minx()));
    a->Set(1, NanNew<Number>(e.miny()));
    a->Set(2, NanNew<Number>(e.maxx()));
    a->Set(3, NanNew<Number>(e.maxy()));

    NanReturnValue(a);
}

NAN_METHOD(Feature::attributes)
{
    NanScope();

    Feature* fp = node::ObjectWrap::Unwrap<Feature>(args.Holder());

    Local<Object> feat = NanNew<Object>();

#if MAPNIK_VERSION >= 200100
    mapnik::feature_ptr feature = fp->get();
    mapnik::feature_impl::iterator itr = feature->begin();
    mapnik::feature_impl::iterator end = feature->end();
    for ( ;itr!=end; ++itr)
    {
        node_mapnik::params_to_object serializer( feat , MAPNIK_GET<0>(*itr));
        boost::apply_visitor( serializer, MAPNIK_GET<1>(*itr).base() );
    }
#else
    std::map<std::string,mapnik::value> const& fprops = fp->get()->props();
    std::map<std::string,mapnik::value>::const_iterator it = fprops.begin();
    std::map<std::string,mapnik::value>::const_iterator end = fprops.end();
    for (; it != end; ++it)
    {
        node_mapnik::params_to_object serializer( feat , it->first);
        boost::apply_visitor( serializer, it->second.base() );
    }
#endif
    NanReturnValue(feat);
}

NAN_METHOD(Feature::numGeometries)
{
    NanScope();
    Feature* fp = node::ObjectWrap::Unwrap<Feature>(args.Holder());
    NanReturnValue(NanNew<Integer>(fp->get()->num_geometries()));
}

// TODO void?
NAN_METHOD(Feature::addGeometry)
{
    NanScope();

    Feature* fp = node::ObjectWrap::Unwrap<Feature>(args.Holder());

    if (args.Length() >= 1 ) {
        Local<Value> value = args[0];
        if (value->IsNull() || value->IsUndefined()) {
            NanThrowTypeError("mapnik.Geometry instance expected");
            NanReturnUndefined();
        } else {
            Local<Object> obj = value->ToObject();
            if (NanNew(Geometry::constructor)->HasInstance(obj)) {
                Geometry* g = node::ObjectWrap::Unwrap<Geometry>(obj);

                try
                {
                    fp->get()->add_geometry(g->get().get());
                }
                catch (std::exception const& ex )
                {
                    NanThrowError(ex.what());
                    NanReturnUndefined();
                }
            }
        }
    }

    NanReturnUndefined();
}

NAN_METHOD(Feature::addAttributes)
{
    NanScope();

    Feature* fp = node::ObjectWrap::Unwrap<Feature>(args.Holder());

    if (args.Length() > 0 ) {
        Local<Value> val = args[0];
        if (val->IsNull() || val->IsUndefined()) {
            NanThrowTypeError("object expected");
            NanReturnUndefined();
        } else {
            Local<Object> attr = val.As<Object>();
            try
            {
                Local<Array> names = attr->GetPropertyNames();
                unsigned int i = 0;
                unsigned int a_length = names->Length();
                boost::scoped_ptr<mapnik::transcoder> tr(new mapnik::transcoder("utf8"));
                while (i < a_length) {
                    Local<Value> name = names->Get(i)->ToString();
                    Local<Value> value = attr->Get(name);
                    if (value->IsString()) {
                        mapnik::value_unicode_string ustr = tr->transcode(TOSTR(value));
#if MAPNIK_VERSION >= 200100
                        fp->get()->put_new(TOSTR(name),ustr);
#else
                        boost::put(*fp->get(),TOSTR(name),ustr);
#endif
                    } else if (value->IsNumber()) {
                        double num = value->NumberValue();
                        // todo - round
                        if (num == value->IntegerValue()) {
#if MAPNIK_VERSION >= 200100
                            fp->get()->put_new(TOSTR(name),static_cast<node_mapnik::value_integer>(value->IntegerValue()));
#else
                            boost::put(*fp->get(),TOSTR(name),static_cast<int>(value->IntegerValue()));
#endif

                        } else {
                            double dub_val = value->NumberValue();
#if MAPNIK_VERSION >= 200100
                            fp->get()->put_new(TOSTR(name),dub_val);
#else
                            boost::put(*fp->get(),TOSTR(name),dub_val);
#endif
                        }
                    } else if (value->IsNull()) {
#if MAPNIK_VERSION >= 200100
                        fp->get()->put_new(TOSTR(name),mapnik::value_null());
#else
                        boost::put(*fp->get(),TOSTR(name),mapnik::value_null());
#endif
                    } else {
                        std::clog << "unhandled type for property: " << TOSTR(name) << "\n";
                    }
                    i++;
                }
            }
            catch (std::exception const& ex )
            {
                NanThrowError(ex.what());
                NanReturnUndefined();
            }
        }
    }

    NanReturnUndefined();
}

NAN_METHOD(Feature::toString)
{
    NanScope();

    Feature* fp = node::ObjectWrap::Unwrap<Feature>(args.Holder());
    NanReturnValue(NanNew(fp->get()->to_string().c_str()));
}

NAN_METHOD(Feature::toJSON)
{
    NanScope();

    std::string json;
#if BOOST_VERSION >= 104700 && MAPNIK_VERSION >= 200100
    Feature* fp = node::ObjectWrap::Unwrap<Feature>(args.Holder());
    // TODO - create once?
    if (!generator.generate(json,*(fp->get())))
    {
        NanThrowError("Failed to generate GeoJSON");
        NanReturnUndefined();
    }
#else
    NanThrowError("GeoJSON output requires at least boost 1.47 and mapnik 2.1.x");
    NanReturnUndefined();
#endif

    NanReturnValue(NanNew(json.c_str()));
}

NAN_METHOD(Feature::toWKT)
{
    NanScope();

    std::string wkt;
#if BOOST_VERSION >= 104700 && MAPNIK_VERSION >= 200100
    Feature* fp = node::ObjectWrap::Unwrap<Feature>(args.Holder());

    if (!mapnik::util::to_wkt(wkt, fp->get()->paths()))
    {
        NanThrowError("Failed to generate WKT");
        NanReturnUndefined();
    }
#else
    NanThrowError("WKT output requires at least boost 1.47 and mapnik 2.1.x");
    NanReturnUndefined();
#endif

    NanReturnValue(NanNew(wkt.c_str()));
}

NAN_METHOD(Feature::toWKB)
{
    NanScope();

    std::string wkt;
#if BOOST_VERSION >= 104700 && MAPNIK_VERSION >= 200100
    Feature* fp = node::ObjectWrap::Unwrap<Feature>(args.Holder());

    mapnik::util::wkb_buffer_ptr wkb = mapnik::util::to_wkb(fp->get()->paths(), mapnik::util::wkbNDR);
    NanReturnValue(NanNewBufferHandle(wkb->buffer(), wkb->size()));
#else
    NanThrowError("WKB output requires at least boost 1.47 and mapnik 2.1.x");
    NanReturnUndefined();
#endif
}
