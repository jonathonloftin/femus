/** \file Ex11.cpp
 *  \brief This example shows how to set and solve the weak form
 *   of the Boussinesq appoximation of the Navier-Stokes Equation
 *
 *  \f{eqnarray*}
 *  && \mathbf{V} \cdot \nabla T - \nabla \cdot\alpha \nabla T = 0 \\
 *  && \mathbf{V} \cdot \nabla \mathbf{V} - \nabla \cdot \nu (\nabla \mathbf{V} +(\nabla \mathbf{V})^T)
 *  +\nabla P = \beta T \mathbf{j} \\
 *  && \nabla \cdot \mathbf{V} = 0
 *  \f}
 *  in a unit box domain (in 2D and 3D) with given temperature 0 and 1 on
 *  the left and right walls, respectively, and insulated walls elsewhere.
 *  \author Eugenio Aulisa
 */

#include "FemusInit.hpp"
#include "MultiLevelSolution.hpp"
#include "MultiLevelProblem.hpp"
#include "NumericVector.hpp"
#include "VTKWriter.hpp"
#include "GMVWriter.hpp"
#include "TransientSystem.hpp"
#include "NonLinearImplicitSystem.hpp"
#include "adept.h"
#include "FieldSplitTree.hpp"
#include <iostream>
#include <fstream>

double Prandtl = 0.015;
double Rayleigh = 3000;

using namespace std;
using namespace femus;

bool SetBoundaryCondition(const std::vector < double >& x, const char SolName[], double& value, const int facename, const double time) {
  bool dirichlet = true; //dirichlet
  value = 0.;
  if(!strcmp(SolName, "T")) {
    if(facename == 1) {
      value = 0.5 * (1.0 - exp(-10.0 * time));
    }
    else if(facename == 2) {
      value = -0.5 * (1.0 - exp(-10.0 * time));
    }
    else {
      dirichlet = false;
    }
  }
  else if(!strcmp(SolName, "P")) {
    dirichlet = false;
  }
  return dirichlet;
}

double InitalValueT(const std::vector < double >& x) {
  return sin(4.0 * x[0]);
};

void AssembleBoussinesqAppoximation_AD(MultiLevelProblem& ml_prob);    //, unsigned level, const unsigned &levelMax, const bool &assembleMatrix );
std::pair<double, vector <double> >GetKineandPointVlaue(MultiLevelSolution* mlSol);// obtain the Knetc energy evlolution;
double GetTemperatureValue(MultiLevelProblem& ml_prob, const unsigned &elem, const std::vector<double>&xi);
void PrintConvergenceInfo(char *stdOutfile, char* outfile, const unsigned &numofrefinements);
void PrintNonlinearTime(char *stdOutfile, char* outfile, const unsigned &numofrefinements);

enum PrecType {
  FS_VTp = 1,
  FS_TVp,
  ASM_VTp,
  ASM_TVp,
  ILU_VTp,
  ILU_TVp
};

int main(int argc, char** args) {

  unsigned precType = 0;

  if(argc >= 2) {
    if(!strcmp("FS_VT", args[1])) precType = FS_VTp;
    else if(!strcmp("FS_TV", args[1])) precType = FS_TVp;
    else if(!strcmp("ASM_VT", args[1])) precType = ASM_VTp;
    else if(!strcmp("ASM_TV", args[1])) precType = ASM_TVp;
    else if(!strcmp("ILU_VT", args[1])) precType = ILU_VTp;
    else if(!strcmp("ILU_TV", args[1])) precType = ILU_TVp;
    if(precType == 0) {
      std::cout << "wrong input arguments!" << std::endl;
      abort();
    }
  }
  else {
    std::cout << "No input argument set default preconditioner = NS+T" << std::endl;
    precType = FS_VTp;
  }

  if(argc >= 3) {
    Prandtl = strtod(args[2], NULL);
    std::cout << Prandtl << std::endl;
  }

  if(argc >= 4) {
    Rayleigh = strtod(args[3], NULL);
    std::cout << Rayleigh << std::endl;
  }

  // init Petsc-MPI communicator
  FemusInit mpinit(argc, args, MPI_COMM_WORLD);
  // define multilevel mesh
  MultiLevelMesh mlMsh;

  // read coarse level mesh and generate finers level meshes
  double scalingFactor = 1.;
  //mlMsh.ReadCoarseMesh("./input/cube_hex.neu","seventh",scalingFactor);
  mlMsh.ReadCoarseMesh("./input/rectangle_w4_h1.neu", "seventh", scalingFactor);
  /* "seventh" is the order of accuracy that is used in the gauss integration scheme
     probably in the furure it is not going to be an argument of this function   */
  unsigned dim = mlMsh.GetDimension();

  unsigned numberOfUniformLevels = 5;
  unsigned numberOfSelectiveLevels = 0;
  mlMsh.RefineMesh(numberOfUniformLevels , numberOfUniformLevels + numberOfSelectiveLevels, NULL);

  // erase all the coarse mesh levels
  //mlMsh.EraseCoarseLevels(numberOfUniformLevels - 3);

  // print mesh info
  mlMsh.PrintInfo();
  MultiLevelSolution mlSol(&mlMsh);

  // add variables to mlSol
  mlSol.AddSolution("T", LAGRANGE, SERENDIPITY, 2);
  mlSol.AddSolution("U", LAGRANGE, SECOND, 2);
  mlSol.AddSolution("V", LAGRANGE, SECOND, 2);

  if(dim == 3) mlSol.AddSolution("W", LAGRANGE, SECOND, 2);

  //mlSol.AddSolution("P", LAGRANGE, FIRST);
  mlSol.AddSolution("P",  DISCONTINUOUS_POLYNOMIAL, FIRST, 2);

  mlSol.AssociatePropertyToSolution("P", "Pressure");
  mlSol.Initialize("All");

  mlSol.Initialize("T", InitalValueT);

  // attach the boundary condition function and generate boundary data
  mlSol.AttachSetBoundaryConditionFunction(SetBoundaryCondition);
  mlSol.FixSolutionAtOnePoint("P");
  mlSol.GenerateBdc("U");
  mlSol.GenerateBdc("V");
  mlSol.GenerateBdc("P");
  mlSol.GenerateBdc("T", "Time_dependent");

  // define the multilevel problem attach the mlSol object to it
  MultiLevelProblem mlProb(&mlSol);

  // add system Poisson in mlProb as a Linear Implicit System
  TransientNonlinearImplicitSystem& system = mlProb.add_system < TransientNonlinearImplicitSystem > ("NS");

  // add solution "u" to system
  system.AddSolutionToSystemPDE("U");
  system.AddSolutionToSystemPDE("V");
  system.AddSolutionToSystemPDE("P");

  if(dim == 3) system.AddSolutionToSystemPDE("W");

  system.AddSolutionToSystemPDE("T");


  std::vector < unsigned > fieldUVP(3);
  fieldUVP[0] = system.GetSolPdeIndex("U");
  fieldUVP[1] = system.GetSolPdeIndex("V");
  fieldUVP[2] = system.GetSolPdeIndex("P");

  std::vector < unsigned > solutionTypeUVP(3);
  solutionTypeUVP[0] = mlSol.GetSolutionType("U");
  solutionTypeUVP[1] = mlSol.GetSolutionType("V");
  solutionTypeUVP[2] = mlSol.GetSolutionType("P");

  FieldSplitTree FS_NS(PREONLY, ASM_PRECOND, fieldUVP, solutionTypeUVP, "Navier-Stokes");
  FS_NS.SetAsmBlockSize(4);
  FS_NS.SetAsmNumeberOfSchurVariables(1);

  std::vector < unsigned > fieldT(1);
  fieldT[0] = system.GetSolPdeIndex("T");

  std::vector < unsigned > solutionTypeT(1);
  solutionTypeT[0] = mlSol.GetSolutionType("T");

  FieldSplitTree FS_T(PREONLY, ASM_PRECOND, fieldT, solutionTypeT, "Temperature");
  FS_T.SetAsmBlockSize(4);
  FS_T.SetAsmNumeberOfSchurVariables(1);

  std::vector < FieldSplitTree *> FS2;
  FS2.reserve(2);
  FS2.push_back(&FS_NS);
  FS2.push_back(&FS_T);
  FieldSplitTree FS_NST(RICHARDSON, FIELDSPLIT_PRECOND, FS2, "Benard");

  //system.SetLinearEquationSolverType(FEMuS_DEFAULT);
  system.SetLinearEquationSolverType(FEMuS_FIELDSPLIT); // Additive Swartz preconditioner
  //system.SetLinearEquationSolverType(FEMuS_ASM); // Field-Split preconditioned

  // attach the assembling function to system
  system.SetAssembleFunction(AssembleBoussinesqAppoximation_AD);

  system.SetMaxNumberOfNonLinearIterations(10);
  system.SetNonLinearConvergenceTolerance(1.e-8);
  //system.SetMaxNumberOfResidualUpdatesForNonlinearIteration(10);
  //system.SetResidualUpdateConvergenceTolerance(1.e-15);

  system.SetMaxNumberOfLinearIterations(10);
  system.SetAbsoluteLinearConvergenceTolerance(1.e-15);

  system.SetMgType(F_CYCLE);

  system.SetNumberPreSmoothingStep(2);
  system.SetNumberPostSmoothingStep(2);
  // initilaize and solve the system
  system.init();

  system.SetSolverFineGrids(RICHARDSON);
  system.SetPreconditionerFineGrids(ILU_PRECOND);
  system.SetFieldSplitTree(&FS_NST);
  system.SetTolerances(1.e-10, 1.e-20, 1.e+50, 20, 20);

  system.ClearVariablesToBeSolved();
  system.AddVariableToBeSolved("All");
  system.SetNumberOfSchurVariables(1);
  system.SetElementBlockNumber(4);

  system.SetSamePreconditioner();


  // print solutions
  std::vector < std::string > variablesToBePrinted;
  variablesToBePrinted.push_back("All");

  VTKWriter vtkIO(&mlSol);
  vtkIO.Write(DEFAULT_OUTPUTDIR, "biquadratic", variablesToBePrinted, 0);

  double dt = 0.2;
  system.SetIntervalTime(dt);
  unsigned n_timesteps = 1500;
  
  double kineticEnergy;
  char out_file[100]="";
  strcpy(out_file,"KineticEnergy.dat");
  ofstream outfile(out_file,ios::out|ios::trunc|ios::binary);
  
  char out_file1[100]="";
  strcpy(out_file1,"Uvelocity.dat");
  ofstream outfile1(out_file1,ios::out|ios::trunc|ios::binary);

  char out_file2[100]="";
  strcpy(out_file2,"Vvelocity.dat");
  ofstream outfile2(out_file2,ios::out|ios::trunc|ios::binary);

  vector <double> ptCoord;
  for(unsigned time_step = 0; time_step < n_timesteps; time_step++) {
    
    if(time_step > 0) system.SetMgType(V_CYCLE);
    system.MGsolve();
    system.CopySolutionToOldSolution();
   
    std::pair < double, vector <double> > out_value = GetKineandPointVlaue(&mlSol) ;
    kineticEnergy = out_value.first;
	ptCoord = out_value.second;

    outfile << (time_step + 1) * dt <<"  "<< sqrt(kineticEnergy/2.0/4.0) << std::endl; 
	outfile1 << (time_step + 1) * dt <<"  "<< ptCoord[0] << std::endl;
	outfile2 << (time_step + 1) * dt <<"  "<< ptCoord[1] << std::endl;

    if ((time_step + 1) % 10 ==0)  vtkIO.Write(DEFAULT_OUTPUTDIR, "biquadratic", variablesToBePrinted, time_step + 1);
  }

  mlMsh.PrintInfo();
  outfile.close();
  outfile1.close();
  outfile2.close();
  return 0;
}


void AssembleBoussinesqAppoximation_AD(MultiLevelProblem& ml_prob) {
  //  ml_prob is the global object from/to where get/set all the data
  //  level is the level of the PDE system to be assembled
  //  levelMax is the Maximum level of the MultiLevelProblem
  //  assembleMatrix is a flag that tells if only the residual or also the matrix should be assembled

  //  extract pointers to the several objects that we are going to use
  TransientNonlinearImplicitSystem* mlPdeSys   = &ml_prob.get_system<TransientNonlinearImplicitSystem> ("NS");   // pointer to the linear implicit system named "Poisson"
  const unsigned level = mlPdeSys->GetLevelToAssemble();

  Mesh*           msh         = ml_prob._ml_msh->GetLevel(level);    // pointer to the mesh (level) object
  elem*           el          = msh->el;  // pointer to the elem object in msh (level)

  MultiLevelSolution*   mlSol         = ml_prob._ml_sol;  // pointer to the multilevel solution object
  Solution*   sol         = ml_prob._ml_sol->GetSolutionLevel(level);    // pointer to the solution (level) object


  LinearEquationSolver* pdeSys        = mlPdeSys->_LinSolver[level];  // pointer to the equation (level) object

  bool assembleMatrix = mlPdeSys->GetAssembleMatrix();
  // call the adept stack object
  adept::Stack& s = FemusInit::_adeptStack;
  if(assembleMatrix) s.continue_recording();
  else s.pause_recording();

  SparseMatrix*   KK          = pdeSys->_KK;  // pointer to the global stifness matrix object in pdeSys (level)
  NumericVector*  RES         = pdeSys->_RES; // pointer to the global residual vector object in pdeSys (level)

  const unsigned  dim = msh->GetDimension(); // get the domain dimension of the problem
  unsigned dim2 = (3 * (dim - 1) + !(dim - 1));        // dim2 is the number of second order partial derivatives (1,3,6 depending on the dimension)
  unsigned    iproc = msh->processor_id(); // get the process_id (for parallel computation)

  // reserve memory for the local standar vectors
  const unsigned maxSize = static_cast< unsigned >(ceil(pow(3, dim)));          // conservative: based on line3, quad9, hex27

  //solution variable
  unsigned solTIndex;
  solTIndex = mlSol->GetIndex("T");    // get the position of "T" in the ml_sol object
  unsigned solTType = mlSol->GetSolutionType(solTIndex);    // get the finite element type for "T"

  vector < unsigned > solVIndex(dim);
  solVIndex[0] = mlSol->GetIndex("U");    // get the position of "U" in the ml_sol object
  solVIndex[1] = mlSol->GetIndex("V");    // get the position of "V" in the ml_sol object

  if(dim == 3) solVIndex[2] = mlSol->GetIndex("W");       // get the position of "V" in the ml_sol object

  unsigned solVType = mlSol->GetSolutionType(solVIndex[0]);    // get the finite element type for "u"

  unsigned solPIndex;
  solPIndex = mlSol->GetIndex("P");    // get the position of "P" in the ml_sol object
  unsigned solPType = mlSol->GetSolutionType(solPIndex);    // get the finite element type for "u"

  unsigned solTPdeIndex;
  solTPdeIndex = mlPdeSys->GetSolPdeIndex("T");    // get the position of "T" in the pdeSys object

  // std::cout << solTIndex <<" "<<solTPdeIndex<<std::endl;


  vector < unsigned > solVPdeIndex(dim);
  solVPdeIndex[0] = mlPdeSys->GetSolPdeIndex("U");    // get the position of "U" in the pdeSys object
  solVPdeIndex[1] = mlPdeSys->GetSolPdeIndex("V");    // get the position of "V" in the pdeSys object

  if(dim == 3) solVPdeIndex[2] = mlPdeSys->GetSolPdeIndex("W");

  unsigned solPPdeIndex;
  solPPdeIndex = mlPdeSys->GetSolPdeIndex("P");    // get the position of "P" in the pdeSys object

  vector < adept::adouble >  solT; // local solution
  vector < vector < adept::adouble > >  solV(dim);    // local solution
  vector < adept::adouble >  solP; // local solution

  vector < double >  solTold; // local solution
  vector < vector < double > >  solVold(dim);    // local solution
  vector < double >  solPold; // local solution

  vector< adept::adouble > aResT; // local redidual vector
  vector< vector < adept::adouble > > aResV(dim);    // local redidual vector
  vector< adept::adouble > aResP; // local redidual vector

  vector < vector < double > > coordX(dim);    // local coordinates
  unsigned coordXType = 2; // get the finite element type for "x", it is always 2 (LAGRANGE QUADRATIC)

  solT.reserve(maxSize);
  solTold.reserve(maxSize);
  aResT.reserve(maxSize);

  for(unsigned  k = 0; k < dim; k++) {
    solV[k].reserve(maxSize);
    solVold[k].reserve(maxSize);
    aResV[k].reserve(maxSize);
    coordX[k].reserve(maxSize);
  }

  solP.reserve(maxSize);
  solPold.reserve(maxSize);
  aResP.reserve(maxSize);


  vector <double> phiV;  // local test function
  vector <double> phiV_x; // local test function first order partial derivatives
  vector <double> phiV_xx; // local test function second order partial derivatives

  phiV.reserve(maxSize);
  phiV_x.reserve(maxSize * dim);
  phiV_xx.reserve(maxSize * dim2);

  vector <double> phiT;  // local test function
  vector <double> phiT_x; // local test function first order partial derivatives
  vector <double> phiT_xx; // local test function second order partial derivatives

  phiT.reserve(maxSize);
  phiT_x.reserve(maxSize * dim);
  phiT_xx.reserve(maxSize * dim2);

  double* phiP;
  double weight; // gauss point weight

  vector< int > sysDof; // local to global pdeSys dofs
  sysDof.reserve((dim + 2) *maxSize);

  vector< double > Res; // local redidual vector
  Res.reserve((dim + 2) *maxSize);

  vector < double > Jac;
  Jac.reserve((dim + 2) *maxSize * (dim + 2) *maxSize);

  if(assembleMatrix)
    KK->zero(); // Set to zero all the entries of the Global Matrix

  // element loop: each process loops only on the elements that owns
  for(int iel = msh->_elementOffset[iproc]; iel < msh->_elementOffset[iproc + 1]; iel++) {

    // element geometry type
    short unsigned ielGeom = msh->GetElementType(iel);

    unsigned nDofsT = msh->GetElementDofNumber(iel, solTType);    // number of solution element dofs
    unsigned nDofsV = msh->GetElementDofNumber(iel, solVType);    // number of solution element dofs
    unsigned nDofsP = msh->GetElementDofNumber(iel, solPType);    // number of solution element dofs
    unsigned nDofsX = msh->GetElementDofNumber(iel, coordXType);    // number of coordinate element dofs

    unsigned nDofsTVP = nDofsT + dim * nDofsV + nDofsP;
    // resize local arrays
    sysDof.resize(nDofsTVP);

    solT.resize(nDofsV);
    solTold.resize(nDofsV);

    for(unsigned  k = 0; k < dim; k++) {
      solV[k].resize(nDofsV);
      solVold[k].resize(nDofsV);
      coordX[k].resize(nDofsX);
    }

    solP.resize(nDofsP);
    solPold.resize(nDofsP);

    aResT.resize(nDofsV);    //resize
    std::fill(aResT.begin(), aResT.end(), 0);    //set aRes to zero

    for(unsigned  k = 0; k < dim; k++) {
      aResV[k].resize(nDofsV);    //resize
      std::fill(aResV[k].begin(), aResV[k].end(), 0);    //set aRes to zero
    }

    aResP.resize(nDofsP);    //resize
    std::fill(aResP.begin(), aResP.end(), 0);    //set aRes to zero

    // local storage of global mapping and solution
    for(unsigned i = 0; i < nDofsT; i++) {
      unsigned solTDof = msh->GetSolutionDof(i, iel, solTType);    // global to global mapping between solution node and solution dof
      solT[i] = (*sol->_Sol[solTIndex])(solTDof);      // global extraction and local storage for the solution
      solTold[i] = (*sol->_SolOld[solTIndex])(solTDof);      // global extraction and local storage for the solution
      sysDof[i] = pdeSys->GetSystemDof(solTIndex, solTPdeIndex, i, iel);    // global to global mapping between solution node and pdeSys dofs
    }

    // local storage of global mapping and solution
    for(unsigned i = 0; i < nDofsV; i++) {
      unsigned solVDof = msh->GetSolutionDof(i, iel, solVType);    // global to global mapping between solution node and solution dof

      for(unsigned  k = 0; k < dim; k++) {
        solV[k][i] = (*sol->_Sol[solVIndex[k]])(solVDof);      // global extraction and local storage for the solution
        solVold[k][i] = (*sol->_SolOld[solVIndex[k]])(solVDof);      // global extraction and local storage for the solution
        sysDof[i + nDofsT + k * nDofsV] = pdeSys->GetSystemDof(solVIndex[k], solVPdeIndex[k], i, iel);    // global to global mapping between solution node and pdeSys dof
      }
    }

    for(unsigned i = 0; i < nDofsP; i++) {
      unsigned solPDof = msh->GetSolutionDof(i, iel, solPType);    // global to global mapping between solution node and solution dof
      solP[i] = (*sol->_Sol[solPIndex])(solPDof);      // global extraction and local storage for the solution
      solPold[i] = (*sol->_SolOld[solPIndex])(solPDof);      // global extraction and local storage for the solution
      sysDof[i + nDofsT + dim * nDofsV] = pdeSys->GetSystemDof(solPIndex, solPPdeIndex, i, iel);    // global to global mapping between solution node and pdeSys dof
    }

    // local storage of coordinates
    for(unsigned i = 0; i < nDofsX; i++) {
      unsigned coordXDof  = msh->GetSolutionDof(i, iel, coordXType);    // global to global mapping between coordinates node and coordinate dof

      for(unsigned k = 0; k < dim; k++) {
        coordX[k][i] = (*msh->_topology->_Sol[k])(coordXDof);      // global extraction and local storage for the element coordinates
      }
    }


    // start a new recording of all the operations involving adept::adouble variables
    if(assembleMatrix) s.new_recording();

    // *** Gauss point loop ***
    for(unsigned ig = 0; ig < msh->_finiteElement[ielGeom][solVType]->GetGaussPointNumber(); ig++) {
      // *** get gauss point weight, test function and test function partial derivatives ***
      msh->_finiteElement[ielGeom][solTType]->Jacobian(coordX, ig, weight, phiT, phiT_x, phiT_xx);
      msh->_finiteElement[ielGeom][solVType]->Jacobian(coordX, ig, weight, phiV, phiV_x, phiV_xx);
      phiP = msh->_finiteElement[ielGeom][solPType]->GetPhi(ig);

      // evaluate the solution, the solution derivatives and the coordinates in the gauss point
      adept::adouble solT_gss = 0;
      double solTold_gss = 0;
      vector < adept::adouble > gradSolT_gss(dim, 0.);
      vector < double > gradSolTold_gss(dim, 0.);

      for(unsigned i = 0; i < nDofsT; i++) {
        solT_gss += phiT[i] * solT[i];
        solTold_gss += phiT[i] * solTold[i];

        for(unsigned j = 0; j < dim; j++) {
          gradSolT_gss[j] += phiT_x[i * dim + j] * solT[i];
          gradSolTold_gss[j] += phiT_x[i * dim + j] * solTold[i];
        }
      }

      vector < adept::adouble > solV_gss(dim, 0);
      vector < double > solVold_gss(dim, 0);
      vector < vector < adept::adouble > > gradSolV_gss(dim);
      vector < vector < double > > gradSolVold_gss(dim);

      for(unsigned  k = 0; k < dim; k++) {
        gradSolV_gss[k].resize(dim);
        gradSolVold_gss[k].resize(dim);
        std::fill(gradSolV_gss[k].begin(), gradSolV_gss[k].end(), 0);
        std::fill(gradSolVold_gss[k].begin(), gradSolVold_gss[k].end(), 0);
      }

      for(unsigned i = 0; i < nDofsV; i++) {
        for(unsigned  k = 0; k < dim; k++) {
          solV_gss[k] += phiV[i] * solV[k][i];
          solVold_gss[k] += phiV[i] * solVold[k][i];
        }

        for(unsigned j = 0; j < dim; j++) {
          for(unsigned  k = 0; k < dim; k++) {
            gradSolV_gss[k][j] += phiV_x[i * dim + j] * solV[k][i];
            gradSolVold_gss[k][j] += phiV_x[i * dim + j] * solVold[k][i];
          }
        }
      }

      adept::adouble solP_gss = 0;
      double solPold_gss = 0;

      for(unsigned i = 0; i < nDofsP; i++) {
        solP_gss += phiP[i] * solP[i];
        solPold_gss += phiP[i] * solPold[i];
      }


      double alpha = 1.;
      double beta = 1.;//40000.;

      double Pr = 0.015;
      double Ra = 3000;

      double dt = mlPdeSys -> GetIntervalTime();
      // *** phiT_i loop ***
      for(unsigned i = 0; i < nDofsT; i++) {
        adept::adouble Temp = 0.;
        adept::adouble TempOld = 0.;

        for(unsigned j = 0; j < dim; j++) {
          Temp +=  1. / sqrt(Ra * Pr) * alpha * phiT_x[i * dim + j] * gradSolT_gss[j];
          Temp +=  phiT[i] * (solV_gss[j] * gradSolT_gss[j]);

          TempOld +=  1. / sqrt(Ra * Pr) * alpha * phiT_x[i * dim + j] * gradSolTold_gss[j];
          TempOld +=  phiT[i] * (solVold_gss[j] * gradSolTold_gss[j]);

        }

        aResT[i] += (- (solT_gss - solTold_gss) * phiT[i] / dt - 0.5 * (Temp + TempOld)) * weight;
      } // end phiT_i loop


      // *** phiV_i loop ***
      for(unsigned i = 0; i < nDofsV; i++) {
        vector < adept::adouble > NSV(dim, 0.);
        vector < double > NSVold(dim, 0.);

        for(unsigned j = 0; j < dim; j++) {
          for(unsigned  k = 0; k < dim; k++) {
            NSV[k]   +=  sqrt(Pr / Ra) * phiV_x[i * dim + j] * (gradSolV_gss[k][j] + gradSolV_gss[j][k]);
            NSV[k]   +=  phiV[i] * (solV_gss[j] * gradSolV_gss[k][j]);

            NSVold[k]   +=  sqrt(Pr / Ra) * phiV_x[i * dim + j] * (gradSolVold_gss[k][j] + gradSolVold_gss[j][k]);
            NSVold[k]   +=  phiV[i] * (solVold_gss[j] * gradSolVold_gss[k][j]);

          }
        }

        for(unsigned  k = 0; k < dim; k++) {
          NSV[k] += -solP_gss * phiV_x[i * dim + k];
          NSVold[k] += -solPold_gss * phiV_x[i * dim + k];
        }

        NSV[1] += -beta * solT_gss * phiV[i];
        NSVold[1] += -beta * solTold_gss * phiV[i];

        for(unsigned  k = 0; k < dim; k++) {
          aResV[k][i] += (- (solV_gss[k] - solVold_gss[k]) * phiV[i] / dt - 0.5 * (NSV[k] + NSVold[k])) * weight;
        }
      } // end phiV_i loop

      // *** phiP_i loop ***
      for(unsigned i = 0; i < nDofsP; i++) {
        for(int k = 0; k < dim; k++) {
          aResP[i] += - (gradSolV_gss[k][k]) * phiP[i]  * weight;
        }
      } // end phiP_i loop

    } // end gauss point loop

    //--------------------------------------------------------------------------------------------------------
    // Add the local Matrix/Vector into the global Matrix/Vector

    //copy the value of the adept::adoube aRes in double Res and store them in RES
    Res.resize(nDofsTVP);    //resize

    for(int i = 0; i < nDofsT; i++) {
      Res[i] = -aResT[i].value();
    }

    for(int i = 0; i < nDofsV; i++) {
      for(unsigned  k = 0; k < dim; k++) {
        Res[ i + nDofsT + k * nDofsV ] = -aResV[k][i].value();
      }
    }

    for(int i = 0; i < nDofsP; i++) {
      Res[ i + nDofsT + dim * nDofsV ] = -aResP[i].value();
    }

    RES->add_vector_blocked(Res, sysDof);

    //Extarct and store the Jacobian
    if(assembleMatrix) {
      Jac.resize(nDofsTVP * nDofsTVP);
      // define the dependent variables
      s.dependent(&aResT[0], nDofsT);

      for(unsigned  k = 0; k < dim; k++) {
        s.dependent(&aResV[k][0], nDofsV);
      }

      s.dependent(&aResP[0], nDofsP);

      // define the independent variables
      s.independent(&solT[0], nDofsT);

      for(unsigned  k = 0; k < dim; k++) {
        s.independent(&solV[k][0], nDofsV);
      }

      s.independent(&solP[0], nDofsP);

      // get the and store jacobian matrix (row-major)
      s.jacobian(&Jac[0] , true);
      KK->add_matrix_blocked(Jac, sysDof, sysDof);

      s.clear_independents();
      s.clear_dependents();
    }
  } //end element loop for each process

  RES->close();

  if(assembleMatrix) {
    KK->close();
  }

  // ***************** END ASSEMBLY *******************
}
std::pair <double, vector <double> >GetKineandPointVlaue(MultiLevelSolution* mlSol){

  unsigned level = mlSol->_mlMesh->GetNumberOfLevels()-1u;
  //  extract pointers to the several objects that we are going to use
  Mesh* msh = mlSol->_mlMesh->GetLevel(level); // pointer to the mesh (level) object 
  elem* el = msh->el; // pointer to the elem object in msh (level)
  Solution* sol = mlSol -> GetSolutionLevel (level); //pointer to the solution (level) object 
  
  const unsigned dim = msh->GetDimension(); // get the domain dimension of the problem
  unsigned dim2 = (3 * (dim - 1) + !(dim - 1)); // dim2 is the number of second order partial derivatives (1,3,6 depending on the dimension)
  unsigned iproc = msh->processor_id(); // get the process_id (for parallel computation)
  
  // reserve memory for the local standar vectors
  const unsigned maxSize = static_cast< unsigned >(ceil(pow(3, dim)));          // conservative: based on line3, quad9, hex27

  vector < unsigned > solVIndex(dim);
  solVIndex[0] = mlSol->GetIndex("U");    // get the position of "U" in the ml_sol object
  solVIndex[1] = mlSol->GetIndex("V");    // get the position of "V" in the ml_sol object
  if(dim == 3) solVIndex[2] = mlSol->GetIndex("W");	// get the position of "W" in the ml_sol object
  unsigned solVType = mlSol->GetSolutionType(solVIndex[0]);	// get the finite element type for "U"

  vector < vector < double > >  solV(dim);    // local solution
  vector < vector < double > > coordX(dim);    // local coordinates
  unsigned coordXType = 2; // get the finite element type for "x", it is always 2 (LAGRANGE QUADRATIC)

  for(unsigned  k = 0; k < dim; k++) {
    solV[k].reserve(maxSize);
    coordX[k].reserve(maxSize);
  }

  vector <double> phiV;  // local test function
  vector <double> phiV_x; // local test function first order partial derivatives
  vector <double> phiV_xx; // local test function second order partial derivatives

  phiV.reserve(maxSize);
  phiV_x.reserve(maxSize * dim);
  phiV_xx.reserve(maxSize * dim2);
  
  double weight; // gauss point weight
  double kineticEnergy = 0.0; 
  
  unsigned recordCoord = 0; 
  double ptUCoord = 0.0;
  double ptVCoord = 0.0;
  
  // element loop: each process loops only on the elements that owns
  for(int iel = msh->_elementOffset[iproc]; iel < msh->_elementOffset[iproc + 1]; iel++) {

    // element geometry type
    short unsigned ielGeom = msh->GetElementType(iel);
    unsigned nDofsV = msh->GetElementDofNumber(iel, solVType);    // number of solution element dofs
    unsigned nDofsX = msh->GetElementDofNumber(iel, coordXType);    // number of coordinate element dofs

    for(unsigned  k = 0; k < dim; k++) {
      solV[k].resize(nDofsV);
      coordX[k].resize(nDofsX);
    }

    // local storage of global mapping and solution
    for(unsigned i = 0; i < nDofsV; i++) {
      unsigned solVDof = msh->GetSolutionDof(i, iel, solVType);    // global to global mapping between solution node and solution dof
      for(unsigned  k = 0; k < dim; k++) {
        solV[k][i] = (*sol->_Sol[solVIndex[k]])(solVDof);      // global extraction and local storage for the solution
      }
    }

    // local storage of coordinates
    for(unsigned i = 0; i < nDofsX; i++) {
      unsigned coordXDof  = msh->GetSolutionDof(i, iel, coordXType);    // global to global mapping between coordinates node and coordinate dof
      for(unsigned k = 0; k < dim; k++) {
        coordX[k][i] = (*msh->_topology->_Sol[k])(coordXDof);      // global extraction and local storage for the element coordinates
      }
    }

	if (recordCoord == 0){
		for(unsigned i = 0; i < nDofsX; i++) {
			if (fabs(coordX[0][i] + 1.875) < 1.0e-6 && fabs(coordX[1][i] + 0.375) < 1.0e-6) {
				ptUCoord = solV[0][i];
				ptVCoord = solV[1][i];
				recordCoord = 1;
			}
		}
	}
    // *** Gauss point loop ***
    for(unsigned ig = 0; ig < msh->_finiteElement[ielGeom][solVType]->GetGaussPointNumber(); ig++) {
      // *** get gauss point weight, test function and test function partial derivatives ***
      msh->_finiteElement[ielGeom][solVType]->Jacobian(coordX, ig, weight, phiV, phiV_x, phiV_xx);

      // evaluate the solution, the solution derivatives and the coordinates in the gauss point
      vector < double > solV_gss(dim, 0);
      vector < vector < double > > gradSolV_gss(dim);

      for(unsigned  k = 0; k < dim; k++) {
        gradSolV_gss[k].resize(dim);
        std::fill(gradSolV_gss[k].begin(), gradSolV_gss[k].end(), 0);
      }

      for(unsigned i = 0; i < nDofsV; i++) {
        for(unsigned  k = 0; k < dim; k++) {
          solV_gss[k] += phiV[i] * solV[k][i];
        }
        for(unsigned j = 0; j < dim; j++) {
          for(unsigned  k = 0; k < dim; k++) {
            gradSolV_gss[k][j] += phiV_x[i * dim + j] * solV[k][i];
          }
        }
      }
      
      for(unsigned  k = 0; k < dim; k++) kineticEnergy += solV_gss[k] * solV_gss[k] * weight;
      
    } // end gauss point loop
  } // end element loop for each process
  // add the kinetic energy of all process
  
  NumericVector* out_vec;
  out_vec = NumericVector::build().release();
  out_vec->init (msh->n_processors(), 1 , false, AUTOMATIC);
  
  out_vec->set (iproc, kineticEnergy);
  out_vec->close();
  kineticEnergy = out_vec->l1_norm();

  double ptCoord1, ptCoord2;
  out_vec->set (iproc, ptUCoord);
  out_vec->close();
  ptCoord1 = out_vec->max();
  ptCoord2 = out_vec->min();
  if (fabs (ptCoord1) > 1.0e-6) ptUCoord = ptCoord1;
  if (fabs (ptCoord2) > 1.0e-6) ptUCoord = ptCoord2;
  
  out_vec->set (iproc, ptVCoord);
  out_vec->close();
  ptCoord1 = out_vec->max();
  ptCoord2 = out_vec->min();
  if (fabs (ptCoord1) > 1.0e-6) ptVCoord = ptCoord1;
  if (fabs (ptCoord2) > 1.0e-6) ptVCoord = ptCoord2;
  delete out_vec;
  
  std::pair < double, vector <double> > out_value;
  out_value.first = kineticEnergy;
  
  vector <double> ptCoord(2);
  ptCoord[0] = ptUCoord;
  ptCoord[1] = ptVCoord; 
  out_value.second = ptCoord; 
  return out_value;
}
