/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <ostream>

#include <boost/optional.hpp>
#include <utility>

#include "AbstractDomain.h"
#include "ConstantAbstractDomain.h"
#include "DexClass.h"
#include "IRInstruction.h"
#include "PatriciaTreeMapAbstractEnvironment.h"
#include "PatriciaTreeMapAbstractPartition.h"

namespace reflection {

namespace impl {

// Forward declarations.
class Analyzer;

} // namespace impl

/*
 * This analysis performs an intraprocedural constant propagation on a variety
 * of objects involved in reflective calls. It is aimed at exposing simple yet
 * common use cases of reflection.
 *
 * Example:
 *
 *   Bar bar_obj = new Bar();                          --> OBJECT(Bar)
 *   java.lang.Class foo_class = Foo.class;            --> CLASS(Foo)
 *   java.lang.Class bar_class = bar_obj.getClass();   --> CLASS(Bar)
 *   Field f = foo_class.getField("foo");              --> FIELD(Foo, "foo")
 *   Method m = bar_class.getMethod("bar");            --> METHOD(Bar, "bar")
 *   String s1 = f.getName();                          --> STRING("foo")
 *   String s2 = m.getName();                          --> STRING("bar")
 *   java.lang.Class baz_class = Class.forName("Baz"); --> CLASS(Baz)
 *
 * Note that the signature of a method is not tracked by the analysis. In the
 * example above, f may refer to any field named "foo" in the class `Foo`.
 */

/*
 * The first three are inputs of an reflecting operation.
 * The last three are output of an reflecting operation.
 */
/* clang-format off */
 enum AbstractObjectKind {
   OBJECT, // An object instantiated locally, passed in as a param or read from
           // heap
   INT,    // An integer literal
   STRING, // A string literal
   CLASS,  // A java.lang.Class object
   FIELD,  // A java.lang.reflect.Field object
   METHOD, // A java.lang.reflect.Method object
 };

 /*
  * Only applies to AbstractObjectKind.CLASS.
  * By what kind of operation the class object is produced.
  */
 enum ClassObjectSource {
   NON_REFLECTION, // Non-reflecting operations like param loading and get field.
   REFLECTION,     // Reflection operations like const-class or Class.forName().
 };

/* clang-format on */

using AbstractHeapAddress = uint64_t;

struct AbstractObject final : public sparta::AbstractValue<AbstractObject> {
  AbstractObjectKind obj_kind;
  DexType* dex_type;
  DexString* dex_string;
  boost::optional<int64_t> dex_int = boost::none;
  // Attaching a set of potential dex types.
  std::unordered_set<DexType*> potential_dex_types;

  // for objects of Class[] type, we model the heap stored array with a
  // HeapClassArrayEnvironment
  AbstractHeapAddress heap_address = 0;

  // dex_type_array is present when obj_kind is method
  // Because overloaded methods are essentially different methods, we need to
  // use the parameter types to precisely know the exact methods. In order to
  // do that, we need to do 2 things:
  // 1. We need to store information of arrays of Class objects.
  // 2. Associate the parameterType argument of getDeclaredMethod to the Class
  // arrays.
  boost::optional<std::vector<DexType*>> dex_type_array = boost::none;

  // AbstractObject must be default constructible in order to be used as an
  // abstract value.
  AbstractObject() = default;

  explicit AbstractObject(DexString* s)
      : obj_kind(STRING), dex_type(nullptr), dex_string(s) {}

  explicit AbstractObject(int64_t i)
      : obj_kind(INT), dex_type(nullptr), dex_string(nullptr), dex_int(i) {}

  AbstractObject(AbstractObjectKind k, DexType* t)
      : obj_kind(k), dex_type(t), dex_string(nullptr) {
    always_assert(k == OBJECT || k == CLASS);
  }

  AbstractObject(AbstractObjectKind k,
                 DexType* t,
                 std::unordered_set<DexType*> p)
      : obj_kind(k),
        dex_type(t),
        dex_string(nullptr),
        potential_dex_types(std::move(p)) {
    always_assert(k == OBJECT || k == CLASS);
  }

  AbstractObject(AbstractObjectKind k, DexType* t, DexString* s)
      : obj_kind(k), dex_type(t), dex_string(s) {
    always_assert(k == FIELD || k == METHOD);
  }

  AbstractObject(AbstractObjectKind k,
                 DexType* t,
                 DexString* s,
                 std::unordered_set<DexType*> p)
      : obj_kind(k),
        dex_type(t),
        dex_string(s),
        potential_dex_types(std::move(p)) {
    always_assert(k == FIELD || k == METHOD);
  }

  AbstractObject(AbstractObjectKind k, AbstractHeapAddress addr)
      : obj_kind(k),
        dex_type(DexType::make_type("[Ljava/lang/Class;")),
        dex_string(nullptr) {
    heap_address = addr;
    always_assert(k == OBJECT);
  }

  void add_potential_dex_type(DexType* type) {
    potential_dex_types.insert(type);
  }

  bool is_known_class_array() const {
    const auto class_array_type =
        DexType::get_type(DexString::make_string("[Ljava/lang/Class;"));
    return this->obj_kind == OBJECT && this->dex_type == class_array_type &&
           heap_address != 0;
  }

  void clear() override {}

  sparta::AbstractValueKind kind() const override {
    return sparta::AbstractValueKind::Value;
  }

  bool leq(const AbstractObject& other) const override;

  bool equals(const AbstractObject& other) const override;

  sparta::AbstractValueKind join_with(const AbstractObject& other) override;

  sparta::AbstractValueKind widen_with(const AbstractObject& other) override {
    return join_with(other);
  }

  sparta::AbstractValueKind meet_with(const AbstractObject& other) override;

  sparta::AbstractValueKind narrow_with(const AbstractObject& other) override {
    return meet_with(other);
  }
};

class AbstractObjectDomain final
    : public sparta::AbstractDomainScaffolding<AbstractObject,
                                               AbstractObjectDomain> {
 public:
  AbstractObjectDomain() { this->set_to_top(); }
  explicit AbstractObjectDomain(AbstractObject obj) {
    this->set_to_value(AbstractObject(std::move(obj)));
  }
  explicit AbstractObjectDomain(sparta::AbstractValueKind kind)
      : sparta::AbstractDomainScaffolding<AbstractObject, AbstractObjectDomain>(
            kind) {}

  boost::optional<AbstractObject> get_object() const {
    return (this->kind() == sparta::AbstractValueKind::Value)
               ? boost::optional<AbstractObject>(*this->get_value())
               : boost::none;
  }
};

bool operator==(const AbstractObject& x, const AbstractObject& y);

bool operator!=(const AbstractObject& x, const AbstractObject& y);

bool is_not_reflection_output(const AbstractObject& obj);

using ReflectionAbstractObject =
    std::pair<AbstractObject, boost::optional<ClassObjectSource>>;

using ReflectionSites = std::vector<
    std::pair<IRInstruction*, std::map<reg_t, ReflectionAbstractObject>>>;

using param_index_t = uint16_t;

using CallingContext =
    sparta::PatriciaTreeMapAbstractPartition<param_index_t,
                                             AbstractObjectDomain>;

// Maps from callsite instruction to the corresponding calling context.
using CallingContextMap =
    sparta::PatriciaTreeMapAbstractEnvironment<const IRInstruction*,
                                               CallingContext>;

using SummaryQueryFn = std::function<AbstractObjectDomain(const DexMethod*)>;

class ReflectionAnalysis final {
 public:
  // If we don't declare a destructor for this class, a default destructor will
  // be generated by the compiler, which requires a complete definition of
  // sra_impl::Analyzer, thus causing a compilation error. Note that the
  // destructor's definition must be located after the definition of
  // sra_impl::Analyzer.
  ~ReflectionAnalysis();

  explicit ReflectionAnalysis(DexMethod* dex_method,
                              CallingContext* context = nullptr,
                              SummaryQueryFn* summary_query_fn = nullptr);

  ReflectionSites get_reflection_sites() const;

  AbstractObjectDomain get_return_value() const;

  /**
   * Return a parameter type array for this invoke method instruction.
   */
  boost::optional<std::vector<DexType*>> get_method_params(
      IRInstruction* invoke_insn) const;

  bool has_found_reflection() const;

  /*
   * Returns the abstract object (if any) referenced by the register at the
   * given instruction. Note that if the instruction overwrites the register,
   * the abstract object returned is the value held by the register *before*
   * that instruction is executed.
   */
  boost::optional<AbstractObject> get_abstract_object(
      size_t reg, IRInstruction* insn) const;

  boost::optional<ClassObjectSource> get_class_source(
      size_t reg, IRInstruction* insn) const;

  CallingContextMap get_calling_context_partition() const;

 private:
  const DexMethod* m_dex_method;
  std::unique_ptr<impl::Analyzer> m_analyzer;

  void get_reflection_site(
      const reg_t reg,
      IRInstruction* insn,
      std::map<reg_t, ReflectionAbstractObject>* abstract_objects) const;
};

} // namespace reflection

std::ostream& operator<<(std::ostream& out,
                         const reflection::AbstractObject& x);

std::ostream& operator<<(std::ostream& out,
                         const reflection::ClassObjectSource& cls_src);

std::ostream& operator<<(std::ostream& out,
                         const reflection::ReflectionAbstractObject& aobj);
