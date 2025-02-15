#  Copyright 2020 Hannah Rittich
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

if(ALLIUM_USE_CUDA)
  set(CUDA_SOURCES
    cuda_vector.cu cuda_vector.hpp
    cuda_util.cu cuda_util.hpp)
endif()

add_library(allium_la
  cg.cpp cg.impl.hpp cg.hpp
  eigen_sparse_matrix.hpp
  eigen_vector.cpp eigen_vector.impl.hpp eigen_vector.hpp
  gmres.cpp gmres.impl.hpp gmres.hpp
  iterative_solver.hpp
  linear_operator.hpp
  local_coo_matrix.hpp
  local_vector.cpp local_vector.hpp
  petsc_object_ptr.hpp
  petsc_sparse_matrix.cpp petsc_sparse_matrix.hpp
  petsc_util.cpp petsc_util.hpp
  petsc_vector.cpp petsc_vector.hpp
  sparse_matrix.hpp
  txt_io.cpp txt_io.hpp
  vector_spec.cpp vector_spec.hpp
  vector_storage.cpp vector_storage.impl.hpp vector_storage.hpp
  vector_trait.hpp
  ${CUDA_SOURCES}
  )

set_target_properties(allium_la
                      PROPERTIES CUDA_SEPARABLE_COMPILATION ON)

list(APPEND ALLIUM_LIBRARIES allium_la)
set(ALLIUM_LIBRARIES ${ALLIUM_LIBRARIES} PARENT_SCOPE)
