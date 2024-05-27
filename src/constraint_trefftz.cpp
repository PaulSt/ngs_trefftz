#include "constraint_trefftz.hpp"
#include <basematrix.hpp>
#include <bilinearform.hpp>
#include <core/array.hpp>
#include <core/flags.hpp>
#include <core/localheap.hpp>
#include <cstddef>
#include <elementtopology.hpp>
#include <expr.hpp>
#include <integrator.hpp>
#include <matrix.hpp>
#include <memory>
#include <meshaccess.hpp>
#include <pybind11/cast.h>
#include <integratorcf.hpp>

#include <fem.hpp>
#include <symbolicintegrator.hpp>
#include <fespace.hpp>
#include <vector>

#include "trefftz_helper.hpp"

using namespace ngfem;

namespace ngcomp
{
  /// creates an embedding marix P for the given operation `op`.
  ///
  /// The embedding is subject to the constraints specified in
  /// `cop_lhs` and `cop_rhs`.
  ///
  ///  @param op the differential operation
  ///
  ///  @param fes the finite element space of `op`
  ///
  ///  @param cop_lhs left hand side of the constraint operation
  ///
  ///  @param cop_rhs right hand side of the constraint operation
  ///
  ///  @param fes_constraint finite element space of the constraint operation
  ///
  ///  @param trefftzndof number of degrees of freedom per element
  ///      in the Trefftz finite element space on `fes`, generated by `op`
  ///      (i.e. the local dimension of the kernel of `op` on one element)
  ///
  ///  @return P, represented as a vector of all element matrices
  template <class SCAL>
  vector<ngbla::Matrix<SCAL>>
  ConstraintTrefftzEmbedding (std::shared_ptr<ngfem::SumOfIntegrals> op,
                              std::shared_ptr<FESpace> fes,
                              std::shared_ptr<ngfem::SumOfIntegrals> cop_lhs,
                              std::shared_ptr<ngfem::SumOfIntegrals> cop_rhs,
                              std::shared_ptr<FESpace> fes_constraint,
                              int trefftzndof)
  {
    // #TODO what is a good size for the local heap??
    LocalHeap local_heap = LocalHeap (1000 * 1000 * 1000);
    auto mesh_access = fes->GetMeshAccess ();
    const size_t num_elements = mesh_access->GetNE (VOL);
    const size_t ndof = fes->GetNDof ();
    const size_t dim = fes->GetDimension ();
    const size_t ndof_constraint = fes_constraint->GetNDof ();
    const size_t dim_constraint = fes_constraint->GetDimension ();

    // calculate the integrators for the three bilinear forms,
    // each for VOL, BND, BBND, BBBND, hence 4 arrays per bilnear form
    Array<shared_ptr<BilinearFormIntegrator>> op_integrators[4],
        cop_lhs_integrators[4], cop_rhs_integrators[4];
    calculateBilinearFormIntegrators (*op, op_integrators);
    calculateBilinearFormIntegrators (*cop_lhs, cop_lhs_integrators);
    calculateBilinearFormIntegrators (*cop_rhs, cop_rhs_integrators);

    vector<Matrix<SCAL>> element_matrices (num_elements);

    const bool fes_has_hidden_dofs = fesHasHiddenDofs (*fes);
    const bool fes_constraint_has_hidden_dofs
        = fesHasHiddenDofs (*fes_constraint);

    // solve the following linear system in an element-wise fashion:
    // L @ T1 = B for the unknown matrix T1,
    // with the given matrices:
    //     /   \    /   \
    //  A= |B_1| B= |B_2|
    //     | L |    | 0 |
    //     \   /    \   /
    mesh_access->IterateElements (
        VOL, local_heap,
        [&] (Ngs_Element mesh_element, LocalHeap &local_heap) {
          const ElementId element_id = ElementId (mesh_element);
          const FiniteElement &fes_element
              = fes->GetFE (element_id, local_heap);

          // skip this element, if the bilinear forms are not defined
          // on this element
          if (!bfIsDefinedOnElement (*op, mesh_element)
              || !bfIsDefinedOnElement (*cop_lhs, mesh_element)
              || !bfIsDefinedOnElement (*cop_rhs, mesh_element))
            return;

          Array<DofId> dofs, dofs_constraint;
          fes->GetDofNrs (element_id, dofs);
          fes_constraint->GetDofNrs (element_id, dofs_constraint);

          //     /   \    /   \
          //  A= |B_1| B= |B_2|
          //     | L |    | 0 |
          //     \   /    \   /
          // with B_1.shape == (ndof_constraint, ndof), L.shape == (ndof, ndof)
          // thus A.shape == (ndof + ndof_constraint, ndof)
          auto elmat_a = FlatMatrix<SCAL> (
              dofs.Size () + dofs_constraint.Size (), dofs.Size (),
              local_heap); // #TODO: define height and width
          tuple<FlatMatrix<SCAL>, FlatMatrix<SCAL>> b1_vstack_l
              = elmat_a.SplitRows (dofs_constraint.Size ());
          // elmat_l and elmat_b1 are views into elamt_a
          FlatMatrix<SCAL> elmat_b1 = get<0> (b1_vstack_l);
          FlatMatrix<SCAL> elmat_l = get<1> (b1_vstack_l);

          //     /   \    /   \
          //  A= |B_1| B= |B_2|
          //     | L |    | 0 |
          //     \   /    \   /
          // with B_2.shape == (ndof_constraint, ndof_constraint),
          // and B.shape == ( ndof_constraint + ndof, ndof_constraint)
          auto elmat_b = FlatMatrix<SCAL> (
              dofs_constraint.Size (), dofs_constraint.Size () + dofs.Size (),
              local_heap

          );
          // elmat_b2 is a view into elamt_b
          FlatMatrix<SCAL> elmat_b2
              = get<0> (elmat_b.SplitRows (dofs_constraint.Size ()));

          calculateElementMatrix (elmat_l, op_integrators[VOL], *mesh_access,
                                  element_id, *fes, *fes, local_heap);
          calculateElementMatrix (elmat_b1, op_integrators[VOL], *mesh_access,
                                  element_id, *fes, *fes_constraint,
                                  local_heap);
          calculateElementMatrix (elmat_b2, op_integrators[VOL], *mesh_access,
                                  element_id, *fes_constraint, *fes_constraint,
                                  local_heap);
          if (fes_has_hidden_dofs)
            extractVisibleDofs (elmat_l, element_id, *fes, *fes, dofs, dofs,
                                local_heap);

          // singular value decomposition of elmat_op:
          // U * sigma * V = elmat_op
          // elmat_a gets overwritten with sigma
          FlatMatrix<SCAL, ColMajor> U (dofs.Size (), local_heap),
              V (dofs.Size (), local_heap);
          GetSVD<SCAL> (elmat_a, U, V);

          FlatMatrix<SCAL> U_T = Trans (U);
          FlatMatrix<SCAL> V_T = Trans (V);
          // #TODO: check dimensions again
          FlatMatrix<SCAL> Sigma_inv (dofs_constraint.Size (),
                                      dofs_constraint.Size (), local_heap);
        });

    return element_matrices;
  }
}

void ExportConstraintTrefftzEmbedding (py::module m)
{
  m.def ("ConstraintTrefftzEmbedding",
         &ngcomp::ConstraintTrefftzEmbedding<double>,
         "creates the constraint Trefftz embedding matrix", py::arg ("op"),
         py::arg ("fes"), py::arg ("cop_lhs"), py::arg ("cop_rhs"),
         py::arg ("fes_constraint"), py::arg ("trefftzndof"));
}