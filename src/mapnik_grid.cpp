
// mapnik
#include <mapnik/version.hpp>


#include "mapnik_grid.hpp"
#include "mapnik_grid_view.hpp"
#include "js_grid_utils.hpp"
#include "utils.hpp"

// boost
#include "boost/ptr_container/ptr_sequence_adapter.hpp"
#include "boost/ptr_container/ptr_vector.hpp"  // for ptr_vector
#include MAPNIK_MAKE_SHARED_INCLUDE
#include "boost/cstdint.hpp"            // for uint16_t

// std
#include <exception>

Persistent<FunctionTemplate> Grid::constructor;

void Grid::Initialize(Handle<Object> target) {

    NanScope();

    Local<FunctionTemplate> lcons = NanNew<FunctionTemplate>(Grid::New);
    lcons->InstanceTemplate()->SetInternalFieldCount(1);
    lcons->SetClassName(NanNew("Grid"));

    // methods
    NODE_SET_PROTOTYPE_METHOD(lcons, "encodeSync", encodeSync);
    NODE_SET_PROTOTYPE_METHOD(lcons, "encode", encode);
    NODE_SET_PROTOTYPE_METHOD(lcons, "fields", fields);
    NODE_SET_PROTOTYPE_METHOD(lcons, "view", view);
    NODE_SET_PROTOTYPE_METHOD(lcons, "width", width);
    NODE_SET_PROTOTYPE_METHOD(lcons, "height", height);
    NODE_SET_PROTOTYPE_METHOD(lcons, "painted", painted);
    NODE_SET_PROTOTYPE_METHOD(lcons, "clear", clear);
    NODE_SET_PROTOTYPE_METHOD(lcons, "clearSync", clear);
    // properties
    ATTR(lcons, "key", get_prop, set_prop);

    target->Set(NanNew("Grid"), lcons->GetFunction());
#if MAPNIK_VERSION < 200100
    NODE_MAPNIK_DEFINE_CONSTANT(lcons->GetFunction(), "base_mask", 0);
#else
    NODE_MAPNIK_DEFINE_64_BIT_CONSTANT(lcons->GetFunction(), "base_mask", mapnik::grid::base_mask);
#endif

    NanAssignPersistent(constructor, lcons);
}

Grid::Grid(unsigned int width, unsigned int height, std::string const& key, unsigned int resolution) :
    ObjectWrap(),
    this_(MAPNIK_MAKE_SHARED<mapnik::grid>(width,height,key,resolution)),
    estimated_size_(width * height) {
#if MAPNIK_VERSION <= 200100
    this_->painted(false);
#endif
    NanAdjustExternalMemory(estimated_size_);
}

Grid::~Grid()
{
    NanAdjustExternalMemory(-estimated_size_);
}

NAN_METHOD(Grid::New)
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
        Grid* g =  static_cast<Grid*>(ptr);
        g->Wrap(args.This());
        NanReturnValue(args.This());
    }

    if (args.Length() >= 2)
    {
        if (!args[0]->IsNumber() || !args[1]->IsNumber())
        {
            NanThrowTypeError("Grid 'width' and 'height' must be a integers");
            NanReturnUndefined();
        }

        // defaults
        std::string key("__id__");
        unsigned int resolution = 1;

        if (args.Length() >= 3) {

            if (!args[2]->IsObject())
            {
                NanThrowTypeError("optional third arg must be an options object");
                NanReturnUndefined();
            }
            Local<Object> options = args[2].As<Object>();

            if (options->Has(NanNew("key"))) {
                Local<Value> bind_opt = options->Get(NanNew("key"));
                if (!bind_opt->IsString())
                {
                    NanThrowTypeError("optional arg 'key' must be an string");
                    NanReturnUndefined();
                }

                key = TOSTR(bind_opt);
            }
            // TODO - remove, deprecated
            if (options->Has(NanNew("resolution"))) {
                Local<Value> bind_opt = options->Get(NanNew("resolution"));
                if (!bind_opt->IsNumber())
                {
                    NanThrowTypeError("optional arg 'resolution' must be an string");
                    NanReturnUndefined();
                }

                resolution = bind_opt->IntegerValue();
            }
        }

        Grid* g = new Grid(args[0]->IntegerValue(), args[1]->IntegerValue(), key, resolution);
        g->Wrap(args.This());
        NanReturnValue(args.This());
    }
    else
    {
        NanThrowError("please provide Grid width and height");
        NanReturnUndefined();
    }
    NanReturnUndefined();
}

NAN_METHOD(Grid::clearSync)
{
    NanScope();
    NanReturnValue(_clearSync(args));
}

Local<Value> Grid::_clearSync(_NAN_METHOD_ARGS)
{
    NanEscapableScope();
#if MAPNIK_VERSION >= 200200
    Grid* g = node::ObjectWrap::Unwrap<Grid>(args.Holder());
    g->get()->clear();
#endif
    return NanEscapeScope(NanUndefined());
}

typedef struct {
    uv_work_t request;
    Grid* g;
    std::string format;
    bool error;
    std::string error_name;
    Persistent<Function> cb;
} clear_grid_baton_t;

NAN_METHOD(Grid::clear)
{
    NanScope();
    Grid* g = node::ObjectWrap::Unwrap<Grid>(args.Holder());

    if (args.Length() == 0) {
        NanReturnValue(_clearSync(args));
    }
    // ensure callback is a function
    Local<Value> callback = args[args.Length()-1];
    if (!args[args.Length()-1]->IsFunction())
    {
        NanThrowTypeError("last argument must be a callback function");
        NanReturnUndefined();
    }
    clear_grid_baton_t *closure = new clear_grid_baton_t();
    closure->request.data = closure;
    closure->g = g;
    closure->error = false;
    NanAssignPersistent(closure->cb, callback.As<Function>());
    uv_queue_work(uv_default_loop(), &closure->request, EIO_Clear, (uv_after_work_cb)EIO_AfterClear);
    g->Ref();
    NanReturnUndefined();
}

void Grid::EIO_Clear(uv_work_t* req)
{
#if MAPNIK_VERSION >= 200200
    clear_grid_baton_t *closure = static_cast<clear_grid_baton_t *>(req->data);
    try
    {
        closure->g->get()->clear();
    }
    catch(std::exception const& ex)
    {
        closure->error = true;
        closure->error_name = ex.what();
    }
#endif
}

void Grid::EIO_AfterClear(uv_work_t* req)
{
    NanScope();
    clear_grid_baton_t *closure = static_cast<clear_grid_baton_t *>(req->data);
    if (closure->error)
    {
        Local<Value> argv[1] = { NanError(closure->error_name.c_str()) };
        NanMakeCallback(NanGetCurrentContext()->Global(), NanNew(closure->cb), 1, argv);
    }
    else
    {
        Local<Value> argv[2] = { NanNull() };
        NanMakeCallback(NanGetCurrentContext()->Global(), NanNew(closure->cb), 1, argv);
    }
    closure->g->Unref();
    NanDisposePersistent(closure->cb);
    delete closure;
}

NAN_METHOD(Grid::painted)
{
    NanScope();

    Grid* g = node::ObjectWrap::Unwrap<Grid>(args.Holder());
    NanReturnValue(NanNew(g->get()->painted()));
}

NAN_METHOD(Grid::width)
{
    NanScope();

    Grid* g = node::ObjectWrap::Unwrap<Grid>(args.Holder());
    NanReturnValue(NanNew<Integer>(g->get()->width()));
}

NAN_METHOD(Grid::height)
{
    NanScope();

    Grid* g = node::ObjectWrap::Unwrap<Grid>(args.Holder());
    NanReturnValue(NanNew<Integer>(g->get()->height()));
}

NAN_GETTER(Grid::get_prop)
{
    NanScope();
    Grid* g = node::ObjectWrap::Unwrap<Grid>(args.Holder());
    std::string a = TOSTR(property);
    if (a == "key")
    {
        NanReturnValue(NanNew(g->get()->get_key().c_str()));
    }
    NanReturnUndefined();
}

NAN_SETTER(Grid::set_prop)
{
    NanScope();
    Grid* g = node::ObjectWrap::Unwrap<Grid>(args.Holder());
    std::string a = TOSTR(property);
    if (a == "key") {
        if (!value->IsNumber())
        {
            NanThrowTypeError("width must be an integer");
            return;
        }
        g->get()->set_key(TOSTR(value));
    }
}


NAN_METHOD(Grid::fields)
{
    NanScope();

    Grid* g = node::ObjectWrap::Unwrap<Grid>(args.Holder());
    std::set<std::string> const& a = g->get()->property_names();
    std::set<std::string>::const_iterator itr = a.begin();
    std::set<std::string>::const_iterator end = a.end();
    Local<Array> l = NanNew<Array>(a.size());
    int idx = 0;
    for (; itr != end; ++itr)
    {
        std::string name = *itr;
        l->Set(idx, NanNew(name.c_str()));
        ++idx;
    }
    NanReturnValue(l);

}

NAN_METHOD(Grid::view)
{
    NanScope();

    if ( (args.Length() != 4) || (!args[0]->IsNumber() && !args[1]->IsNumber() && !args[2]->IsNumber() && !args[3]->IsNumber() ))
    {
        NanThrowTypeError("requires 4 integer arguments: x, y, width, height");
        NanReturnUndefined();
    }

    unsigned x = args[0]->IntegerValue();
    unsigned y = args[1]->IntegerValue();
    unsigned w = args[2]->IntegerValue();
    unsigned h = args[3]->IntegerValue();

    Grid* g = node::ObjectWrap::Unwrap<Grid>(args.Holder());
    NanReturnValue(GridView::New(g,x,y,w,h));
}


#if MAPNIK_VERSION >= 200100

NAN_METHOD(Grid::encodeSync) // format, resolution
{
    NanScope();

    Grid* g = node::ObjectWrap::Unwrap<Grid>(args.Holder());

    // defaults
    std::string format("utf");
    unsigned int resolution = 4;
    bool add_features = true;

    // accept custom format
    if (args.Length() >= 1){
        if (!args[0]->IsString())
        {
            NanThrowTypeError("first arg, 'format' must be a string");
            NanReturnUndefined();
        }
        format = TOSTR(args[0]);
    }

    // options hash
    if (args.Length() >= 2) {
        if (!args[1]->IsObject())
        {
            NanThrowTypeError("optional second arg must be an options object");
            NanReturnUndefined();
        }

        Local<Object> options = args[1].As<Object>();

        if (options->Has(NanNew("resolution")))
        {
            Local<Value> bind_opt = options->Get(NanNew("resolution"));
            if (!bind_opt->IsNumber())
            {
                NanThrowTypeError("'resolution' must be an Integer");
                NanReturnUndefined();
            }

            resolution = bind_opt->IntegerValue();
        }

        if (options->Has(NanNew("features")))
        {
            Local<Value> bind_opt = options->Get(NanNew("features"));
            if (!bind_opt->IsBoolean())
            {
                NanThrowTypeError("'features' must be an Boolean");
                NanReturnUndefined();
            }

            add_features = bind_opt->BooleanValue();
        }
    }

    try {

        boost::ptr_vector<uint16_t> lines;
        std::vector<mapnik::grid::lookup_type> key_order;
        node_mapnik::grid2utf<mapnik::grid>(*g->get(),lines,key_order,resolution);

        // convert key order to proper javascript array
        Local<Array> keys_a = NanNew<Array>(key_order.size());
        std::vector<std::string>::iterator it;
        unsigned int i;
        for (it = key_order.begin(), i = 0; it < key_order.end(); ++it, ++i)
        {
            keys_a->Set(i, NanNew((*it).c_str()));
        }

        mapnik::grid const& grid_type = *g->get();

        // gather feature data
        Local<Object> feature_data = NanNew<Object>();
        if (add_features) {
            node_mapnik::write_features<mapnik::grid>(*g->get(),
                                                      feature_data,
                                                      key_order);
        }

        // Create the return hash.
        Local<Object> json = NanNew<Object>();
        Local<Array> grid_array = NanNew<Array>();
        unsigned array_size = std::ceil(grid_type.width()/static_cast<float>(resolution));
        for (unsigned j=0;j<lines.size();++j)
        {
            grid_array->Set(j, NanNew(&lines[j],array_size));
        }
        json->Set(NanNew("grid"), grid_array);
        json->Set(NanNew("keys"), keys_a);
        json->Set(NanNew("data"), feature_data);
        NanReturnValue(json);

    }
    catch (std::exception const& ex)
    {
        NanThrowError(ex.what());
        NanReturnUndefined();
    }
}

typedef struct {
    uv_work_t request;
    Grid* g;
    std::string format;
    bool error;
    std::string error_name;
    Persistent<Function> cb;
    boost::ptr_vector<uint16_t> lines;
    unsigned int resolution;
    bool add_features;
    std::vector<mapnik::grid::lookup_type> key_order;
} encode_grid_baton_t;

NAN_METHOD(Grid::encode) // format, resolution
{
    NanScope();

    Grid* g = node::ObjectWrap::Unwrap<Grid>(args.Holder());

    // defaults
    std::string format("utf");
    unsigned int resolution = 4;
    bool add_features = true;

    // accept custom format
    if (args.Length() >= 1){
        if (!args[0]->IsString())
        {
            NanThrowTypeError("first arg, 'format' must be a string");
            NanReturnUndefined();
        }
        format = TOSTR(args[0]);
    }

    // options hash
    if (args.Length() >= 2) {
        if (!args[1]->IsObject())
        {
            NanThrowTypeError("optional second arg must be an options object");
            NanReturnUndefined();
        }

        Local<Object> options = args[1].As<Object>();

        if (options->Has(NanNew("resolution")))
        {
            Local<Value> bind_opt = options->Get(NanNew("resolution"));
            if (!bind_opt->IsNumber())
            {
                NanThrowTypeError("'resolution' must be an Integer");
                NanReturnUndefined();
            }

            resolution = bind_opt->IntegerValue();
        }

        if (options->Has(NanNew("features")))
        {
            Local<Value> bind_opt = options->Get(NanNew("features"));
            if (!bind_opt->IsBoolean())
            {
                NanThrowTypeError("'features' must be an Boolean");
                NanReturnUndefined();
            }

            add_features = bind_opt->BooleanValue();
        }
    }

    // ensure callback is a function
    if (!args[args.Length()-1]->IsFunction())
    {
        NanThrowTypeError("last argument must be a callback function");
        NanReturnUndefined();
    }
    Local<Function> callback = Local<Function>::Cast(args[args.Length()-1]);

    encode_grid_baton_t *closure = new encode_grid_baton_t();
    closure->request.data = closure;
    closure->g = g;
    closure->format = format;
    closure->error = false;
    closure->resolution = resolution;
    closure->add_features = add_features;
    NanAssignPersistent(closure->cb, callback.As<Function>());
    // todo - reserve lines size?
    uv_queue_work(uv_default_loop(), &closure->request, EIO_Encode, (uv_after_work_cb)EIO_AfterEncode);
    g->Ref();
    NanReturnUndefined();
}

void Grid::EIO_Encode(uv_work_t* req)
{
    encode_grid_baton_t *closure = static_cast<encode_grid_baton_t *>(req->data);

    try
    {
        node_mapnik::grid2utf<mapnik::grid>(*closure->g->get(),
                                            closure->lines,
                                            closure->key_order,
                                            closure->resolution);
    }
    catch (std::exception const& ex)
    {
        closure->error = true;
        closure->error_name = ex.what();
    }
}

void Grid::EIO_AfterEncode(uv_work_t* req)
{
    NanScope();

    encode_grid_baton_t *closure = static_cast<encode_grid_baton_t *>(req->data);


    if (closure->error) {
        Local<Value> argv[1] = { NanError(closure->error_name.c_str()) };
        NanMakeCallback(NanGetCurrentContext()->Global(), NanNew(closure->cb), 1, argv);
    } else {

        // convert key order to proper javascript array
        Local<Array> keys_a = NanNew<Array>(closure->key_order.size());
        std::vector<std::string>::iterator it;
        unsigned int i;
        for (it = closure->key_order.begin(), i = 0; it < closure->key_order.end(); ++it, ++i)
        {
            keys_a->Set(i, NanNew((*it).c_str()));
        }

        mapnik::grid const& grid_type = *closure->g->get();
        // gather feature data
        Local<Object> feature_data = NanNew<Object>();
        if (closure->add_features) {
            node_mapnik::write_features<mapnik::grid>(grid_type,
                                                      feature_data,
                                                      closure->key_order);
        }

        // Create the return hash.
        Local<Object> json = NanNew<Object>();
        Local<Array> grid_array = NanNew<Array>(closure->lines.size());
        unsigned array_size = std::ceil(grid_type.width()/static_cast<float>(closure->resolution));
        for (unsigned j=0;j<closure->lines.size();++j)
        {
            grid_array->Set(j, NanNew(&closure->lines[j],array_size));
        }
        json->Set(NanNew("grid"), grid_array);
        json->Set(NanNew("keys"), keys_a);
        json->Set(NanNew("data"), feature_data);

        Local<Value> argv[2] = { NanNull(), NanNew(json) };
        NanMakeCallback(NanGetCurrentContext()->Global(), NanNew(closure->cb), 2, argv);
    }

    closure->g->Unref();
    NanDisposePersistent(closure->cb);
    delete closure;
}


#else

NAN_METHOD(Grid::encodeSync) // format, resolution
{
    NanScope();

    Grid* g = node::ObjectWrap::Unwrap<Grid>(args.Holder());

    // defaults
    std::string format("utf");
    unsigned int resolution = 4;
    bool add_features = true;

    // accept custom format
    if (args.Length() >= 1){
        if (!args[0]->IsString())
        {
            NanThrowTypeError("first arg, 'format' must be a string");
            NanReturnUndefined();
        }
        format = TOSTR(args[0]);
    }

    // options hash
    if (args.Length() >= 2) {
        if (!args[1]->IsObject())
        {
            NanThrowTypeError("optional second arg must be an options object");
            NanReturnUndefined();
        }

        Local<Object> options = args[1].As<Object>();

        if (options->Has(NanNew("resolution")))
        {
            Local<Value> bind_opt = options->Get(NanNew("resolution"));
            if (!bind_opt->IsNumber())
            {
                NanThrowTypeError("'resolution' must be an Integer");
                NanReturnUndefined();
            }

            resolution = bind_opt->IntegerValue();
        }

        if (options->Has(NanNew("features")))
        {
            Local<Value> bind_opt = options->Get(NanNew("features"));
            if (!bind_opt->IsBoolean())
            {
                NanThrowTypeError("'features' must be an Boolean");
                NanReturnUndefined();
            }

            add_features = bind_opt->BooleanValue();
        }
    }

    try {

        Local<Array> grid_array = NanNew<Array>();
        std::vector<mapnik::grid::lookup_type> key_order;
        node_mapnik::grid2utf<mapnik::grid>(*g->get(),grid_array,key_order,resolution);

        // convert key order to proper javascript array
        Local<Array> keys_a = NanNew<Array>(key_order.size());
        std::vector<std::string>::iterator it;
        unsigned int i;
        for (it = key_order.begin(), i = 0; it < key_order.end(); ++it, ++i)
        {
            keys_a->Set(i, NanNew((*it).c_str()));
        }

        // gather feature data
        Local<Object> feature_data = NanNew<Object>();
        if (add_features) {
            node_mapnik::write_features<mapnik::grid>(*g->get(),
                                                      feature_data,
                                                      key_order
                );
        }

        // Create the return hash.
        Local<Object> json = NanNew<Object>();
        json->Set(NanNew("grid"), grid_array);
        json->Set(NanNew("keys"), keys_a);
        json->Set(NanNew("data"), feature_data);
        return json;

    }
    catch (std::exception const& ex)
    {
        NanThrowError(ex.what());
        NanReturnUndefined();
    }
}

// @TODO: convert this to EIO. It's currently doing all the work in the main
// thread, and just provides an async interface.
NAN_METHOD(Grid::encode) // format, resolution
{
    NanScope();

    Grid* g = node::ObjectWrap::Unwrap<Grid>(args.Holder());

    // defaults
    std::string format("utf");
    unsigned int resolution = 4;
    bool add_features = true;

    // accept custom format
    if (args.Length() >= 1){
        if (!args[0]->IsString())
        {
            NanThrowTypeError("first arg, 'format' must be a string");
            NanReturnUndefined();
        }
        format = TOSTR(args[0]);
    }

    // options hash
    if (args.Length() >= 2) {
        if (!args[1]->IsObject())
        {
            NanThrowTypeError("optional second arg must be an options object");
            NanReturnUndefined();
        }

        Local<Object> options = args[1]->ToObject();

        if (options->Has(NanNew("resolution")))
        {
            Local<Value> bind_opt = options->Get(NanNew("resolution"));
            if (!bind_opt->IsNumber())
            {
                NanThrowTypeError("'resolution' must be an Integer");
                NanReturnUndefined();
            }

            resolution = bind_opt->IntegerValue();
        }

        if (options->Has(NanNew("features")))
        {
            Local<Value> bind_opt = options->Get(NanNew("features"));
            if (!bind_opt->IsBoolean())
            {
                NanThrowTypeError("'features' must be an Boolean");
                NanReturnUndefined();
            }

            add_features = bind_opt->BooleanValue();
        }
    }

    // ensure callback is a function
    if (!args[args.Length()-1]->IsFunction())
    {
        NanThrowTypeError("last argument must be a callback function");
        NanReturnUndefined();
    }
    Local<Function> callback = args[args.Length() - 1].As<Function>();

    try {

        Local<Array> grid_array = NanNew<Array>();
        std::vector<mapnik::grid::lookup_type> key_order;
        node_mapnik::grid2utf<mapnik::grid>(*g->get(), grid_array, key_order, resolution);

        // convert key order to proper javascript array
        Local<Array> keys_a = NanNew<Array>(key_order.size());
        std::vector<std::string>::iterator it;
        unsigned int i;
        for (it = key_order.begin(), i = 0; it < key_order.end(); ++it, ++i)
        {
            keys_a->Set(i, NanNew((*it).c_str()));
        }

        // gather feature data
        Local<Object> feature_data = NanNew<Object>();
        if (add_features) {
            node_mapnik::write_features<mapnik::grid>(*g->get(),
                                                      feature_data,
                                                      key_order
                );
        }

        // Create the return hash.
        Local<Object> json = NanNew<Object>();
        json->Set(NanNew("grid"), grid_array);
        json->Set(NanNew("keys"), keys_a);
        json->Set(NanNew("data"), feature_data);

        Local<Value> argv[2] = { NanNull(), NanNew(json) };
        NanMakeCallback(NanGetCurrentContext()->Global(), callback, 2, argv);
    }
    catch (std::exception const& ex)
    {
        Local<Value> argv[1] = { NanError(ex.what()) };
        NanMakeCallback(NanGetCurrentContext()->Global(), callback, 1, argv);
    }

    NanReturnUndefined();
}

#endif
