#ifndef FILE_DIFFOPMAPPED
#define FILE_DIFFOPMAPPED

#include "TrefftzElement.hpp"

namespace ngfem
{

  /*
     realizations of bdb integrators for many equations.
     The differential operators provide the B-matrix,
     the DMatOps provide the coefficient tensors
  */

	/// Identity
	template <int D, typename FEL = ScalarMappedElement<D> >
	class DiffOpMapped : public DiffOp<	DiffOpMapped<D, FEL> >
	{
	public:
		enum { DIM = 1 };
		enum { DIM_SPACE = D };
		enum { DIM_ELEMENT = D };
		enum { DIM_DMAT = 1 };
		enum { DIFFORDER = 0 };

		static string Name() { return "mapped"; }
		static constexpr bool SUPPORT_PML = true;

		static const FEL & Cast (const FiniteElement & fel)
		{ return static_cast<const FEL&> (fel); }

		template <typename MIP, typename MAT>
		static void GenerateMatrix (const FiniteElement & fel, const MIP & mip,
				MAT && mat, LocalHeap & lh)
		{
			HeapReset hr(lh);
			mat.Row(0) = Cast(fel).GetShape(mip, lh);
		}

		static void GenerateMatrix (const FiniteElement & fel,
				const BaseMappedIntegrationPoint & mip,
				FlatMatrixFixHeight<1> & mat, LocalHeap & lh)
		{
			Cast(fel).CalcShape (mip, mat.Row(0)); // FlatVector<> (fel.GetNDof(), &mat(0,0)));
		}

		static void GenerateMatrix (const FiniteElement & fel,
				const BaseMappedIntegrationPoint & mip,
				SliceMatrix<double,ColMajor> mat, LocalHeap & lh)
		{
			Cast(fel).CalcShape (mip, mat.Row(0));
		}

		// using DiffOp<DiffOpId<D, FEL> > :: GenerateMatrixIR;
		template <typename MAT>
		static void GenerateMatrixIR (const FiniteElement & fel,
																	const BaseMappedIntegrationRule & mir,
																	MAT & mat, LocalHeap & lh)
		{ //cout << "pre mat 4: " << mat << endl;
			Cast(fel).CalcShape (mir, Trans(mat));
			//cout << "gen mat 4:" << mat << endl;
		}

		static void GenerateMatrixSIMDIR (const FiniteElement & fel,
																			const SIMD_BaseMappedIntegrationRule & mir,
																			BareSliceMatrix<SIMD<double>> mat)
		{
			Cast(fel).CalcShape (mir, mat);
		}

		template <typename MIP, class TVX, class TVY>
		static void Apply (const FiniteElement & fel, const MIP & mip,
					 const TVX & x, TVY & y,
					 LocalHeap & lh)
		{
			HeapReset hr(lh);
			y = Trans (Cast(fel).GetShape (mip, lh)) * x;
		}

		static void Apply (const FiniteElement & fel, const MappedIntegrationPoint<D,D> & mip,
					 const FlatVector<double> & x, FlatVector<double> & y,
					 LocalHeap & lh)
		{
			//cout << "x: " << x <<endl;
			y(0) = Cast(fel).Evaluate(mip, x);
		}

		// using DiffOp<DiffOpId<D, FEL> >::ApplyIR;
		template <class MIR, class TMY>
		static void ApplyIR (const FiniteElement & fel, const MIR & mir,
												 FlatVector<double> x, TMY y,
			 								 	 LocalHeap & lh)
		{
			Cast(fel).Evaluate (mir, x, FlatVector<> (mir.Size(), &y(0,0)));
		}

		template <class MIR>
		static void ApplyIR (const FiniteElement & fel, const MIR & mir,
												 FlatVector<Complex> x, FlatMatrix<Complex> y,
			 LocalHeap & lh)
		{
			Cast(fel).Evaluate (mir,
													SliceMatrix<double> (fel.GetNDof(), 2, 2, reinterpret_cast<double*> (&x(0))),
													SliceMatrix<double> (mir.Size(), 2, 2, reinterpret_cast<double*> (&y(0))));
		}

		// using ApplySIMDIR;
		using DiffOp<DiffOpMapped<D, FEL> >::ApplySIMDIR;
		static void ApplySIMDIR (const FiniteElement & fel, const SIMD_BaseMappedIntegrationRule & mir,
														 BareSliceVector<double> x, BareSliceMatrix<SIMD<double>> y)
		{
			Cast(fel).Evaluate (mir, x, y.Row(0));
		}

		template <typename MIP, class TVX, class TVY>
		static void ApplyTrans (const FiniteElement & fel, const MIP & mip,
					const TVX & x, TVY & y,
					LocalHeap & lh)
		{
			HeapReset hr(lh);
			y = Cast(fel).GetShape (mip, lh) * x;
		}

		// using DiffOp<DiffOpId<D, FEL> >::ApplyTransIR;
		template <class MIR>
		static void ApplyTransIR (const FiniteElement & fel,
						const MIR & mir,
						FlatMatrix<double> x, FlatVector<double> y,
						LocalHeap & lh)
		{
			Cast(fel).EvaluateTrans (mir, FlatVector<> (mir.Size(), &x(0,0)), y);
		}

		template <class MIR>
		static void ApplyTransIR (const FiniteElement & fel,
						const MIR & mir,
						FlatMatrix<Complex> x, FlatVector<Complex> y,
						LocalHeap & lh)
		{
			DiffOp<DiffOpMapped<D, FEL> > :: ApplyTransIR (fel, mir, x, y, lh);
		}

		using DiffOp<DiffOpMapped<D, FEL> >::AddTransSIMDIR;
		static void AddTransSIMDIR (const FiniteElement & fel, const SIMD_BaseMappedIntegrationRule & mir,
																BareSliceMatrix<SIMD<double>> y, BareSliceVector<double> x)
		{
			Cast(fel).AddTrans (mir, y.Row(0), x);
		}

	};



	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Identity on boundary
	template <int D, typename FEL = ScalarMappedElement<D> >
  class DiffOpMappedBoundary : public DiffOp<DiffOpMappedBoundary<D, FEL> >
  {
  public:
    enum { DIM = 1 };
    enum { DIM_SPACE = D };
    enum { DIM_ELEMENT = D-1 };
    enum { DIM_DMAT = 1 };
    enum { DIFFORDER = 0 };

    static const FEL & Cast (const FiniteElement & fel)
    { return static_cast<const FEL&> (fel); }

    template <typename AFEL, typename MIP, typename MAT>
    static void GenerateMatrix (const AFEL & fel, const MIP & mip,
				MAT & mat, LocalHeap & lh)
    {
      HeapReset hr(lh);
      mat.Row(0) = static_cast<const FEL&>(fel).GetShape(mip, lh);
    }

    template <typename AFEL, typename MIP, class TVX, class TVY>
    static void Apply (const AFEL & fel, const MIP & mip,
		       const TVX & x, TVY & y,
		       LocalHeap & lh)
    {
      HeapReset hr(lh);
      y = Trans (static_cast<const FEL&>(fel).GetShape (mip, lh)) * x;
      // y(0) = InnerProduct (x, static_cast<const FEL&>(fel).GetShape (mip.IP(), lh));
    }

    static void Apply (const ScalarFiniteElement<D-1> & fel, const MappedIntegrationPoint<D-1,D> & mip,
		       const FlatVector<double> & x, FlatVector<double> & y,
		       LocalHeap & lh)
    {
      y(0) = static_cast<const FEL&>(fel).Evaluate(mip, x);
    }

    template <typename AFEL, typename MIP, class TVX, class TVY>
    static void ApplyTrans (const AFEL & fel, const MIP & mip,
			    const TVX & x, TVY & y,
			    LocalHeap & lh)
    {
      HeapReset hr(lh);
      y = static_cast<const FEL&>(fel).GetShape (mip, lh) * x;
    }


    // using DiffOp<DiffOpIdBoundary<D, FEL> >::ApplyTransIR;
    template <class MIR>
    static void ApplyTransIR (const FiniteElement & fel, const MIR & mir,
			      FlatMatrix<double> x, FlatVector<double> y,
			      LocalHeap & lh)
    {
      // static Timer t("applytransir - bnd");
      // RegionTimer reg(t);

      static_cast<const FEL&>(fel).EvaluateTrans (mir, FlatVector<> (mir.Size(), &x(0,0)), y);
    }

    template <class MIR>
    static void ApplyTransIR (const FiniteElement & fel, const MIR & mir,
			      FlatMatrix<Complex> x, FlatVector<Complex> y,
			      LocalHeap & lh)
    {
      DiffOp<DiffOpMappedBoundary<D, FEL> > :: ApplyTransIR (fel, mir, x, y, lh);
      // static_cast<const FEL&>(fel).
      // EvaluateTrans (mir.IR(), FlatVector<> (mir.Size(), &x(0,0)), y);
    }

    using DiffOp<DiffOpMappedBoundary<D, FEL> >::ApplySIMDIR;
    static void ApplySIMDIR (const FiniteElement & fel, const SIMD_BaseMappedIntegrationRule & mir,
                             BareSliceVector<double> x, BareSliceMatrix<SIMD<double>> y)
    {
      Cast(fel).Evaluate (mir, x, y.Row(0));
    }

    using DiffOp<DiffOpMappedBoundary<D, FEL> >::AddTransSIMDIR;
    static void AddTransSIMDIR (const FiniteElement & fel, const SIMD_BaseMappedIntegrationRule & mir,
                                BareSliceMatrix<SIMD<double>> y, BareSliceVector<double> x)
    {
      Cast(fel).AddTrans (mir, y.Row(0), x);
    }
  };



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



	/// Gradient operator of dimension D
	template <int D, typename FEL = ScalarMappedElement<D> >
	class DiffOpMappedGradient : public DiffOp<DiffOpMappedGradient<D, FEL> >
	{
	public:
		enum { DIM = 1 };
		enum { DIM_SPACE = D };
		enum { DIM_ELEMENT = D };
		enum { DIM_DMAT = D };
		enum { DIFFORDER = 1 };

		static string Name() { return "mappedgrad"; }
		static constexpr bool SUPPORT_PML = true;

		static const FEL & Cast (const FiniteElement & fel)
		{ return static_cast<const FEL&> (fel); }

/*
		static void GenerateMatrix (const FiniteElement & fel,
																const MappedIntegrationPoint<D,D> & mip,
																SliceMatrix<double,ColMajor> mat, LocalHeap & lh)
		{
			Cast(fel).CalcMappedDShape (mip, Trans(mat));
		}

		template <typename MAT>
		static void GenerateMatrix (const FiniteElement & fel,
																const MappedIntegrationPoint<D,D,Complex> & mip,
																MAT && mat, LocalHeap & lh)
		{
			HeapReset hr(lh);
			mat = Trans (Cast(fel).GetDShape(mip,lh)); //* mip.GetJacobianInverse ());
		}


		static void GenerateMatrixIR (const FiniteElement & fel,
																	const MappedIntegrationRule<D,D> & mir,
																	SliceMatrix<double,ColMajor> mat, LocalHeap & lh)
		{
			Cast(fel).CalcMappedDShape (mir, Trans(mat));
		}

		static void GenerateMatrixSIMDIR (const FiniteElement & fel,
																			const SIMD_BaseMappedIntegrationRule & mir,
																			BareSliceMatrix<SIMD<double>> mat)
		{
			Cast(fel).CalcMappedDShape (mir, mat);
		}

		///
		template <typename MIP, class TVX, class TVY>
		static void Apply (const FiniteElement & fel, const MIP & mip,
					 const TVX & x, TVY && y,
					 LocalHeap & lh)
		{
			HeapReset hr(lh);
			typedef typename TVX::TSCAL TSCAL;
			Vec<D,TSCAL> hv = Trans (Cast(fel).GetDShape(mip.IP(), lh)) * x;
			y = Trans (mip.GetJacobianInverse()) * hv;
		}

		template <class TVY>
		static void Apply (const FiniteElement & fel, const MappedIntegrationPoint<D,D> & mip,
					 const FlatVector<> & x, TVY && y,
					 LocalHeap & lh)
		{
			Vec<D> hv = Cast(fel).EvaluateGrad(mip.IP(), x);
			y = Trans (mip.GetJacobianInverse()) * hv;
		}

		using DiffOp<DiffOpGradient<D, FEL> >::ApplyIR;

		template <class MIR>
		static void ApplyIR (const FiniteElement & fel, const MIR & mir,
			 const FlatVector<double> x, FlatMatrixFixWidth<D,double> y,
			 LocalHeap & lh)
		{
			FlatMatrixFixWidth<D> grad(mir.Size(), &y(0));
			Cast(fel).EvaluateGrad (mir.IR(), x, grad);
			for (int i = 0; i < mir.Size(); i++)
	{
		Vec<D> hv = grad.Row(i);
		grad.Row(i) = Trans (mir[i].GetJacobianInverse()) * hv;
	}
		}

		using DiffOp<DiffOpGradient<D, FEL> >::ApplySIMDIR;
		static void ApplySIMDIR (const FiniteElement & fel, const SIMD_BaseMappedIntegrationRule & mir,
														 BareSliceVector<double> x, BareSliceMatrix<SIMD<double>> y)
		{
			Cast(fel).EvaluateGrad (mir, x, y);
		}


		///
		template <typename MIP, class TVX, class TVY>
		static void ApplyTrans (const FiniteElement & fel, const MIP & mip,
					const TVX & x, TVY & y,
					LocalHeap & lh)
		{
			typedef typename TVX::TSCAL TSCAL;
			Vec<D,TSCAL> vx = x;
			auto hv = mip.GetJacobianInverse() * vx;
			y = Cast(fel).GetDShape(mip.IP(),lh) * hv;
		}

		using DiffOp<DiffOpGradient<D, FEL> >::AddTransSIMDIR;
		static void AddTransSIMDIR (const FiniteElement & fel, const SIMD_BaseMappedIntegrationRule & mir,
																BareSliceMatrix<SIMD<double>> y, BareSliceVector<double> x)
		{
			Cast(fel).AddGradTrans (mir, y, x);
		}
*/
	};




}
#endif
