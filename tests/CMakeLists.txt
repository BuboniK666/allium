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
  set(CUDA_SOURCES cuda.cu)
endif()

add_executable(test_suite
  cg.cpp
  eigen.cpp
  explicit_integrator.cpp
  gmres.cpp
  hash.cpp
  imex_euler.cpp
  local_matrix.cpp
  local_mesh.cpp
  local_vector.cpp
  main.cpp
  numeric.cpp
  petsc_mesh.cpp
  point.cpp
  polynomial.cpp
  range.cpp
  sparse_matrix.cpp
  vector_storage.cpp
  ${CUDA_SOURCES}
  )
target_link_libraries(test_suite ${ALLIUM_LIBRARIES} ${GTEST_LIBRARIES} ${LIBRARIES})

