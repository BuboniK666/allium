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
#include <allium/mesh/vtk_io.hpp>
#include <allium/ipc/comm.hpp>
#include <allium/util/memory.hpp>
#include <allium/ode/imex_euler.hpp>
#include <sstream>
#include <iostream>
#include <iomanip>

using namespace allium;

using Number = double;
using Real = real_part_t<Number>;
using Mesh = PetscMesh<double, 2>;
using LocalMesh = PetscLocalMesh<double, 2>;

/** Stores the problem specific parameters. */
class Fisher {
  public:
    Fisher(int N, double alpha, double beta);

    void simulate();

  private:
    const global_size_t m_N;
    const double m_h;
    const double m_alpha;
    const double m_beta;

    double exact_solution(double t, double x, double y);

    void zero_boundary(::LocalMesh& mesh);

    void add_boundary(Mesh& mesh, double t);

    void set_solution(Mesh& result, double t);

    void apply_shifted_laplace(Mesh& f, Number a, const Mesh& u);

    void solve_f_impl(Mesh& y,
                      Real t,
                      Number a,
                      const Mesh& r,
                      InitialGuess initial_guess);

    static void f_expl(Mesh& result, Real t, const Mesh& u);
};

int main(int argc, char** argv)
{
  Init init(argc, argv);  // initialize Allium

  Fisher problem(64, 1.0, 0.5);
  problem.simulate();

  return 0;
}

Fisher::Fisher(int N, double alpha, double beta)
  : m_N(N),
    m_h(20.0 / (N-1)),
    m_alpha(alpha),
    m_beta(beta)
{}

void Fisher::simulate() {
  using namespace std::placeholders;

  auto comm = Comm::world();

  if (comm.rank() == 0)
    std::cout << "Fisher 2D solver" << std::endl;

  // Create a mesh. For description of the parameters, see the PETSc manual
  auto spec = std::shared_ptr<PetscMeshSpec<2>>(
                new PetscMeshSpec<2>(
                  comm,
                  {DM_BOUNDARY_GHOSTED, DM_BOUNDARY_GHOSTED},
                  DMDA_STENCIL_STAR,
                  {m_N, m_N}, // global size
                  {PETSC_DECIDE, PETSC_DECIDE}, // processors per dim
                  1, // ndof
                  1)); // stencil_width

  Mesh u(spec);
  Mesh error(spec);

  // setup the integrator
  ImexEuler<Mesh> integrator;
  integrator.setup(f_expl,
                   [](Mesh& out, double t, const Mesh& in) {}, // not needed for Euler
                   std::bind(&Fisher::solve_f_impl, this, _1, _2, _3, _4, _5));

  double t0 = 0;
  set_solution(u, t0);
  integrator.initial_value(t0, u);
  integrator.dt(0.01);

  auto filename = [](int frame) {
    std::stringstream s;
    s << "mesh_" << frame << ".pvti";
    return s.str();
  };

  write_vtk(filename(0), u);

  for (int i = 0; i < 200; ++i) {
    double t1 = (i+1)*0.1;
    integrator.integrate(t1);

    write_vtk(filename(i+1), integrator.current_value());

    // error = exact - u
    set_solution(error, t1);
    error.add_scaled(-1.0, integrator.current_value());

    auto e = error.l2_norm();
    if (comm.rank() == 0) {
      std::cout << "t = " << t1 << ", ‖e‖ = " << e << std::endl;
    }
  }
}

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
double Fisher::exact_solution(double t, double x, double y)
{
  double norm = sqrt(m_alpha*m_alpha + m_beta*m_beta);
  double alpha = m_alpha / norm;
  double beta = m_beta / norm;

  double r = alpha * x + beta * y;

  double gamma = (1 - tanh((1.0/(2*sqrt(6)))*(r-(5.0/sqrt(6))*t)));
  return (1.0/4) * gamma * gamma;
}

/**
  Set the boundary values of the mesh to zero.
*/
void Fisher::zero_boundary(::LocalMesh& mesh)
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
void Fisher::add_boundary(Mesh& mesh, double t)
{
  // the range of the whole mesh
  auto global_range = mesh.mesh_spec()->range();

  // the range associated to the current processor
  auto range = mesh.mesh_spec()->local_range();

  // Access the local data of the mesh.
  // Note, this can be a costly operation in the case that the data has to
  // be transferred from an accelerator.
  auto lmesh = local_mesh(mesh);

  const auto h = m_h;

  // Iterate over all mesh points
  for (auto p : range) {
    if (p[0] == 0) { // left boundary
      double x = -1.0 * h;
      double y = p[1] * h;
      lmesh(p[0], p[1]) += (1.0 / (h*h)) * exact_solution(t, x, y);
    }
    if (p[1] == 0) { // top boundary
      double x = p[0] * h;
      double y = -1 * h;
      lmesh(p[0], p[1]) += (1.0 / (h*h)) * exact_solution(t, x, y);
    }
    if (p[0] == global_range.end_pos()[0]-1) { // right boundary
      double x = global_range.end_pos()[0] * h;
      double y = p[1] * h;
      lmesh(p[0], p[1]) += (1.0 / (h*h)) * exact_solution(t, x, y);
    }
    if (p[1] == global_range.end_pos()[1]-1) { // right boundary
      double x = p[0] * h;
      double y = global_range.end_pos()[1] * h;
      lmesh(p[0], p[1]) += (1.0 / (h*h)) * exact_solution(t, x, y);
    }
  }
}

/** Set the mesh values to the exact solution. */
void Fisher::set_solution(Mesh& result, double t)
{
  auto range = result.mesh_spec()->local_range();

  auto lresult = local_mesh(result);
  for (auto p : range) {
    double x = m_h * p[0];
    double y = m_h * p[1];
    lresult(p[0], p[1]) = exact_solution(t, x, y);
  }
}

/**
 Compute `f = (-Δ + a I) u`.
 */
void Fisher::apply_shifted_laplace(Mesh& f, Number a, const Mesh& u)
{
  // PETSc require to create a "local mesh" to have access to the ghost
  // nodes.
  ::LocalMesh u_aux(u.mesh_spec());
  u_aux.assign(u); // copy to determine the ghost nodes

  // We set the boundary to zero such that we can apply the same stencil
  // everywhere (also at the boundary).
  zero_boundary(u_aux);

  auto range = u.mesh_spec()->local_range();
  auto lu = local_mesh(u_aux);
  auto lf = local_mesh(f);

  // Apply the stencil
  //         |     -1     |
  // (1/h^2) | -1   4  -1 |
  //         |     -1     |h
  for (auto p : range) {
    lf(p[0], p[1])
      = (1.0 / (m_h*m_h))
        * ( (4+a*(m_h*m_h)) * lu(p[0],   p[1])
            - lu(p[0]-1, p[1])
            - lu(p[0],   p[1]-1)
            - lu(p[0],   p[1]+1)
            - lu(p[0]+1, p[1]));
  }
}

/**
 Solves y - a f_i(t, y) = r, where f_i(t, y) = Δy.
*/
void Fisher::solve_f_impl(Mesh& y,
                          Real t,
                          Number a,
                          const Mesh& r,
                          InitialGuess initial_guess) {
  using namespace std::placeholders;

  // rhs = (1/a) r + Δ^b u^b
  Mesh rhs(r.mesh_spec());
  rhs.assign(r);
  rhs *= (1.0/a);
  add_boundary(rhs, t);

  // solve (-Δ + (1/a) I) y = (1/a) r + Δ^b y^b
  CgSolver<Mesh> solver;
  auto op = std::bind(&Fisher::apply_shifted_laplace, this, _1, 1.0/a, _2);
  solver.setup(shared_copy(make_linear_operator<Mesh>(op)));
  solver.solve(y, rhs, initial_guess);
};

/** The explicit part of the ODE, f_e(y) = y*(1-y) */
void Fisher::f_expl(Mesh& result, Real t, const Mesh& u)
{
  auto range = u.mesh_spec()->local_range();
  auto lresult = local_mesh(result);
  auto lu = local_mesh(u);

  for (auto p : range) {
    lresult(p[0], p[1]) = lu(p[0], p[1]) * (1.0 - lu(p[0], p[1]));
  }
}

