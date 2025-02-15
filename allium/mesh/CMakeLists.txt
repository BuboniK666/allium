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
      cuda_mesh.cu cuda_mesh.hpp
      cuda_narray.cu cuda_narray.hpp)
endif()

add_library(allium_mesh
  petsc_mesh.cpp petsc_mesh.hpp
  petsc_mesh_spec.cpp petsc_mesh_spec.hpp
  point.hpp
  regular_mesh.hpp
  vtk_io.cpp vtk_io.hpp
  ${CUDA_SOURCES}
  )

list(APPEND ALLIUM_LIBRARIES allium_mesh)
set(ALLIUM_LIBRARIES ${ALLIUM_LIBRARIES} PARENT_SCOPE)
