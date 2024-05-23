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

using namespace ngfem;

/// @param bf symblic representation of a bilinear form
/// @param bfis stores the calculated BilinearFormIntegrators
void calculateBilinearFormIntegrators (
    const SumOfIntegrals &bf,
    Array<shared_ptr<BilinearFormIntegrator>> bfis[4])
{
  for (auto icf : bf.icfs)
    {
      DifferentialSymbol &dx = icf->dx;
      bfis[dx.vb] += icf->MakeBilinearFormIntegrator ();
    }
}

/// @param lf symblic representation of a linear form
/// @param lfis stores the calculated LinearFormIntegrators
void calculateLinearFormIntegrators (
    SumOfIntegrals &lf, Array<shared_ptr<LinearFormIntegrator>> lfis[4])
{
  for (auto icf : lf.icfs)
    {
      DifferentialSymbol &dx = icf->dx;
      lfis[dx.vb] += icf->MakeLinearFormIntegrator ();
    }
}

/// decides, if the given finite element space
/// has hidden degrees of freedom.
///
/// @param fes finite element space
bool fesHasHiddenDofs (const ngcomp::FESpace &fes)
{
  const size_t ndof = fes.GetNDof ();
  bool has_hidden_dofs = false;
  for (ngcomp::DofId d = 0; d < ndof; d++)
    if (ngcomp::HIDDEN_DOF == fes.GetDofCouplingType (d))
      return true;
  return false;
}

/// Tests, if the given bilinear form is defined on the given element.
///
/// @param bf bilinear form
/// @param mesh_element local mesh element
bool bfIsDefinedOnElement (const SumOfIntegrals &bf,
                           const ngcomp::Ngs_Element &mesh_element)
{
  for (auto icf : bf.icfs)
    {
      if (icf->dx.vb == VOL)
        if ((!icf->dx.definedonelements)
            || (icf->dx.definedonelements->Test (mesh_element.Nr ())))
          return true;
    }
  return false;
}

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

    //// let L be the matrix corrensponding to
    //// the differential operator op
    // T_BilinearForm<SCAL> opbf
    //     = T_BilinearForm<SCAL> (fes, "Base FE Space", Flags ({}));

    //// #TODO is there an easier way to add the whole
    //// SumOfIntegrals at once?
    //// ~~~~
    //// iterate over each integral individually,
    //// instead of the python way where one can add the whole sum at once
    // for (auto integral : *op)
    //   {
    //     opbf.AddIntegrator (integral->MakeBilinearFormIntegrator ());
    //   }

    // opbf.Assemble (local_heap);
    // const BaseMatrix &L = opbf.GetMatrix ();

    //// let B1 be the matrix corrensponding to
    //// the left hand side constraint operator cop_lhs
    // T_BilinearForm<SCAL> copbf_lhs = T_BilinearForm<SCAL> (
    //     fes, fes_constraint, "Mixed FE Space", Flags ({}));

    // for (auto integral : *cop_lhs)
    //   {
    //     copbf_lhs.AddIntegrator (integral->MakeBilinearFormIntegrator ());
    //   }

    // copbf_lhs.Assemble (local_heap);
    // const BaseMatrix &B1 = copbf_lhs.GetMatrix ();

    //// let B2 be the matrix corrensponding to
    //// the right hand side constraint operator cop_rhs
    // T_BilinearForm<SCAL> copbf_rhs = T_BilinearForm<SCAL> (
    //     fes_constraint, "Constraint FE Space", Flags ({}));

    // for (auto integral : *cop_rhs)
    //   {
    //     copbf_rhs.AddIntegrator (integral->MakeBilinearFormIntegrator ());
    //   }

    // copbf_rhs.Assemble (local_heap);
    // const BaseMatrix &B2 = copbf_lhs.GetMatrix ();

    //// number of degrees of freedom of the contraint finite element space
    // const size_t n_constr = fes_constraint->GetNDof ();

    //// layout:
    //// /    |    \
    //// | P1 | P2 |
    //// \    |    /
    //// with P1 having shape (fes.ndof, n_constr),
    // Matrix<SCAL> P = Matrix<SCAL> (fes->GetNDof (), n_constr);
    // P = 0.0;

    // calculate the integrators for the three bilinear forms,
    // each for VOL, BND, BBND, BBBND, hence 4 arrays per bilnear form
    Array<shared_ptr<BilinearFormIntegrator>> op_integrators[4],
        cop_lhs_integrators[4], cop_rhs_integrators[4];
    calculateBilinearFormIntegrators (*op, op_integrators);
    calculateBilinearFormIntegrators (*cop_lhs, cop_lhs_integrators);
    calculateBilinearFormIntegrators (*cop_rhs, cop_rhs_integrators);

    vector<Matrix<SCAL>> element_matrices (num_elements);

    const bool has_hidden_dofs = fesHasHiddenDofs (*fes);

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
          const ElementId el_id = ElementId (mesh_element);
          const FiniteElement &fes_element = fes->GetFE (el_id, local_heap);

          // skip this element, if the bilinear form is not defined
          // on this element
          if (!bfIsDefinedOnElement (*op, mesh_element))
            return;

          // #TODO: does array construction work this way?
          Array<DofId> dofs = Array<DofId> ();
          fes->GetDofNrs (el_id, dofs);

          Array<DofId> dofs_constraint = Array<DofId> ();
          fes_constraint->GetDofNrs (el_id, dofs_constraint);

          // #TODO: does this work this way?
          // auto el_l = opbf.GetInnerMatrix ();
          // auto el_b1 = copbf_lhs.GetInnerMatrix ();
          // auto el_b2 = copbf_rhs.GetInnerMatrix ();

          auto el_a = FlatMatrix<SCAL> (); // #TODO: define height and width
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
