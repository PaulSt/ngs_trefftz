#include <fem.hpp>
#include "myTrefftz.hpp"
#include "MultiArray.hpp"

namespace ngfem
{
	template <int D>
  void MyTrefftz<D> :: CalcShape (const IntegrationPoint & ip,
                                  BareSliceVector<> shape) const
  {
		for(int l=0;l<nbasis;l++) //loop over basis functions
		{
			for(int i=0;i<BinCoeff(D+1 + order, order);i++)//loop over indices
			{
				shape(l) += 1;
			}
		}
  }

	template <int D>
  void MyTrefftz<D> :: CalcDShape (const IntegrationPoint & ip,
                                   SliceMatrix<> dshape) const
  {
  }

	template <int D>
	void MyTrefftz<D> :: TrefftzBasis()
	{
		cout << "\n nbasis: " << nbasis << "\n";

		vector<array<int, D+1> > indices;
		indices.reserve( BinCoeff(D+1+order,order) );
		MakeIndices(order, indices);
		//MakeIndices(order-1, indices);

		for(int l=0;l<nbasis;l++) //loop over basis functions
		{
			cout << "======= basis: " << l << endl;

			for(int i=0;i<BinCoeff(D+1 + order, order);i++)//loop over indices
			{
				float k = (float)indices[i][0];
				if(k > 1 )
				{
					//cout << "===== rekursion";
					float temp = 0.0;

					for(int m=1;m<=D;m++)
					{ //rekursive sum
						array<int, D+1> get_coeff = indices[i];
						get_coeff[0] = get_coeff[0] - 2;
						get_coeff[m] = get_coeff[m] + 2;
						temp += (indices[i][m]+1) * (indices[i][m]+2) * basisFunctions[l].get(get_coeff);
					}

					temp = 1/(k * (k-1)) * temp;
					basisFunctions[l].put(indices[i],temp);
				}
				else if(k == 0 )
				{ //time=0
					basisFunctions[l].put(indices[l],1.0); //set coeff at time=0 to monomial basis
					i += BinCoeff(D + order, order) + BinCoeff(D + order-1, order-1);
				}

			}
		}
	}

	template <int D>
	void MyTrefftz<D> :: MakeIndices(int maxes, vector<array<int, D+1> > &indices)
	{
		array<int, D+1>  numbers;
		cout << "\n ===== exponentials: \n";
		MakeIndices_inner(D+1, numbers, maxes, indices);
	}

	template <int D>
	void MyTrefftz<D> :: MakeIndices_inner(int dim, array<int, D+1> &numbers, int maxes, vector< array<int, D+1> > &indices)
	{
		if (dim>0)
		{
			for(int i=0;i<=maxes;i++)
			{
				numbers[numbers.size() - dim]=i;
				MakeIndices_inner(dim-1, numbers,maxes,indices) ;
			}
		}
		else
		{
			int sum=0;
			for(int i=0;i<numbers.size();i++)
			{
				sum += numbers[i];
			}
			if(sum<=maxes){
				indices.push_back(numbers);
				for(int i=0;i<numbers.size();i++)
				{
					cout <<numbers[i]<<" ";
				}
				cout << "\n";
			}
		}
	}

	template <int D>
	int MyTrefftz<D> :: BinCoeff(int n,int k) const
	{
		return round( tgamma(n+1) / (tgamma(k+1)*tgamma(n-k+1)) );
	}
}


#ifdef NGS_PYTHON
void ExportMyTrefftz(py::module m)
{
  using namespace ngfem;
  py::class_<MyTrefftz<2>, shared_ptr<MyTrefftz<2>>, FiniteElement>
    (m, "MyTrefftz", "Trefftz space for wave eq")
    //.def(py::init<>())
		.def(py::init<int>())
		.def("TrefftzBasis", &MyTrefftz<2>::TrefftzBasis)
		.def("CalcShape", &MyTrefftz<2>::CalcShape)
		;
}
#endif // NGS_PYTHON
