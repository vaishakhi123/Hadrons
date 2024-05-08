/*************************************************************************************

Grid physics library, www.github.com/paboyle/Grid

Source file: Hadrons/Modules/MContraction/MesonCCLoopHL.hpp

Copyright (C) 2015-2019

Author: Antonin Portelli <antonin.portelli@me.com>
Author: Lanny91 <andrew.lawson@gmail.com>
Author: Vera Guelpers <vmg1n14@soton.ac.uk>
Author: Tom Blum (conserved currents)

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

See the full license in the file "LICENSE" in the top level distribution directory
*************************************************************************************/
/*  END LEGAL */

#ifndef Hadrons_MContraction_MesonLoopCCHL4D_hpp_
#define Hadrons_MContraction_MesonLoopCCHL4D_hpp_

#include <Hadrons/A2AVectors.hpp>
#include <Hadrons/EigenPack.hpp>
#include <Hadrons/Global.hpp>
#include <Hadrons/Module.hpp>
#include <Hadrons/ModuleFactory.hpp>
#include <Hadrons/Modules/MSource/Point.hpp>
#include <Hadrons/Solver.hpp>
#include <Grid/lattice/Lattice_reduction.h>

BEGIN_HADRONS_NAMESPACE

/******************************************************************************
 *             TMesonLoopCCHL                                    *
 ******************************************************************************/
BEGIN_MODULE_NAMESPACE(MContraction)


class MesonLoopCCHL4DPar: Serializable
{
public:
    GRID_SERIALIZABLE_CLASS_MEMBERS(MesonLoopCCHL4DPar,
                                    std::string, gauge,
                                    std::string, output,
                                    std::string, eigenPack,
                                    std::string, solver,
                                    std::string, action,
                                    double, mass,
                                    int, tinc,
                                    int, block,
                                    int, hits);
};

template <typename FImpl1, typename FImpl2>
class TStagMesonLoopCCHL4D: public Module<MesonLoopCCHL4DPar>
{
public:
    typedef typename FImpl1::FermionField FermionField;
    typedef A2AVectorsSchurStaggered<FImpl1> A2A;
    typedef FermionOperator<FImpl1>          FMat;
    FERM_TYPE_ALIASES(FImpl1,1);
    FERM_TYPE_ALIASES(FImpl2,2);
    SOLVER_TYPE_ALIASES(FImpl1,);
    class Result: Serializable
    {
    public:
        GRID_SERIALIZABLE_CLASS_MEMBERS(Result,
                                        std::vector<Complex>, corr);
    };
public:
    // constructor
    TStagMesonLoopCCHL4D(const std::string name);
    // destructor
    virtual ~TStagMesonLoopCCHL4D(void) {};
    // dependencies/products
    virtual std::vector<std::string> getInput(void);
    virtual std::vector<std::string> getOutput(void);
    protected:
    // execution
    virtual void setup(void);
    // execution
    virtual void execute(void);
    inline bool exists (const std::string& name) {
      struct stat buffer;
      return (stat (name.c_str(), &buffer) == 0);
    }

private:
    //FMat         *action_{nullptr};
    //Solver       *solver_{nullptr};
};

MODULE_REGISTER_TMP(StagMesonLoopCCHL4D, ARG(TStagMesonLoopCCHL4D<STAGIMPL, STAGIMPL>), MContraction);

/******************************************************************************
 *                           TStagMesonLoopCCHL implementation                      *
 ******************************************************************************/
// constructor /////////////////////////////////////////////////////////////////
template <typename FImpl1, typename FImpl2>
TStagMesonLoopCCHL4D<FImpl1, FImpl2>::TStagMesonLoopCCHL4D(const std::string name)
: Module<MesonLoopCCHL4DPar>(name)
{}

// dependencies/products ///////////////////////////////////////////////////////
template <typename FImpl1, typename FImpl2>
std::vector<std::string> TStagMesonLoopCCHL4D<FImpl1, FImpl2>::getInput(void)
{
    std::vector<std::string> input = {par().gauge, par().action};
    std::string sub_string;
    
    input.push_back(par().eigenPack);
    //input.push_back(par().solver + "_subtract");
    input.push_back(par().solver);
    
    return input;
}

template <typename FImpl1, typename FImpl2>
std::vector<std::string> TStagMesonLoopCCHL4D<FImpl1, FImpl2>::getOutput(void)
{
    std::vector<std::string> output = {};

    return output;
}

// setup ///////////////////////////////////////////////////////////////////
template <typename FImpl1, typename FImpl2>
void TStagMesonLoopCCHL4D<FImpl1, FImpl2>::setup(void)
{
    
    auto        &action     = envGet(FMat, par().action);
    //auto        &solver     = envGet(Solver, par().solver + "_subtract");
    auto        &solver     = envGet(Solver, par().solver);
    envTmp(A2A, "a2a", 1, action, solver);
    
    envTmpLat(FermionField, "source");
    envTmpLat(FermionField, "sink");
    envTmpLat(FermionField, "tmp");
    envTmpLat(FermionField, "tmp2");
    envTmpLat(FermionField, "sol");
    envTmpLat(FermionField, "solshift");
    envTmpLat(FermionField, "sourceshift");
    envTmpLat(FermionField, "w");
    envTmpLat(LatticeComplex, "eta");
}

// execution ///////////////////////////////////////////////////////////////////
template <typename FImpl1, typename FImpl2>
void TStagMesonLoopCCHL4D<FImpl1, FImpl2>::execute(void)
{
    LOG(Message) << "Computing High-Low Conserved Current Stag meson contractions " << std::endl;

    std::vector<ComplexD>  corr;
    std::vector<Result> result(3);
    int nt = env().getDim(Tp);
    int ns = env().getDim(Xp);
    // init
    for(int mu=0;mu<3;mu++){
        result[mu].corr.resize(nt);
        for(int t=0;t<nt;t++){
            result[mu].corr[t]=(ComplexD)(0.,0.);
        }
    }

    auto &U       = envGet(LatticeGaugeField, par().gauge);
    auto        &action      = envGet(FMat, par().action);
    //auto        &solver    = envGet(Solver, par().solver + "_subtract");
    auto        &solver    = envGet(Solver, par().solver);
    auto &epack   = envGet(BaseFermionEigenPack<FImpl1>, par().eigenPack);
    double mass = par().mass;
    int block = par().block;
    int hits = par().hits;
    std::vector<double> mlsq(epack.eval.size());
    for(int i=0;i<epack.eval.size();i++){
        mlsq[i]=(epack.eval[i]-mass*mass) * mass;
    }
    DeflatedGuesser<FermionField> LLsub(epack.evec, mlsq);
    FermionField tmp_e(env().getRbGrid());
    FermionField tmp_o(env().getRbGrid());

    envGetTmp(A2A, a2a);
    
    // Do spatial gammas only
    Lattice<iScalar<vInteger>> x(U.Grid()); LatticeCoordinate(x,0);
    Lattice<iScalar<vInteger>> y(U.Grid()); LatticeCoordinate(y,1);
    Lattice<iScalar<vInteger>> z(U.Grid()); LatticeCoordinate(z,2);
    Lattice<iScalar<vInteger>> t(U.Grid()); LatticeCoordinate(t,3);
    Lattice<iScalar<vInteger>> lin_z(U.Grid()); lin_z=x+y;
    Lattice<iScalar<vInteger>> lin_t(U.Grid()); lin_t=x+y+z;
    LatticeComplex phases(U.Grid());
    std::vector<LatticeColourMatrix> Umu(3,U.Grid());
    // source, solution
    envGetTmp(FermionField, source);
    envGetTmp(FermionField, sink);
    envGetTmp(FermionField, tmp);
    envGetTmp(FermionField, tmp2);
    envGetTmp(FermionField, sol);
    envGetTmp(FermionField, solshift);
    envGetTmp(FermionField, sourceshift);
    envGetTmp(FermionField, w);
    
    std::string outFileName;
    std::vector<std::vector<std::vector<ComplexD>>> all_results(3, 
        std::vector<std::vector<ComplexD>>(hits,
            std::vector<ComplexD>(nt, ComplexD(0., 0.))));
    
    for(int mu=0;mu<3;mu++){

        //staggered phases go into links
        Umu[mu] = PeekIndex<LorentzIndex>(U,mu);
        phases=1.0;
        if(mu==0){
        }else if(mu==1){
            phases = where( mod(x    ,2)==(Integer)0, phases,-phases);
        }else if(mu==2){
            phases = where( mod(lin_z,2)==(Integer)0, phases,-phases);
        }else if(mu==3){
            phases = where( mod(lin_t,2)==(Integer)0, phases,-phases);
        }else assert(0);
        Umu[mu] *= phases;
    }

    int Nl_ = epack.evec.size();
    Complex shift(1., 1.);
    
    std::vector<std::vector<std::vector<std::vector<ComplexD>>>> randomEtas(nt, std::vector<std::vector<std::vector<ComplexD>>>(3, std::vector<std::vector<ComplexD>>(Nl_, std::vector<ComplexD>(block * 2))));

    // Precompute and store random numbers for eta for all time slices, mu, and eigenvectors
    for(int ts = 0; ts < nt; ts++){
        for(int mu = 0; mu < 3; mu++){
            for(unsigned int il = 0; il < Nl_; il += block){
                for(int iv = il; iv < il + block; iv++){
                    for(int pm = 0; pm < 2; pm++){
                        ComplexD eta;
                        bernoulli(rngSerial(), eta);
                        // Store the precomputed random number in the data structure
                        randomEtas[ts][mu][iv][pm] = eta;
                    }
                }
            }
        }
    }
    

    FermionField sub(env().getGrid());

    // lopp over time slice
    for(int ts=0; ts<nt;ts+=par().tinc){

        LOG(Message) << "StagMesonLoopCCHLHL src_t " << ts << std::endl;
        //std::complex<double> eta = precomputedRandomNumbers[ts / par().tinc];
        // lopp over directions
        for(int mu=0;mu<3;mu++){

            LOG(Message) << "StagMesonLoopCCHLHL src_mu " << mu << std::endl;
             
            // lopp over hits
            LOG(Message) << "Total " << hits << "hits" <<std::endl;
            for(int hit = 1; hit <= hits; hit++)
            {
                // loop over evecs
                for (unsigned int il = 0; il < Nl_; il+=block)
                {
                    source = 0.0;
                    sink = 0.0;

                    //loop over blocks
                    for(int iv=il;iv<il+block;iv++){
            
                        std::complex<double> eval(mass,sqrt(epack.eval[iv]-mass*mass));
                        for(int pm=0;pm<2;pm++){
                            LOG(Message) << "Eigenvector " << 2*iv+pm << std::endl;
                            a2a.makeLowModeW(w, epack.evec[iv], eval, pm);
                            if(pm){
                                eval = conjugate(eval);
                            }
                            std::complex<double> iota_angle(0.0, std::arg(eval));
                            //ComplexD eta;
                            //bernoulli(rngSerial(), eta);
		
			                std::complex<double> eta = randomEtas[ts][mu][iv][pm];
                            //LOG(Message) << "random number " << eta << "for ts"<< ts <<std::endl;
			                eta = (2.*eta - shift)*(1./::sqrt(2.));
                
                            source += ((eta)*(std::exp(-iota_angle)/std::sqrt(std::abs(eval))))*w;
                            sink += ((eta)*(1./std::sqrt(std::abs(eval))))*w;
                        }
                    } 
                    
                    tmp = where(t == ts, source, source*0.);
                    tmp2 = adj(Umu[mu]) * tmp;
                   
                    // shift source at x to x+mu
                    tmp = Cshift(tmp2, mu, -1);
                    LOG(Message) << GridLogMessage<< "mu = "<<mu<<"ts= "<<ts<<std::endl;
                    solver(sol, tmp);
                    // subtract the low modes
                    sub = Zero();
                    pickCheckerboard(Even,tmp_e,tmp);
                    action.Meooe(tmp_e,tmp_o);
                    LLsub(tmp_o,tmp_e);
                    action.Meooe(tmp_e,tmp_o);// tmp_o is now even
                    setCheckerboard(sub,tmp_o);
                    sol += sub;
                
                    //LOG(Message) << "Solution " << sol << std::endl;
                    // take inner-product with eigenbra on all time slices
                    solshift = Cshift(sol,mu,1);
                    solshift = Umu[mu]*solshift;
                    sliceInnerProductVector(corr,sink,solshift,3); //first term
                    
                    for(int tsnk=0; tsnk<nt; tsnk++){
                        result[mu].corr[(tsnk-ts+nt)%nt] += (corr[tsnk]);
                    }
                    
                    
                    sourceshift = Cshift(sink,mu,1);
                    sourceshift = Umu[mu]*sourceshift;
                    sliceInnerProductVector(corr,sourceshift,sol,3); //third term
                    
                    // take inner-product with eigenmode on all time slices
                    for(int tsnk=0; tsnk<nt; tsnk++){
                        result[mu].corr[(tsnk-ts+nt)%nt] += (corr[tsnk]);
                    }

                    tmp = where(t == ts, source, source*0.);
                    // shift source
                    tmp2 = Cshift(tmp, mu, 1);
                    tmp = Umu[mu] * tmp2;

                    LOG(Message) << GridLogMessage<< "mu = "<<mu<<"ts= "<<ts<<std::endl;
                    solver(sol, tmp);
                    sub = Zero();
                    pickCheckerboard(Even,tmp_e,tmp);
                    action.Meooe(tmp_e,tmp_o);
                    LLsub(tmp_o,tmp_e);
                    action.Meooe(tmp_e,tmp_o);// tmp_o is now even
                    setCheckerboard(sub,tmp_o);
                    sol += sub;
                    
                    // take inner-product with eigenmode on all time slices
                    solshift = Cshift(sol,mu,1);
                    solshift = Umu[mu]*solshift;
                    sliceInnerProductVector(corr,sink,solshift,3); //second term
                    
                    for(int tsnk=0; tsnk<nt; tsnk++){
                        
                        result[mu].corr[(tsnk-ts+nt)%nt] += (corr[tsnk]);
                    }
                    
                    
                    sourceshift = Cshift(sink,mu,1);
                    sourceshift = Umu[mu]*sourceshift;
                    sliceInnerProductVector(corr,sourceshift,sol,3); //fourth term
                    for(int tsnk=0; tsnk<nt; tsnk++){
                        result[mu].corr[(tsnk-ts+nt)%nt] += (corr[tsnk]);
                    }
                }
            }
        }
    }
    for (int i = 0; i < 3; ++i){
        if(U.Grid()->IsBoss()){
            makeFileDir(par().output);
            outFileName = par().output+"HLcc_2pt_mu"+std::to_string(i);
            for(int t=0; t<nt; t++)
                result[i].corr[t] = result[i].corr[t]/std::complex<double>(hits, 0.0);
            saveResult(outFileName, "HLCC", result[i]);
        }
    }
    
}
    
END_MODULE_NAMESPACE

END_HADRONS_NAMESPACE

#endif
 
 // instead of 1 CG on all nodes, we want multiples CGs across all nodes