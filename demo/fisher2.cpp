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

#include <allium/main/init.hpp>
#include <allium/mesh/petsc_mesh_spec.hpp>
#include <allium/mesh/petsc_mesh.hpp>
#include <allium/ipc/comm.hpp>
#include <allium/util/memory.hpp>
#include <allium/ode/imex_euler.hpp>
#include <iomanip>

using namespace allium;

using Number = PetscScalar;
using Real = real_part_t<Number>;

/** Stores the problem specific parameters. */
struct Problem {
  double alpha;
  double beta;
  double h;
};

/**
  The exact analytic solution of the problem.
  The solution for this particular case is known, hence, we can check the
  correctness of out code.

  The exact solution we are using there is a generalization to 2d of the
  solution derived in [Malfliet, 1992],
  @f[
    u(x,t) = (1/4)\{1-\tanh[(1/2\sqrt{6})(x-(5/\sqrt{6})t)]\}^2
    \,.
  @f]

  Malfliet, W. 1992. “Solitary Wave Solutions of Nonlinear Wave Equations.”
  American Journal of Physics, American Journal of Physics, 60 (7): 650–54.
  https://doi.org/10.1119/1.17120.
*/
double exact_solution(Problem pb, double t, double x, double y)
{
  double norm = sqrt(pb.alpha*pb.alpha + pb.beta*pb.beta);
  double alpha = pb.alpha / norm;
  double beta = pb.beta / norm;

  double r = alpha * x + beta * y;

  double gamma = (1 - tanh((1.0/(2*sqrt(6)))*(r-(5.0/sqrt(6))*t)));
  return (1.0/4) * gamma * gamma;
}

/**
  Set the boundary values of the mesh to zero.
*/
void zero_boundary(PetscLocalMesh<2>& mesh)
{
  // the range of the whole mesh
  auto global_range = mesh.mesh_spec()->range();

  // the range associated to the current processor
  auto range = mesh.mesh_spec()->local_ghost_range();

  // Access the local data of the mesh.
  // Note, this can be a costly operation in the case that the data has to
  // be transferred from an accelerator.
  auto lmesh = local_mesh(mesh);

  // Iterate over all mesh points
  for (auto p : range) {
    if (p[0] == -1
        || p[1] == -1
        || p[0] == global_range.end_pos()[0]
        || p[1] == global_range.end_pos()[1]) {
      lmesh(p[0], p[1]) = 0;
    }
  }
}

/** Add the contribution of the boundary points when applying the Laplace
 operator to the given vector. */
void add_boundary(PetscMesh<2>& mesh, Problem pb, double t)
{
  // the range of the whole mesh
  auto global_range = mesh.mesh_spec()->range();

  // the range associated to the current processor
  auto range = mesh.mesh_spec()->local_range();

  // Access the local data of the mesh.
  // Note, this can be a costly operation in the case that the data has to
  // be transferred from an accelerator.
  auto lmesh = local_mesh(mesh);

  double h = pb.h;

  // Iterate over all mesh points
  for (auto p : range) {
    if (p[0] == 0) { // top boundary
      double x = -1.0 * h;
      double y = p[1] * h;
      lmesh(p[0], p[1]) += (1.0 / (h*h)) * exact_solution(pb, t, x, y);
    }
    if (p[1] == 0) { // left boundary
      double x = p[0] * h;
      double y = -1 * h;
      lmesh(p[0], p[1]) += (1.0 / (h*h)) * exact_solution(pb, t, x, y);
    }
    if (p[0] == global_range.end_pos()[0]-1) { // bottom boundary
      double x = global_range.end_pos()[0] * h;
      double y = p[1] * h;
      lmesh(p[0], p[1]) += (1.0 / (h*h)) * exact_solution(pb, t, x, y);
    }
    if (p[1] == global_range.end_pos()[1]-1) { // right boundary
      double x = p[0] * h;
      double y = global_range.end_pos()[1] * h;
      lmesh(p[0], p[1]) += (1.0 / (h*h)) * exact_solution(pb, t, x, y);
    }
  }
}

/** Set the mesh values to the exact solution. */
void set_solution(PetscMesh<2>& result, Problem pb, double t)
{
  auto range = result.mesh_spec()->local_range();

  auto lresult = local_mesh(result);
  for (auto p : range) {
    double x = pb.h * p[0];
    double y = pb.h * p[1];
    lresult(p[0], p[1]) = exact_solution(pb, t, x, y);
  }
}

/** Solves F(t, y, a*y + p) = q, where F(t, y, z) = z - Δy */
auto make_f_impl(Problem pb) {
  return
    [=](PetscMesh<2>& y, Real t, Number a, const PetscMesh<2>& p, const PetscMesh<2>& q)
    {
      auto op = [a,pb](PetscMesh<2>& f, const PetscMesh<2>& u) {
        // PETSc require to create a "local mesh" to have access to the ghost
        // nodes.
        PetscLocalMesh<2> u_aux(u.mesh_spec());
        u_aux.assign(u); // copy to determine the ghost nodes

        // We set the boundary to zero such that we can apply the same stencil
        // everywhere (also at the boundary).
        zero_boundary(u_aux);

        auto range = u.mesh_spec()->local_range();
        auto lu = local_mesh(u_aux);
        auto lf = local_mesh(f);

        // Apply the stencil
        //         |    -1     |
        // (1/h^2) | -1  4  -1 |
        //         |    -1     |h
        for (auto p : range) {
          lf(p[0], p[1])
            = (1.0 / (pb.h*pb.h))
              * ( (4+a*(pb.h*pb.h)) * lu(p[0],   p[1])
                  - lu(p[0]-1, p[1])
                  - lu(p[0],   p[1]-1)
                  - lu(p[0],   p[1]+1)
                  - lu(p[0]+1, p[1]));
        }
      };

      // rhs = q - p
      PetscMesh<2> rhs(p.mesh_spec());
      rhs.assign(q);
      rhs.add_scaled(-1.0, p);
      add_boundary(rhs, pb, t);

      // solve (-Δ + aI) y = q - p
      CgSolver<PetscMesh<2>> solver;
      solver.setup(shared_copy(make_linear_operator<PetscMesh<2>>(op)));
      solver.solve(y, rhs);
    };
};

/** The explicit part of the ODE, G(y) = y*(1-y) */
void f_expl(PetscMesh<2>& result, Real t, const PetscMesh<2>& u)
{
  auto range = u.mesh_spec()->local_range();
  auto lresult = local_mesh(result);
  auto lu = local_mesh(u);

  for (auto p : range) {
    lresult(p[0], p[1]) = lu(p[0], p[1]) * (1.0 - lu(p[0], p[1]));
  }
}

/** Print the mesh on STDOUT, sorted by MPI ranks. */
template <typename M>
void print_mesh(const M& mesh) {
  auto comm = mesh.mesh_spec()->comm();

  for (int i=0; i < comm.size(); ++i) {
    if (i == comm.rank()) {
      std::cout << "rank " << comm.rank() << std::endl;

      auto lmesh = local_mesh(mesh);
      auto range = mesh.local_range();
      for (int i = range.begin_pos()[0]; i < range.end_pos()[0]; i++) {
        for (int j = range.begin_pos()[1]; j < range.end_pos()[1]; ++j) {
          std::cout << std::setw(10) << lmesh(i, j) << " ";
        }
        std::cout << std::endl;
      }

    }

    comm.barrier();
  }
}

int main(int argc, char** argv)
{
  using namespace std::placeholders;

  Init init(argc, argv);  // initialize Allium

  const int N = 10;
  double h = 1.0 / (N-1);

  auto comm = Comm::world();

  if (comm.rank() == 0)
    std::cout << "Fisher 2D solver" << std::endl;

  // Create a mesh. For description of the parameters, see the PETSc manual
  auto spec = std::shared_ptr<PetscMeshSpec<2>>(
                new PetscMeshSpec<2>(
                  comm,
                  {DM_BOUNDARY_GHOSTED, DM_BOUNDARY_GHOSTED},
                  DMDA_STENCIL_STAR,
                  {N, N}, // global size
                  {PETSC_DECIDE, PETSC_DECIDE}, // processors per dim
                  1, // ndof
                  1)); // stencil_width

  PetscMesh<2> u(spec);
  PetscMesh<2> error(spec);

  Problem pb = { 1.0, 0.5, h };

  // setup the integrator
  ImexEuler<PetscMesh<2>> integrator;
  integrator.setup(f_expl, make_f_impl(pb));

  double t0 = 0;
  double t1 = 1;
  set_solution(u, pb, t0);
  integrator.initial_values(t0, u);
  integrator.dt(0.01);

  print_mesh(u);

  integrator.integrate(u, t1);

  print_mesh(u);

  // error = exact - u
  set_solution(error, pb, t1);
  error.add_scaled(-1.0, u);

  std::cout << "‖e‖ = " << error.l2_norm() << std::endl;

  return 0;
}

