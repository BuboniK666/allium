// Copyright 2020 Hannah Rittich
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CHIVE_MPI_COMM_HPP
#define CHIVE_MPI_COMM_HPP

#include <mpi.h>
#include <vector>

namespace chive {
  class MpiComm {
    public:
      MpiComm(::MPI_Comm handle);

      bool operator!= (const MpiComm& other) {
        return handle != other.handle;
      }

      static MpiComm world();

      int get_rank();
      int get_size();

      void barrier(void);

      std::vector<long long> sum_exscan(std::vector<long long> buf);

      MPI_Comm get_handle() { return handle; }
    private:
      MPI_Comm handle;
  };
}

#endif
