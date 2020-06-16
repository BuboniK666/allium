#ifndef CHIVE_LA_PETSC_OBJECT_PTR_HPP
#define CHIVE_LA_PETSC_OBJECT_PTR_HPP

#include <chive/config.hpp>
#include <petscmat.h>
#include <petscvec.h>
#include <petscksp.h>
#include <petscpc.h>
#include <petscts.h>

#ifdef USE_SLEPC
#include <slepceps.h>
#endif

// make a list of objects castable to PetscObject such that we do not
// accidentally cast other operators
template <typename T>
struct is_petsc_object : std::false_type {};

template<> struct is_petsc_object<Mat> : std::true_type {};
template<> struct is_petsc_object<Vec> : std::true_type {};
template<> struct is_petsc_object<KSP> : std::true_type {};
template<> struct is_petsc_object<PC>  : std::true_type {};
template<> struct is_petsc_object<TS>  : std::true_type {};
#ifdef USE_SLEPC
template<> struct is_petsc_object<EPS> : std::true_type {};
#endif

template <typename T>
PetscObject petsc_object_cast(T value) {
  // check that we actually have a PETSc type
  static_assert(is_petsc_object<T>::value,
                "Can only cast PETSc types to PETSc objects.");
  return reinterpret_cast<PetscObject>(value);
}

/** Petsc Wrapper Object for memory management.
 * Takes care of increasing and decreasing reference counts.
 */
template <typename T>
class PetscObjectPtr {
  public:
    static_assert(is_petsc_object<T>::value,
                  "PetscObjectPtr can only hold PETSc objects.");

    PetscObjectPtr() {
      value = nullptr;
    }

    PetscObjectPtr(const PetscObjectPtr& other) {
      PetscObjectReference(petsc_object_cast(other.value));
      this->value = other.value;
    }

    PetscObjectPtr(PetscObjectPtr&& other) {
      this->value = other.value;
      other.value = nullptr;
    }

    PetscObjectPtr(T new_value, bool adopt) {
      value = nullptr;
      set(new_value, adopt);
    }

    virtual ~PetscObjectPtr() {
      release();
    }

    operator T () const {
      return get();
    }

    PetscObjectPtr& operator= (const PetscObjectPtr& other) {
      set(other.value, false);
      return *this;
    }

    /** Decrease the reference count of the PETSc object by one and set value
     * to nullptr. */
    void release() {
      if (value != nullptr) {
        PetscObjectDereference(petsc_object_cast(value));
        value = nullptr;
      }
    }

    /** Gives up the ownership of value without decreasing the reference
     * count. Returns value. */
    T abandon() {
      T result = value;
      value = nullptr;
      return result;
    }

    void set(T new_value, bool adopt) {
      if (!adopt) {
        // It is important to increase the reference count first, otherwise we
        // might free new_value by directly or indirectly releasing it.
        PetscObjectReference(petsc_object_cast(new_value));
      }
      release();
      value = new_value;
    }

    /** A pointer to set the value of the PetscObjectPtr.
     * This releases the object and returns a pointer that can be used to set
     * a new value. The pointer might not be used or set to null. In case a
     * new values is provided, PetscObjectPtr adopts the referenced object, i.e., it
     * will decrease its reference count when going out of scope. */
    T* writable_ptr() {
      release();
      return &value;
    }

    T get() const {
      return value;
    }

    friend void swap(PetscObjectPtr<T> &a, PetscObjectPtr<T> &b){
      T v = a.value;
      a.value = b.value;
      b.value = v;
    }

  protected:
    T value;
};

#endif
