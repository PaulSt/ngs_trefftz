#ifndef SPECIALCOEFFICIENTFUNCTION_HPP
#define SPECIALCOEFFICIENTFUNCTION_HPP

#include <fem.hpp>
#include <comp.hpp>
#include <multigrid.hpp>
#include <h1lofe.hpp>
#include <regex>
#include "trefftzelement.hpp"

using namespace ngcomp;
namespace ngfem
{

    class ClipCoefficientFunction : public CoefficientFunction
    {
        private:
            shared_ptr<CoefficientFunction> coef;
            double clipvalue;
            int clipdim;
        public:
            ///
            ClipCoefficientFunction(shared_ptr<CoefficientFunction> acoef,int adimension,int aclipdim,double aclipvalue, bool ais_complex = false)
                : CoefficientFunction(adimension,ais_complex), coef(acoef), clipdim(aclipdim), clipvalue(aclipvalue)
            { ; }
            ///
            virtual double Evaluate (const BaseMappedIntegrationPoint & ip) const;
            ///
            virtual void Evaluate (const BaseMappedIntegrationRule & ir, FlatMatrix<double> values) const;
            virtual void EvaluateStdRule (const BaseMappedIntegrationRule & ir, FlatMatrix<double> values) const;
    };


    class IntegrationPointFunction : public CoefficientFunction
    {
        public:

            IntegrationPointFunction(shared_ptr<MeshAccess> mesh, IntegrationRule& intrule)
                : CoefficientFunction(1)
            {
                values.resize(mesh->GetNE());

                for (auto& vec : values)
                {
                    vec.resize(intrule.GetNIP());

                    for (int i = 0;i < vec.size();i++)
                    {
                        vec[i] = i; //input data or something
                    }
                }
            }

            virtual double Evaluate(const BaseMappedIntegrationPoint & ip) const
            {
                int p = ip.GetIPNr();
                int el = ip.GetTransformation().GetElementNr();

                if (p < 0 || p >= values[el].size())
                {
                    cout << "got illegal integration point number " << p << endl;
                    return 0;
                }

                return values[el][p];
            }

            void PrintTable()
            {
                for (int i = 0;i < values.size();i++)
                {
                    for (int j = 0;j < values[i].size();j++)
                    {
                        cout << values[i][j] << ", ";
                    }
                    cout << endl;
                }
                cout << endl;
            }

        private:
            vector<vector<double>> values;
    };


    class TrefftzCoefficientFunction : public CoefficientFunction
    {
        int basisfunction;
        T_TrefftzElement<3> treff = T_TrefftzElement<3>(4,1,ET_TRIG,0);

        public:
        TrefftzCoefficientFunction()
            : CoefficientFunction(1) { ; }

        TrefftzCoefficientFunction(int basis)
            : CoefficientFunction(1) { basisfunction = basis; }

        virtual double Evaluate(const BaseMappedIntegrationPoint& mip) const override
        {
            FlatVector<double> point = mip.GetPoint();

            int ndof = treff.GetNBasis();
            cout  << "nr: " << basisfunction << " / " << ndof << endl;
            Vector<double> shape(ndof);
            //Matrix<double> shape(ndof,2);
            treff.CalcShape(mip,shape);
            return shape[basisfunction];
        }
    };
}

#ifdef NGS_PYTHON
#include <python_ngstd.hpp>
void ExportSpecialCoefficientFunction(py::module m);
#endif // NGS_PYTHON

#endif
