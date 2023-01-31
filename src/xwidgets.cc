// oct_demo.cc -- example of a dynamically linked function for Octave.
//
// To compile this file, type the command
//
//   mkoctfile oct_demo.cc
//
// from within Octave or from the shell prompt.  This will create a file
// called oct_demo.oct that can be loaded by Octave.  To test the
// oct_demo.oct file, start Octave and type the command
//
//   oct_demo ("easy as", 1, 2, 3)
//
// at the Octave prompt.  Octave should respond by printing
//
//   Hello, world!
//   easy as
//   1
//   2
//   3
//   ans = 3
//
// Additional samples of code are in the examples directory of the Octave
// distribution.  See also the Octave manual
//
//   https://octave.org/doc/v5.2.0/External-Code-Interface.html

#include <iostream>
#include <string>

#include <octave/cdef-class.h>
#include <octave/cdef-manager.h>
#include <octave/cdef-method.h>
#include <octave/cdef-object.h>
#include <octave/cdef-property.h>
#include <octave/cdef-utils.h>
#include <octave/error.h>
#include <octave/interpreter.h>
#include <octave/oct.h>
#include <octave/ov-base.h>
#include <octave/ov-classdef.h>
#include <octave/ov-null-mat.h>
#include <octave/ov.h>
#include <octave/ovl.h>
#include <octave/parse.h>

#include <xwidgets/xcommon.hpp>

constexpr inline char const *XWIDGET_CLASS_NAME = "__xwidget_internal__";

class xwidget : public octave::handle_cdef_object, public xw::xcommon {

public:
  void put(std::string const &, octave_value const &) override;
  void mark_as_constructed(octave::cdef_class const &cls) override;

private:
  xwidget();
  ~xwidget();

  void open();
  void close();

  void serialize_state(nl::json &, xeus::buffer_sequence &) const;
  void apply_patch(nl::json const &, xeus::buffer_sequence const &);
  void handle_message(xeus::xmessage const &);
  void handle_custom_message(nl::json const &);

  /**
   * @brief call any observers set in the octave interpreter context for the
   * specified property name
   */
  void notify_backend(std::string const &);
  /**
   * @brief send to the frontend a new value for the specified property.
   * Octave value is automatically converted to a json value
   */
  void notify_frontend(std::string const &, octave_value const &);

public:
  static octave_value_list cdef_constructor(octave::interpreter &,
                                            octave_value_list const &, int);
  static octave_value_list cdef_observe(octave_value_list const &, int);
  static octave_value_list cdef_display(octave_value_list const &, int);
  static octave_value_list cdef_id(octave_value_list const &, int);
  static octave_value_list cdef_on(octave_value_list const &, int);

private:
  template <typename T>
  friend inline void xw::xwidgets_serialize(T const &value, nl::json &j,
                                            xeus::buffer_sequence &);

private:
  std::map<std::string, std::list<octave_value>> m_observerCallbacks;
  std::map<std::string, std::list<octave_value>> m_eventCallbacks;
};

xwidget *get_widget(octave_classdef const *obj) {
  octave::cdef_object const &ref =
      const_cast<octave_classdef *>(obj)->get_object_ref();
  octave::cdef_object_rep *rep =
      const_cast<octave::cdef_object_rep *>(ref.get_rep());

  return dynamic_cast<xwidget *>(rep);
}

namespace xw {

inline void xwidgets_serialize(octave_value const &ov, nl::json &j,
                               xeus::buffer_sequence &b);
inline void
xwidgets_deserialize(octave_value &ov, nl::json const &j,
                     xeus::buffer_sequence const &b = xeus::buffer_sequence());

namespace {

template <typename M>
inline void xwidgets_serialize_matrix_like(M const &mv, nl::json &j,
                                           xeus::buffer_sequence &b) {
  j = nl::json::array();

  for (octave_idx_type i = 0; i < mv.numel(); i++) {
    nl::json e;
    xwidgets_serialize(mv.elem(i), e, b);
    j.push_back(e);
  }
}

template <typename T>
inline void xwidgets_deserialize_matrix_like(octave_value &ov,
                                             nl::json const &j,
                                             xeus::buffer_sequence const &b) {
  T p(dim_vector(static_cast<octave_idx_type>(j.size()), 1));
  octave_idx_type i = 0;
  for (auto &e : j)
    xwidgets_deserialize(p(i++), e, b);
  ov = p;
}

inline void xwidgets_deserialize_object(octave_value &ov, nl::json const &j,
                                        xeus::buffer_sequence const &b) {
  octave_scalar_map p;
  for (auto &[key, val] : j.items()) {
    octave_value e;
    xwidgets_deserialize(e, val, b);
    p.assign(key, e);
  }
  ov = p;
}

} // namespace

inline void xwidgets_serialize(octave_classdef const &cdv, nl::json &j,
                               xeus::buffer_sequence &) {
  if (cdv.is_instance_of(XWIDGET_CLASS_NAME))
    j = "IPY_MODEL_" + std::string(get_widget(&cdv)->id());
  else
    warning("xwidget: cannot serialize classdef");
}

inline void xwidgets_serialize(Array<std::string> const &mv, nl::json &j,
                               xeus::buffer_sequence &b) {
  xwidgets_serialize_matrix_like(mv, j, b);
}

inline void xwidgets_serialize(Cell const &cv, nl::json &j,
                               xeus::buffer_sequence &b) {
  xwidgets_serialize_matrix_like(cv, j, b);
}

inline void xwidgets_serialize(octave_value const &ov, nl::json &j,
                               xeus::buffer_sequence &b) {
  if (ov.is_bool_scalar())
    xwidgets_serialize(ov.bool_value(), j, b);
  else if (ov.is_real_scalar())
    xwidgets_serialize(ov.scalar_value(), j, b);
  else if (ov.isinteger() && ov.is_scalar_type())
    xwidgets_serialize(ov.int64_value(), j, b);
  else if (ov.is_string())
    xwidgets_serialize(ov.string_value(), j, b);
  else if (ov.is_classdef_object())
    xwidgets_serialize(*ov.classdef_object_value(), j, b);
  else if (ov.iscell())
    xwidgets_serialize(ov.cell_value(), j, b);
  else if (ov.isnull())
    xwidgets_serialize(nullptr, j, b);
  else
    warning("xwidget: cannot serialize octave value %s", ov.type_name().data());
}

inline void xwidgets_deserialize(octave_value &ov, nl::json const &j,
                                 xeus::buffer_sequence const &b) {
  if (j.is_boolean())
    ov = j.get<bool>();
  else if (j.is_number_float())
    ov = j.get<double>();
  else if (j.is_number_integer())
    ov = octave_int64(j.get<int64_t>());
  else if (j.is_string())
    ov = j.get<std::string>();
  // No classdef at the moment
  else if (j.is_array())
    xwidgets_deserialize_matrix_like<Cell>(ov, j, b);
  else if (j.is_object())
    xwidgets_deserialize_object(ov, j, b);
  else if (j.is_null())
    ov = octave_null_matrix::instance;
  else
    warning("xwidget: cannot deserialize json value %s", j.type_name());
}

} // namespace xw

xwidget::xwidget() : octave::handle_cdef_object(), xw::xcommon() {
  this->comm().on_message(
      std::bind(&xwidget::handle_message, this, std::placeholders::_1));
}

xwidget::~xwidget() {
  std::clog << "Destructing " << get_class().get_name() << std::endl;
  this->close();
}

void xwidget::open() {
  // serialize state
  nl::json state;
  xeus::buffer_sequence buffers;
  this->serialize_state(state, buffers);

  // open comm
  xw::xcommon::open(std::move(state), std::move(buffers));
}

void xwidget::close() { xw::xcommon::close(); }

namespace {

/**
 * @brief Check if property should be synced with widget model in frontend
 * by looking for "Sync" attribute"
 *
 * The following must be present in classdef definition in .m file
 *
 * ...
 *   properties (Sync = true)
 *     _model_name = "ButtonModel";
 *     _view_name = "ButtonView";
 *
 *     description = "";
 *     tooltip = "";
 *   end
 * ...
 *
 * We can use a nonstandard attribute because Octave parses all attributes
 * of properties regardless of their "correctness".
 *
 * @param property reference to a property definition object
 * @return true if property has attribute "Sync" set to true
 */
inline bool is_sync_property(octave::cdef_property &property) {
  return !property.get("Sync").isempty() && property.get("Sync").bool_value();
}

}; // namespace

void xwidget::serialize_state(nl::json &state,
                              xeus::buffer_sequence &buffers) const {
  octave::cdef_class cls = this->get_class();
  auto properties = cls.get_property_map(octave::cdef_class::property_all);

  for (auto property_tuple : properties) {
    octave::cdef_property property = property_tuple.second;
    if (is_sync_property(property)) {
      octave_value ov = this->get(property_tuple.first);
      xw::xwidgets_serialize(ov, state[property_tuple.first], buffers);
    }
  }
}

void xwidget::apply_patch(nl::json const &state,
                          xeus::buffer_sequence const &buffers) {
  octave::cdef_class cls = this->get_class();
  auto properties = cls.get_property_map(octave::cdef_class::property_all);

  for (auto property_tuple : properties) {
    octave::cdef_property property = property_tuple.second;
    if (properties.count(property_tuple.first) && is_sync_property(property) &&
        state.contains(property_tuple.first)) {
      octave_value value;
      xw::xwidgets_deserialize(value, state[property_tuple.first], buffers);
      // Call superclass put to avoid notifying the view again in a loop
      octave::handle_cdef_object::put(property_tuple.first, value);
      this->notify_backend(property_tuple.first);
    }
  }
}

void xwidget::put(std::string const &pname, octave_value const &val) {
  octave::handle_cdef_object::put(pname, val);
  if (this->is_constructed()) // When default property values are being set
  {
    octave::cdef_class cls = this->get_class();
    auto properties = cls.get_property_map(octave::cdef_class::property_all);

    if (properties.count(pname) && is_sync_property(properties[pname])) {
      std::clog << "Notify change " << pname << std::endl;
      this->notify_frontend(pname, val);
      this->notify_backend(pname);
    }
  }
}

void xwidget::notify_frontend(std::string const &name,
                              octave_value const &value) {
  nl::json state;
  xeus::buffer_sequence buffers;
  xw::xwidgets_serialize(value, state[name], buffers);
  send_patch(std::move(state), std::move(buffers));
}

void xwidget::notify_backend(std::string const &pname) {
  if (this->m_observerCallbacks.count(pname)) {
    for (auto callback : this->m_observerCallbacks[pname]) {
      // Object reference
      octave::cdef_object obj(this->clone());
      octave::feval(callback, octave::to_ov(obj));
    }
  }
}

void xwidget::handle_message(xeus::xmessage const &message) {
  nl::json const &content = message.content();
  nl::json const &data = content["data"];
  const std::string method = data["method"];

  if (method == "update") {
    nl::json const &state = data["state"];
    auto const &buffers = message.buffers();
    nl::json const &buffer_paths = data["buffer_paths"];
    this->hold() = std::addressof(message);
    xw::insert_buffer_paths(const_cast<nl::json &>(state), buffer_paths);
    this->apply_patch(state, buffers);
    this->hold() = nullptr;
  } else if (method == "request_state") {
    nl::json state;
    xeus::buffer_sequence buffers;
    this->serialize_state(state, buffers);
    send_patch(std::move(state), std::move(buffers));
  } else if (method == "custom") {
    auto it = data.find("content");
    if (it != data.end()) {
      this->handle_custom_message(it.value());
    }
  }
}

void xwidget::handle_custom_message(nl::json const &jsonmessage) {
  auto meth = this->get_class().find_method("handle_custom_message");

  if (meth.ok()) {
    octave_value message;
    xw::xwidgets_deserialize(message, jsonmessage);
    octave::cdef_object obj(this->clone());
    meth.execute(obj, ovl(message), 0);
  } else if (jsonmessage.contains("event")) {
    std::string event = jsonmessage["event"];
    if (this->m_eventCallbacks.count(event)) {
      for (auto callback : this->m_eventCallbacks[event]) {
        // Object reference
        octave::cdef_object obj(this->clone());
        octave::feval(callback, octave::to_ov(obj));
      }
    }
  }
}

void xwidget::mark_as_constructed(octave::cdef_class const &cls) {
  octave::handle_cdef_object::mark_as_constructed(cls);

  if (m_ctor_list.empty())
    // Open the comm
    this->open();
}

octave_value_list xwidget::cdef_constructor(octave::interpreter &interpreter,
                                            octave_value_list const &args,
                                            int) {
  // Get a reference to the old object
  octave::cdef_object &obj = args(0).classdef_object_value()->get_object_ref();
  // Retrieve the class we want to construct
  octave::cdef_class cls = obj.get_class();

  if (get_widget(args(0).classdef_object_value()) == nullptr) {
    std::clog << "Inject xwidget into " << cls.get_name() << std::endl;

    // Create a new object with our widget rep
    xwidget *wdg = new xwidget();
    octave::cdef_object new_obj(wdg);
    // Set it to the new object
    new_obj.set_class(cls);
    // Initialize the properties
    cls.initialize_object(new_obj);
    // Construct superclasses (only handle)
    interpreter.get_cdef_manager().find_class("handle").run_constructor(new_obj,
                                                                        ovl());
    // Replace the old object
    obj = new_obj;

    return ovl(octave::to_ov(new_obj));
  } else // If the object rep has already been substituted with an xwidget (this
         // will happen with multiple inheritance)
  {
    std::clog << "No need to inject xwidget into " << cls.get_name()
              << std::endl;

    return ovl(args(0));
  }
}

octave_value_list xwidget::cdef_observe(octave_value_list const &args, int) {
  // Object reference
  octave_classdef *obj = args(0).classdef_object_value();
  // Property to observe
  std::string pname =
      args(1).xstring_value("PNAME must be a string with the property name");
  // Observer callback
  octave_value fcn = args(2);

  if (!fcn.is_function_handle())
    error("HANDLE must be a function handle");

  get_widget(obj)->m_observerCallbacks[pname].push_back(fcn);

  return ovl();
}

octave_value_list xwidget::cdef_display(octave_value_list const &args, int) {
  get_widget(args(0).classdef_object_value())->display();
  return ovl();
}

octave_value_list xwidget::cdef_id(octave_value_list const &args, int) {
  return ovl(std::string(get_widget(args(0).classdef_object_value())->id()));
}

octave_value_list xwidget::cdef_on(octave_value_list const &args, int) {
  // Object reference
  octave_classdef *obj = args(0).classdef_object_value();
  // Property to observe
  std::string event =
      args(1).xstring_value("EVENT must be a string with the event name");
  // Observer callback
  octave_value fcn = args(2);

  if (!fcn.is_function_handle())
    error("HANDLE must be a function handle");

  get_widget(obj)->m_eventCallbacks[event].push_back(fcn);

  return ovl();
}

DEFMETHOD_DLD(xwidgets_load, interpreter, args, /* nargout */, "") {
  octave::cdef_manager &cm = interpreter.get_cdef_manager();
  octave::cdef_class cls =
      cm.make_class(XWIDGET_CLASS_NAME, cm.find_class("handle"));

  cls.install_method(
      cm.make_method(cls, XWIDGET_CLASS_NAME, xwidget::cdef_constructor));
  cls.install_method(cm.make_method(cls, "observe", xwidget::cdef_observe));
  cls.install_method(cm.make_method(cls, "display", xwidget::cdef_display));
  cls.install_method(cm.make_method(cls, "id", xwidget::cdef_id));
  cls.install_method(cm.make_method(cls, "on", xwidget::cdef_on));

  interpreter.get_symbol_table().install_built_in_function(
      XWIDGET_CLASS_NAME, cls.get_constructor_function());

  return ovl();
}
