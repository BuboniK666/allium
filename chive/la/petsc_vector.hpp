#ifndef CHIVE_LA_PETSC_VECTOR_HPP
#define CHIVE_LA_PETSC_VECTOR_HPP

#include <chive/config.hpp>

#ifdef CHIVE_USE_PETSC

#include "petsc_object_ptr.hpp"
#include <petscvec.h>
#include "vector.hpp"

namespace chive {
  class PetscVectorStorage final
      : public VectorStorageBase<PetscVectorStorage, PetscScalar>
  {
    public:
      PetscVectorStorage(VectorSpec spec);

      void add(const VectorStorage& rhs) override;
      void scale(const Number& factor) override;
      Number dot(const VectorStorage<Number>& rhs) override;
      Real l2_norm() const override;

      PetscObjectPtr<Vec> native() const { return ptr; }
    protected:
      Number* aquire_data_ptr() override;
      void release_data_ptr(Number* data) override;
    private:
      PetscObjectPtr<Vec> ptr;
  };

  using PetscVector = VectorBase<PetscVectorStorage>;
}

#endif
#endif
