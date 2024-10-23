from typing import Final, List
from ngsolve import *
from ngstrefftz import *
from dg import *
import time

import dg
from netgen.geom2d import unit_square
import scipy as sp
import numpy as np
import scipy.sparse

# import matplotlib.pyplot as plt


SetNumThreads(3)


def PySVDConformingTrefftz(
    op: comp.SumOfIntegrals,
    fes: FESpace,
    cop_lhs: comp.SumOfIntegrals,
    cop_rhs: comp.SumOfIntegrals,
    fes_conformity: FESpace,
    trefftzndof: int,
    debug: bool = False,
) -> np.ndarray:
    """
    produces an embedding matrix P

    `op`: the differential operation

    `fes`: the finite element space of `op`

    `cop_lhs`: left hand side of the conformity operation

    `cop_rhs`: right hand side of the conformity operation

    `fes_conformity`: finite element space of the conformity operation

    `trefftzndof`: number of degrees of freedom per element
        in the Trefftz finite element space on `fes`, generated by `op`
        (i.e. the local dimension of the kernel of `op` on one element)

    `debug`: if `True`, print debug messages. Default: `False`

    returns: P

    raises: LinAlgError: if the imput is not sound, a non-invertible matrix may
        be tried to be inverted
    """
    mesh = fes.mesh

    # let L be the matrix corrensponding to
    # the differential operator op
    opbf = BilinearForm(fes)
    opbf += op
    opbf.Assemble()
    rows, cols, vals = opbf.mat.COO()
    L = sp.sparse.csr_matrix((vals, (rows, cols)))
    L = L.todense()

    # let B1 be the matrix corrensponding to
    # the left hand side conformity operator cop_lhs
    copbf_lhs = BilinearForm(trialspace=fes, testspace=fes_conformity)
    copbf_lhs += cop_lhs
    copbf_lhs.Assemble()
    rows, cols, vals = copbf_lhs.mat.COO()
    B1 = sp.sparse.csr_matrix((vals, (rows, cols)))
    B1 = B1.todense()
    if debug:
        print("B1.shape", B1.shape)

    # let B2 be the matrix corrensponding to
    # the right hand side conformity operator cop_rhs
    copbf_rhs = BilinearForm(fes_conformity)
    copbf_rhs += cop_rhs
    copbf_rhs.Assemble()
    rows, cols, vals = copbf_rhs.mat.COO()
    B2 = sp.sparse.csr_matrix((vals, (rows, cols)))
    B2 = B2.todense()

    # localndof = int(fes.ndof/mesh.ne)
    # number of degrees of freedom per element
    # in the trefftz finite element space on fes
    # trefftzndof: Final[int] = 2 * order + 1 - 3

    # number of degrees of freedom of the contraint finite element space
    n_constr: Final[int] = fes_conformity.ndof

    # layout:
    # /    |    \
    # | P1 | P2 |
    # \    |    /
    # with P1 having shape (fes.ndof, n_constr),
    P = np.zeros([L.shape[1], (trefftzndof) * mesh.ne + n_constr])

    # solve the following linear system in an element-wise fashion:
    # L @ T1 = B for the unknown matrix T1,
    # with the given matrices:
    #     /   \    /   \
    #  A= |B_1| B= |B_2|
    #     | L |    | 0 |
    #     \   /    \   /
    for el, el_c in zip(fes.Elements(), fes_conformity.Elements()):
        nr: Final[int] = el.nr
        dofs: Final[List[int]] = el.dofs
        dofs_c: Final[List[int]] = el_c.dofs

        if debug:
            print("dofs:", dofs)
            print("dofs_c:", dofs_c)

        # produce local sub-matrices from the global matrices L, B1, B2
        elmat_l = L[dofs, :][:, dofs]
        elmat_b1 = B1[dofs_c, :][:, dofs]
        elmat_b2 = B2[dofs_c, :][:, dofs_c]

        if debug:
            print("elmat_b1", elmat_b1.shape)
            print("elmat_l", elmat_l.shape)

        #     /   \    /   \
        #  A= |B_1| B= |B_2|
        #     | L |    | 0 |
        #     \   /    \   /
        elmat_a = np.vstack([elmat_b1, elmat_l])
        elmat_b = np.vstack([elmat_b2, np.zeros([len(dofs), len(dofs_c)])])

        if debug:
            print("elmat_a", elmat_a)
            print("elmat_b", elmat_b)

        # A = U @ s @ V, singular value decomposition
        U, s, V = np.linalg.svd(elmat_a)

        # pseudo inverse of s
        s_inv = np.hstack(
            [np.diag(1.0 / s), np.zeros((V.shape[0], U.shape[0] - V.shape[0]))]
        )

        if debug:
            print("U", U)
            print("s_inv", s_inv)
            print("V", V)

        # solve A @ T1 = B
        # i.e. T1 = A^{-1} @ B
        # for the unknown T1
        T1 = V.T @ s_inv @ U.T @ elmat_b

        if debug:
            print("T1", T1)

        # Place the local solution T1
        # into the global solution P.
        P[np.ix_(dofs, dofs_c)] += T1

        ## matplotlib and netgen do not like to be opened at the same time,
        ## so the plotting of the singular values is disabled for now
        # if debug:
        #    plt.plot(s, marker="x")
        #    plt.title(f"singular values of A for element number {nr}")
        #    plt.xlabel("singular value number")
        #    plt.ylabel("singular value")
        #    plt.yscale("log")
        #    plt.show()

        # Place the basis of the kernel of A into P2.
        #
        # The basis of the kernel is found in V,
        # namely as the bottommost row-vectors.
        #
        # In P2, the kernel from one element is placed into
        # `trefftzndof` many neighbouring columns,
        # starting with element nr. 0 with columns
        # n_constr to n_constr + trefftzndof - 1 (inclusively).
        # Note, that the basis is transposed from row to column-vectors.
        nonzero_dofs: Final[int] = len(dofs) - trefftzndof
        P[
            np.ix_(
                dofs,
                range(n_constr + nr * trefftzndof, n_constr + (nr + 1) * trefftzndof),
            )
        ] += V[nonzero_dofs:, :].T

    if debug:
        import netgen.gui

        print(P)
        gfu = GridFunction(fes)
        for i in range(P.shape[1]):
            print("P slice:\n", P[:, i].flatten())
            gfu.vec.FV().NumPy()[:] = P[:, i]
            Draw(gfu)
            input("")
    return P


def test_PySVDConformingTrefftz(order: int = 2, debug: bool = False, maxh=0.4) -> float:
    """
    simple test case for PySVDConstrainedTrefftz.

    `order`: polynomial oder of the underlying space

    `debug`: True: print debug info, default: False

    >>> test_PySVDConformingTrefftz(order=3, debug=False) # doctest:+ELLIPSIS
    3.6...e-05
    """
    mesh2d = Mesh(unit_square.GenerateMesh(maxh=maxh))

    fes = L2(mesh2d, order=order, dgjumps=True)  # ,all_dofs_together=True)
    u, v = fes.TnT()
    uh = u.Operator("hesse")
    vh = v.Operator("hesse")
    op = (uh[0, 0] + uh[1, 1]) * (vh[0, 0] + vh[1, 1]) * dx

    fes_conformity = FacetFESpace(mesh2d, order=0)  # ,all_dofs_together=True)
    uF, vF = fes_conformity.TnT()
    cop_lhs = u * vF * dx(element_boundary=True)
    cop_rhs = uF * vF * dx(element_boundary=True)

    P = PySVDConformingTrefftz(
        op, fes, cop_lhs, cop_rhs, fes_conformity, 2 * order + 1 - 3, debug=False
    )
    if debug:
        print("P:")
        print(P.shape)
        print(P)

        import matplotlib.pyplot as plt

        plt.spy(P)
        plt.show()

    a, f = dg.dgell(fes, dg.exactlap)
    rows, cols, vals = a.mat.COO()
    A = scipy.sparse.csr_matrix((vals, (rows, cols)))
    A = A.todense()

    TA = P.transpose() @ A @ P
    tsol = np.linalg.solve(TA, P.transpose() @ f.vec.FV())
    tpgfu = GridFunction(fes)
    tpgfu.vec.data = P @ tsol
    if debug:
        import netgen.gui

        Draw(tpgfu)
        input()
    return sqrt(Integrate((tpgfu - dg.exactlap) ** 2, mesh2d))


def test_ConstrainedTrefftzCpp(order: int = 2, debug: bool = False, maxh=0.4) -> float:
    """
    simple test case for Constrained Trefftz C++ implementation.

    `order`: polynomial oder of the underlying space

    `debug`: True: print debug info, default: False

    >>> test_ConstrainedTrefftzCpp(order=3, debug=False) # doctest:+ELLIPSIS
    3.6...e-05
    """
    mesh2d = Mesh(unit_square.GenerateMesh(maxh=maxh))

    fes = L2(mesh2d, order=order, dgjumps=True)  # ,all_dofs_together=True)
    u, v = fes.TnT()
    uh = u.Operator("hesse")
    vh = v.Operator("hesse")
    op = (uh[0, 0] + uh[1, 1]) * (vh[0, 0] + vh[1, 1]) * dx

    fes_conformity = FacetFESpace(mesh2d, order=0)  # ,all_dofs_together=True)
    uF, vF = fes_conformity.TnT()
    cop_lhs = u * vF * dx(element_boundary=True)
    cop_rhs = uF * vF * dx(element_boundary=True)

    P = TrefftzEmbedding(op, fes, cop_lhs, cop_rhs, fes_conformity, 2 * order + 1 - 3)

    rows, cols, vals = P.COO()

    P = scipy.sparse.csr_matrix((vals, (rows, cols)))
    P.todense()
    if debug:
        print(P)

    if debug:
        import matplotlib.pyplot as plt

        plt.spy(P)
        plt.show()

    a, f = dg.dgell(fes, dg.exactlap)
    rows, cols, vals = a.mat.COO()
    A = scipy.sparse.csr_matrix((vals, (rows, cols)))
    A = A.todense()

    TA = P.transpose() @ A @ P
    tsol = np.linalg.solve(TA, P.transpose() @ f.vec.FV())
    tpgfu = GridFunction(fes)
    tpgfu.vec.data = P @ tsol
    if debug:
        import netgen.gui

        Draw(tpgfu)
        input()
    return sqrt(Integrate((tpgfu - dg.exactlap) ** 2, mesh2d))


def test_conformityed_trefftz_with_rhs(order, order_conformity):
    """
    >>> test_conformityed_trefftz_with_rhs(5, 3) < 1e-07
    True
    """
    mesh2d = Mesh(unit_square.GenerateMesh(maxh=0.3))
    fes = L2(mesh2d, order=order, dgjumps=True)
    mesh = fes.mesh
    start = time.time()
    u, v = fes.TnT()
    uh = u.Operator("hesse")
    vh = v.Operator("hesse")
    op = (uh[0, 0] + uh[1, 1]) * (vh[0, 0] + vh[1, 1]) * dx
    rhs = -exactpoi.Diff(x).Diff(x) - exactpoi.Diff(y).Diff(y)
    lop = -rhs * (vh[0, 0] + vh[1, 1]) * dx

    fes_conformity = FacetFESpace(
        mesh2d, order=order_conformity
    )  # ,all_dofs_together=True)
    uF, vF = fes_conformity.TnT()
    cop_lhs = u * vF * dx(element_boundary=True)
    cop_rhs = uF * vF * dx(element_boundary=True)

    PP, ufv = TrefftzEmbedding(
        op, fes, cop_lhs, cop_rhs, fes_conformity, lop, 2 * order + 3 - 1
    )
    PPT = PP.CreateTranspose()
    a, f = dgell(fes, exactpoi, rhs)
    TA = PPT @ a.mat @ PP
    TU = TA.Inverse() * (PPT * (f.vec - a.mat * ufv))
    tpgfu = GridFunction(fes)
    tpgfu.vec.data = PP * TU + ufv
    return sqrt(Integrate((tpgfu - exactpoi) ** 2, mesh))


def test_conformityed_trefftz_trivial_mixed_mode(order, order_conformity):
    """
    Test, if the conforming trefftz procedure takes the trial space as test space,
    if no test space is given.

    >>> test_conformityed_trefftz_trivial_mixed_mode(5, 3)
    """
    mesh2d = Mesh(unit_square.GenerateMesh(maxh=0.3))
    fes = L2(mesh2d, order=order, dgjumps=True)
    mesh = fes.mesh
    start = time.time()
    u, v = fes.TnT()
    uh = u.Operator("hesse")
    vh = v.Operator("hesse")
    op = (uh[0, 0] + uh[1, 1]) * (vh[0, 0] + vh[1, 1]) * dx
    rhs = -exactpoi.Diff(x).Diff(x) - exactpoi.Diff(y).Diff(y)
    lop = -rhs * (vh[0, 0] + vh[1, 1]) * dx

    fes_conformity = FacetFESpace(
        mesh2d, order=order_conformity
    )  # ,all_dofs_together=True)
    uF, vF = fes_conformity.TnT()
    cop_lhs = u * vF * dx(element_boundary=True)
    cop_rhs = uF * vF * dx(element_boundary=True)

    ta, va = TrefftzEmbedding(
        op, fes, cop_lhs, cop_rhs, fes_conformity, lop, 2 * order + 3 - 1
    )
    tb, vb = TrefftzEmbedding(
        op,
        fes,
        cop_lhs,
        cop_rhs,
        fes_conformity,
        lop,
        2 * order + 3 - 1,
        fes_test=fes,
    )
    import scipy.sparse as sp

    rows, cols, vals = ta.COO()
    Ta = sp.csr_matrix((vals, (rows, cols)))
    rows, cols, vals = tb.COO()
    Tb = sp.csr_matrix((vals, (rows, cols)))

    assert np.isclose(
        Ta.toarray(), Tb.toarray()
    ).all(), "The embedding matrices do not agree"
    assert np.isclose(
        va.FV().NumPy(), vb.FV().NumPy()
    ).all(), "The particular solutions disagree"


def test_conformityed_trefftz_mixed_mode(order, order_conformity):
    """
    >>> test_conformityed_trefftz_mixed_mode(6, 2) # doctest:+ELLIPSIS
    8...e-11
    """
    mesh2d = Mesh(unit_square.GenerateMesh(maxh=0.3))
    fes = L2(mesh2d, order=order, dgjumps=True)
    fes_test = L2(mesh2d, order=order - 1, dgjumps=True)
    mesh = fes.mesh
    start = time.time()
    u = fes.TrialFunction()
    v = fes_test.TestFunction()
    uh = u.Operator("hesse")
    vh = v.Operator("hesse")
    op = (uh[0, 0] + uh[1, 1]) * (vh[0, 0] + vh[1, 1]) * dx
    rhs = -exactpoi.Diff(x).Diff(x) - exactpoi.Diff(y).Diff(y)
    lop = -rhs * (vh[0, 0] + vh[1, 1]) * dx

    fes_conformity = FacetFESpace(
        mesh2d, order=order_conformity
    )  # ,all_dofs_together=True)
    uF, vF = fes_conformity.TnT()
    cop_lhs = u * vF * dx(element_boundary=True)
    cop_rhs = uF * vF * dx(element_boundary=True)

    PP, ufv = TrefftzEmbedding(
        op, fes, cop_lhs, cop_rhs, fes_conformity, lop, 2 * order + 3 - 1, fes_test
    )
    PPT = PP.CreateTranspose()
    a, f = dgell(fes, exactpoi, rhs)
    TA = PPT @ a.mat @ PP
    TU = TA.Inverse() * (PPT * (f.vec - a.mat * ufv))
    tpgfu = GridFunction(fes)
    tpgfu.vec.data = PP * TU + ufv
    return sqrt(Integrate((tpgfu - exactpoi) ** 2, mesh))


def test_conforming_trefftz_without_op(order, order_conformity):
    """
    ## >>> test_conforming_trefftz_without_op(5, 3) < 1e-07
    ## True
    """
    mesh2d = Mesh(unit_square.GenerateMesh(maxh=0.3))
    fes = L2(mesh2d, order=order, dgjumps=True)
    mesh = fes.mesh
    start = time.time()
    u, v = fes.TnT()
    uh = u.Operator("hesse")
    vh = v.Operator("hesse")
    op = None
    rhs = -exactpoi.Diff(x).Diff(x) - exactpoi.Diff(y).Diff(y)
    lop = -rhs * (vh[0, 0] + vh[1, 1]) * dx

    fes_conformity = FacetFESpace(
        mesh2d, order=order_conformity
    )  # ,all_dofs_together=True)
    uF, vF = fes_conformity.TnT()
    cop_lhs = u * vF * dx(element_boundary=True)
    cop_rhs = uF * vF * dx(element_boundary=True)

    PP, ufv = TrefftzEmbedding(
        op, fes, cop_lhs, cop_rhs, fes_conformity, lop, 2 * order + 3 - 1
    )
    PPT = PP.CreateTranspose()
    a, f = dgell(fes, exactpoi, rhs)
    TA = PPT @ a.mat @ PP
    TU = TA.Inverse() * (PPT * (f.vec - a.mat * ufv))
    tpgfu = GridFunction(fes)
    tpgfu.vec.data = PP * TU + ufv
    return sqrt(Integrate((tpgfu - exactpoi) ** 2, mesh))


def test_ConstrainedTrefftzFESpace(
    order: int = 2, debug: bool = False, maxh=0.4
) -> float:
    """
    simple test case for EmbTrefftzFESpace with conforming Trefftz.

    `order`: polynomial oder of the underlying space

    `debug`: True: print debug info, default: False

    >>> test_ConstrainedTrefftzFESpace(order=3, debug=False) # doctest:+ELLIPSIS
    3...e-05
    """
    mesh2d = Mesh(unit_square.GenerateMesh(maxh=maxh))

    fes = L2(mesh2d, order=order, dgjumps=True)  # ,all_dofs_together=True)

    u, v = fes.TnT()
    uh = u.Operator("hesse")
    vh = v.Operator("hesse")
    op = (uh[0, 0] + uh[1, 1]) * (vh[0, 0] + vh[1, 1]) * dx

    fes_conformity = FacetFESpace(mesh2d, order=0)  # ,all_dofs_together=True)
    uF, vF = fes_conformity.TnT()
    cop_lhs = u * vF * dx(element_boundary=True)
    cop_rhs = uF * vF * dx(element_boundary=True)

    fes_trefftz = EmbeddedTrefftzFES(fes)
    # P = TrefftzEmbedding(op, fes, cop_lhs, cop_rhs, fes_conformity, 2 * order + 1 - 3)
    fes_trefftz.SetOp(
        op, cop_lhs, cop_rhs, fes_conformity, ndof_trefftz=2 * order + 1 - 3
    )

    a, f = dg.dgell(fes_trefftz, dg.exactlap)
    a.Assemble()
    f.Assemble()

    A_inv = a.mat.Inverse()
    tpgfu = GridFunction(fes_trefftz)
    tpgfu.vec.data = A_inv * f.vec
    if debug:
        import netgen.gui

        Draw(tpgfu)
        input()
    return sqrt(Integrate((tpgfu - dg.exactlap) ** 2, mesh2d))


if __name__ == "__main__":
    import doctest

    doctest.testmod()
