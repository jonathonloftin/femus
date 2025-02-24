/**
 * Solve 
 *     - \Delta u = 1
*/


#include "FemusInit.hpp"
#include "MultiLevelSolution.hpp"
#include "MultiLevelProblem.hpp"
#include "VTKWriter.hpp"
#include "NonLinearImplicitSystem.hpp"
#include "NumericVector.hpp"

#include "CurrentElem.hpp"
#include "ElemType_template.hpp"

#include "Assemble_jacobian.hpp"
#include "Assemble_unknown_jacres.hpp"

using namespace femus;
 

/// @todo Laplace beltrami on a flat domain does not give the same numbers, need to check that


double InitialValueDS(const MultiLevelProblem * ml_prob, const std::vector < double >& x, const char name[]) {
    
  return 0.;
  
}


 
 
bool SetBoundaryCondition(const MultiLevelProblem * ml_prob, const std::vector < double >& x, const char name[], double& value, const int face_name, const double time) {

  bool dirichlet = false;
  value = 0.;
  
  const double tolerance = 1.e-5;
  
  
 if (ml_prob->GetMLMesh()->GetDimension() == 2 )  {
     
     
    if (face_name == 1) {
      dirichlet = true;
        value = 0.;
  }
  else if (face_name == 2) {
      dirichlet = true;
        value = 0.;
  }
  else if (face_name == 3) {
      dirichlet = true;
        value = 0.;
  }

   
   
 
 }

 
 
  return dirichlet;
  
 }

 
 
 //====== NEUMANN LOOP 1D =============================================   
void neumann_loop_1d(const MultiLevelProblem *    ml_prob, 
                     const Mesh *                    msh,
                     const MultiLevelSolution *    ml_sol, 
                     const unsigned iel,
                     CurrentElem < double > & geom_element,
                     const unsigned xType,
                     const std::string solname_u,
                     const unsigned solFEType_u,
                     std::vector< double > & Res
                    ) {
    
     double grad_u_dot_n;
    
    for (unsigned jface = 0; jface < msh->GetElementFaceNumber(iel); jface++) {
        
       geom_element.set_coords_at_dofs_bdry_3d(iel, jface, xType);
       
       geom_element.set_elem_center_bdry_3d();
       
       std::vector  <  double > xx_face_elem_center(3, 0.); 
          xx_face_elem_center = geom_element.get_elem_center_bdry_3d();
        
       const int boundary_index = msh->el->GetFaceElementIndex(iel, jface);
       
       if ( boundary_index < 0) { //I am on the boundary
                  
         unsigned int face = - (boundary_index + 1);
    
         bool is_dirichlet =  ml_sol->GetBdcFunctionMLProb()(ml_prob, xx_face_elem_center, solname_u.c_str(), grad_u_dot_n, face, 0.);                     
         //we have to be careful here, because in GenerateBdc those coordinates are passed as NODE coordinates, 
         //while here we pass the FACE ELEMENT CENTER coordinates. 
         // So, if we use this for enforcing space-dependent Dirichlet or Neumann values, we need to be careful!
         
             if ( !(is_dirichlet)  &&  (grad_u_dot_n != 0.) ) {  //dirichlet == false and nonhomogeneous Neumann
                 
                 
                 
                   unsigned n_dofs_face = msh->GetElementFaceDofNumber(iel, jface, solFEType_u);

                  for (unsigned i = 0; i < n_dofs_face; i++) {
                      
                 unsigned int i_vol = msh->GetLocalFaceVertexIndex(iel, jface, i);

                 Res[i_vol] +=  grad_u_dot_n /* * phi[node] = 1. */;
                 
                         }

                         
                         
                         
        
                    }
                  
              }
    }
    
}




template < class real_num, class real_num_mov >
void neumann_loop_2d3d(const MultiLevelProblem *    ml_prob, 
                       const Mesh *                    msh,
                       const MultiLevelSolution *    ml_sol, 
                       const unsigned iel,
                       CurrentElem < double > & geom_element,
                       const unsigned solType_coords,
                       const std::string solname_u,
                       const unsigned solFEType_u,
                       std::vector< double > & Res,
                       //-----------
                       std::vector < std::vector < /*const*/ elem_type_templ_base<real_num, real_num_mov> *  > >  elem_all,
                       const unsigned dim,
                       const unsigned space_dim,
                       const unsigned max_size
                    ) {
    
    
    /// @todo - should put these outside the iel loop --
    std::vector < std::vector < double > >  JacI_iqp_bdry(space_dim);
     std::vector < std::vector < double > >  Jac_iqp_bdry(dim-1);
    for (unsigned d = 0; d < Jac_iqp_bdry.size(); d++) {   Jac_iqp_bdry[d].resize(space_dim); }
    for (unsigned d = 0; d < JacI_iqp_bdry.size(); d++) { JacI_iqp_bdry[d].resize(dim-1); }
    
    double detJac_iqp_bdry;
  double weight_iqp_bdry = 0.;
// ---
  //boundary state shape functions
  vector <double> phi_u_bdry;  
  vector <double> phi_u_x_bdry; 

  phi_u_bdry.reserve(max_size);
  phi_u_x_bdry.reserve(max_size * space_dim);
// ---
  

    
    
     double grad_u_dot_n;
    
    for (unsigned jface = 0; jface < msh->GetElementFaceNumber(iel); jface++) {
        
       geom_element.set_coords_at_dofs_bdry_3d(iel, jface, solType_coords);
       
       geom_element.set_elem_center_bdry_3d();
       
       const unsigned ielGeom_bdry = msh->GetElementFaceType(iel, jface);    
       
       
       std::vector  <  double > xx_face_elem_center(3, 0.); 
          xx_face_elem_center = geom_element.get_elem_center_bdry_3d();
        
       const int boundary_index = msh->el->GetFaceElementIndex(iel, jface);
       
       if ( boundary_index < 0) { //I am on the boundary
                  
         unsigned int face = - (boundary_index + 1);
    
         bool is_dirichlet =  ml_sol->GetBdcFunctionMLProb()(ml_prob, xx_face_elem_center, solname_u.c_str(), grad_u_dot_n, face, 0.);                     
         //we have to be careful here, because in GenerateBdc those coordinates are passed as NODE coordinates, 
         //while here we pass the FACE ELEMENT CENTER coordinates. 
         // So, if we use this for enforcing space-dependent Dirichlet or Neumann values, we need to be careful!
         
             if ( !(is_dirichlet) /* &&  (grad_u_dot_n != 0.)*/ ) {  //dirichlet == false and nonhomogeneous Neumann
                 
                 
                 
                        const unsigned n_gauss_bdry = ml_prob->GetQuadratureRule(ielGeom_bdry).GetGaussPointsNumber();
        
    
		for(unsigned ig_bdry = 0; ig_bdry < n_gauss_bdry; ig_bdry++) {
  
     elem_all[ielGeom_bdry][solType_coords]->JacJacInv(geom_element.get_coords_at_dofs_bdry_3d(), ig_bdry, Jac_iqp_bdry, JacI_iqp_bdry, detJac_iqp_bdry, space_dim);
//      elem_all[ielGeom_bdry][solType_coords]->compute_normal(Jac_iqp_bdry, normal);
    
    weight_iqp_bdry = detJac_iqp_bdry * ml_prob->GetQuadratureRule(ielGeom_bdry).GetGaussWeightsPointer()[ig_bdry];
                
    elem_all[ielGeom_bdry][solFEType_u ]->shape_funcs_current_elem(ig_bdry, JacI_iqp_bdry, phi_u_bdry, phi_u_x_bdry,  boost::none, space_dim);
                 
                   unsigned n_dofs_face = msh->GetElementFaceDofNumber(iel, jface, solFEType_u);

                  for (unsigned i_bdry = 0; i_bdry < n_dofs_face; i_bdry++) {
                      
                 unsigned int i_vol = msh->GetLocalFaceVertexIndex(iel, jface, i_bdry);

                 Res[i_vol] +=  weight_iqp_bdry * grad_u_dot_n * phi_u_bdry[i_bdry];
                 
                           }
                         
                         
                         
                        }
        
                         
        
                    }
                  
              }
    }
    
}






 

template < class real_num, class real_num_mov >
void AssembleProblemDirNeu(MultiLevelProblem& ml_prob);



int main(int argc, char** args) {

  // init Petsc-MPI communicator
  FemusInit mpinit(argc, args, MPI_COMM_WORLD);

  // ======= Files ========================
  const bool use_output_time_folder = false;
  const bool redirect_cout_to_file = false;
  Files files; 
        files.CheckIODirectories(use_output_time_folder);
        files.RedirectCout(redirect_cout_to_file);

  // ======= Quad Rule ========================
  std::string fe_quad_rule("seventh");

    // ======= Mesh  ==================
   std::vector<std::string> mesh_files;
  
   // note to self: for a function V which meets the boundary conditions: V= x*y*(1 - (x^2 + y^2))
   // has a laplacian phi = -12*x*y
   
    mesh_files.push_back("Mesh_2_xy_assignment1_triangular.med");
    mesh_files.push_back("Mesh_2_xy_assignment1_quadrangle.med");
    
   //mesh_files.push_back("Mesh_1_x_dir_neu.med");
//    mesh_files.push_back("Mesh_2_xy_boundaries_groups_4x4.med");
//    mesh_files.push_back("Mesh_1_x_all_dir.med");
//    mesh_files.push_back("Mesh_1_y_all_dir.med");
//    mesh_files.push_back("Mesh_1_z_all_dir.med");
//    mesh_files.push_back("Mesh_2_xz_all_dir.med");
//    mesh_files.push_back("Mesh_2_yz_all_dir.med");
//    mesh_files.push_back("Mesh_3_xyz_all_dir.med");
//    mesh_files.push_back("dome_tri.med");
//    mesh_files.push_back("dome_quad.med");
//    mesh_files.push_back("disk_quad.med");
//    mesh_files.push_back("disk_quad_45x.med");
//    mesh_files.push_back("disk_quad_90x.med");
//    mesh_files.push_back("disk_tri.med");
//    mesh_files.push_back("disk_tri_45x.med");
//    mesh_files.push_back("disk_tri_90x.med");
   


 for (unsigned int m = 0; m < mesh_files.size(); m++)  {
   
  // ======= Mesh  ==================
  // define multilevel mesh
  MultiLevelMesh ml_mesh;
  double scalingFactor = 1.;
 
  const bool read_groups = true; //with this being false, we don't read any group at all. Therefore, we cannot even read the boundary groups that specify what are the boundary faces, for the boundary conditions
  const bool read_boundary_groups = true;
  
  std::string mesh_file_tot = "./input/" + mesh_files[m];
  
  ml_mesh.ReadCoarseMesh(mesh_file_tot.c_str(), fe_quad_rule.c_str(), scalingFactor, read_groups, read_boundary_groups);
//     ml_mesh.GenerateCoarseBoxMesh(2,0,0,0.,1.,0.,0.,0.,0.,EDGE3,fe_quad_rule.c_str());
//     ml_mesh.GenerateCoarseBoxMesh(0,2,0,0.,0.,0.,1.,0.,0.,EDGE3,fe_quad_rule.c_str());
 
  unsigned numberOfUniformLevels = /*1*/1;
  unsigned numberOfSelectiveLevels = 0;
  ml_mesh.RefineMesh(numberOfUniformLevels , numberOfUniformLevels + numberOfSelectiveLevels, NULL);
  ml_mesh.EraseCoarseLevels(numberOfUniformLevels + numberOfSelectiveLevels - 1);
  ml_mesh.PrintInfo();

  
  
  // ======= Solution  ==================
  MultiLevelSolution ml_sol(&ml_mesh);

  ml_sol.SetWriter(VTK);
  ml_sol.GetWriter()->SetDebugOutput(true);
  
  // ======= Problem ========================
  // define the multilevel problem attach the ml_sol object to it
  MultiLevelProblem ml_prob(&ml_sol);
  
  // add variables to ml_sol
  ml_sol.AddSolution("d_s", LAGRANGE, FIRST/*DISCONTINUOUS_POLYNOMIAL, ZERO*/);
  
  // ======= Solution: Initial Conditions ==================
  ml_sol.Initialize("All");    // initialize all variables to zero
  ml_sol.Initialize("d_s", InitialValueDS, & ml_prob);

  // ======= Solution: Boundary Conditions ==================
  ml_sol.AttachSetBoundaryConditionFunction(SetBoundaryCondition);
  ml_sol.GenerateBdc("d_s", "Steady",  & ml_prob);

  

  // ======= Problem, II ========================
  ml_prob.SetFilesHandler(&files);
  ml_prob.SetQuadratureRuleAllGeomElems(fe_quad_rule);
  ml_prob.set_all_abstract_fe_multiple();
  
//   std::vector < std::vector < const elem_type_templ_base<double, double> *  > > elem_all = ml_prob.evaluate_all_fe<double, double>();
  
    // ======= System ========================
 // add system  in ml_prob as a Linear Implicit System
  NonLinearImplicitSystem& system = ml_prob.add_system < NonLinearImplicitSystem > ("Poisson");
  
  system.SetDebugNonlinear(true);
 
  system.AddSolutionToSystemPDE("d_s");
 
  // attach the assembling function to system
  system.SetAssembleFunction(AssembleProblemDirNeu<double, double>);

//   system.SetMaxNumberOfLinearIterations(2);
  // initialize and solve the system
  system.SetMgType(V_CYCLE/*F_CYCLE*//*M_CYCLE*/); //it doesn't matter if I use only 1 level


  system.SetOuterSolver(GMRES);
 
  system.init();
  
  system.MGsolve();
  
    // ======= Print ========================
  // print solutions
  const std::string print_order = "biquadratic"; //"linear", "quadratic", "biquadratic"
  std::vector < std::string > variablesToBePrinted;
  variablesToBePrinted.push_back("all");
 
  ml_sol.GetWriter()->Write(mesh_files[m], files.GetOutputPath(), print_order.c_str(), variablesToBePrinted);
  
  }
 
 
  return 0;
}





template < class real_num, class real_num_mov >
void AssembleProblemDirNeu(MultiLevelProblem& ml_prob) {

  NonLinearImplicitSystem* mlPdeSys  = &ml_prob.get_system<NonLinearImplicitSystem> ("Poisson");  
  const unsigned level = mlPdeSys->GetLevelToAssemble();
  const bool assembleMatrix = mlPdeSys->GetAssembleMatrix();

  Mesh*                    msh = ml_prob._ml_msh->GetLevel(level);
  elem*                     el = msh->el;

  MultiLevelSolution*    ml_sol = ml_prob._ml_sol;
  Solution*                sol = ml_prob._ml_sol->GetSolutionLevel(level);

  LinearEquationSolver* pdeSys = mlPdeSys->_LinSolver[level];
  SparseMatrix*             JAC = pdeSys->_KK;
  NumericVector*           RES = pdeSys->_RES;

  const unsigned  dim = msh->GetDimension();
  unsigned dim2 = (3 * (dim - 1) + !(dim - 1));
  const unsigned maxSize = static_cast< unsigned >(ceil(pow(3, dim)));

  unsigned    iproc = msh->processor_id();

  //=============== Geometry ========================================
  unsigned xType = BIQUADR_FE; // the FE for the domain need not be biquadratic
  
  CurrentElem < double > geom_element(dim, msh);            // must be adept if the domain is moving, otherwise double
    
  constexpr unsigned int space_dim = 3;
//***************************************************  


 //******************** quadrature *******************************  
  double jacXweight_qp; 

 //********************* unknowns *********************** 
 //***************************************************  
  const int n_vars = mlPdeSys->GetSolPdeIndex().size();
  std::cout << "************" << n_vars << "************";
  std::vector <double> phi_u;
  std::vector <double> phi_u_x; 
  std::vector <double> phi_u_xx;

  phi_u.reserve(maxSize);
  phi_u_x.reserve(maxSize * space_dim);
  phi_u_xx.reserve(maxSize * dim2);
  
  const std::string solname_u = "d_s";
  unsigned solIndex_u;
  solIndex_u = ml_sol->GetIndex(solname_u.c_str()); 
  unsigned solFEType_u = ml_sol->GetSolutionType(solIndex_u); 

  unsigned solPdeIndex_u;
  solPdeIndex_u = mlPdeSys->GetSolPdeIndex(solname_u.c_str());

  std::vector < double >  sol_u;     sol_u.reserve(maxSize);
  std::vector< int > l2GMap_u;    l2GMap_u.reserve(maxSize);
 //***************************************************  
 //***************************************************  

  
 //***************************************************  
 //********* WHOLE SET OF VARIABLES ****************** 

  std::vector< int > l2GMap_AllVars; l2GMap_AllVars.reserve(n_vars*maxSize); // local to global mapping
  std::vector< double >         Res;            Res.reserve(n_vars*maxSize);  // local redidual vector
  std::vector < double >        Jac;            Jac.reserve(n_vars*maxSize * n_vars*maxSize);
 //***************************************************  

  RES->zero();
  if (assembleMatrix)  JAC->zero();

  
 //***************************************************  
     std::vector < std::vector < real_num_mov > >  JacI_qp(space_dim);
     std::vector < std::vector < real_num_mov > >  Jac_qp(dim);
    for (unsigned d = 0; d < dim; d++) { Jac_qp[d].resize(space_dim); }
    for (unsigned d = 0; d < space_dim; d++) { JacI_qp[d].resize(dim); }
    
    real_num_mov detJac_qp;
    
    std::vector < std::vector < real_num_mov > >  JacJacT(dim);
    for (unsigned d = 0; d < dim; d++) { JacJacT[d].resize(dim); }

    std::vector < std::vector < real_num_mov > >  JacJacT_inv(dim);
    for (unsigned d = 0; d < dim; d++) { JacJacT_inv[d].resize(dim); }

  //prepare Abstract quantities for all fe fams for all geom elems: all quadrature evaluations are performed beforehand in the main function
  std::vector < std::vector < /*const*/ elem_type_templ_base<real_num, real_num_mov> *  > > elem_all;
  ml_prob.get_all_abstract_fe(elem_all);
 //***************************************************  
  


  // element loop: each process loops only on the elements that owns
  for (int iel = msh->_elementOffset[iproc]; iel < msh->_elementOffset[iproc + 1]; iel++) {
      
      //std::cout << "iel is: " << iel << std::endl; // iel stands for the ith element

    geom_element.set_coords_at_dofs_and_geom_type(iel, xType);
        
    const short unsigned ielGeom = geom_element.geom_type();

 //**************** state **************************** 
    unsigned nDof_u     = msh->GetElementDofNumber(iel, solFEType_u);
    sol_u    .resize(nDof_u);
    l2GMap_u.resize(nDof_u);
   // local storage of global mapping and solution
    for (unsigned i = 0; i < sol_u.size(); i++) {
     unsigned solDof_u = msh->GetSolutionDof(i, iel, solFEType_u);
      sol_u[i] = (*sol->_Sol[solIndex_u])(solDof_u);
      l2GMap_u[i] = pdeSys->GetSystemDof(solIndex_u, solPdeIndex_u, i, iel);
    }
 //***************************************************  
 
 //******************** ALL VARS ********************* 
    unsigned nDof_AllVars = nDof_u; 
    int nDof_max    =  nDof_u;   // TODO COMPUTE MAXIMUM maximum number of element dofs for one scalar variable
    
    Res.resize(nDof_AllVars);                  std::fill(Res.begin(), Res.end(), 0.);
    Jac.resize(nDof_AllVars * nDof_AllVars);   std::fill(Jac.begin(), Jac.end(), 0.);
    l2GMap_AllVars.resize(0);                  l2GMap_AllVars.insert(l2GMap_AllVars.end(),l2GMap_u.begin(),l2GMap_u.end());
 //*************************************************** 
    

 //========= BOUNDARY ==================   
    if (dim == 1)   neumann_loop_1d(& ml_prob, msh, ml_sol,
                      iel, geom_element, xType,
                      solname_u, solFEType_u,
                      Res
                     );

    if (dim == 2 || dim == 3)   neumann_loop_2d3d(& ml_prob, msh, ml_sol,
                      iel, geom_element, xType,
                      solname_u, solFEType_u,
                      Res,
                      elem_all,
                      dim,
                      space_dim,
                      maxSize
                     );
 
 //========= VOLUME ==================   
   
 //========= gauss value quantities ==================   
	std::vector<double> sol_u_x_gss(space_dim);     std::fill(sol_u_x_gss.begin(), sol_u_x_gss.end(), 0.);
 //===================================================   
    
    
    
    
      // *** Quadrature point loop ***
      for (unsigned i_qp = 0; i_qp < ml_prob.GetQuadratureRule(ielGeom).GetGaussPointsNumber(); i_qp++) {
          
        // *** get gauss point weight, test function and test function partial derivatives ***
// 	msh->_finiteElement[ielGeom][solFEType_u]->Jacobian(geom_element.get_coords_at_dofs_3d(),    i_qp, weight,    phi_u,    phi_u_x,    boost::none /*phi_u_xx*/);
          
    
          
	elem_all[ielGeom][xType]->JacJacInv(geom_element.get_coords_at_dofs_3d(), i_qp, Jac_qp, JacI_qp, detJac_qp, space_dim);
    jacXweight_qp = detJac_qp * ml_prob.GetQuadratureRule(ielGeom).GetGaussWeightsPointer()[i_qp];
    elem_all[ielGeom][solFEType_u]->shape_funcs_current_elem(i_qp, JacI_qp, phi_u, phi_u_x, boost::none /*phi_u_xx*/, space_dim);

    elem_all[ielGeom][xType]->jac_jacT(Jac_qp, JacJacT, space_dim);
    elem_all[ielGeom][xType]->jac_jacT_inv(JacJacT, JacJacT_inv, space_dim);

    
    std::cout << "ielGeom iqp= " << i_qp << " .... What is this?" << std::endl;
    std::cout << *(ml_prob.GetQuadratureRule(ielGeom).GetGaussCoordinatePointer(i_qp)) << std::endl;
    
    
    // the coords that you get in this way are clearly already in the classical form - may need to back-out of classical in order to properly evaluate \Delta Uo
    
    // xi-location of quadrature point
    //for (unsigned i = 0; i < nDof_u; i++){
    //    x_qp[i_qp] +=  x * phi_u[i];
    //    y_qp[i_qp] += y * phi_u[i];
    //} 
          // yi-location of quadrature point
          
          
          // x-location of quadrature point ("real")
          // y-location of quadrature point ("real")
    
    
    
//--------------    
	std::fill(sol_u_x_gss.begin(), sol_u_x_gss.end(), 0.);
	
	for (unsigned i = 0; i < nDof_u; i++) {
// 	                                                sol_u_gss      += sol_u[i] * phi_u[i];
                   for (unsigned d = 0; d < sol_u_x_gss.size(); d++)   sol_u_x_gss[d] += sol_u[i] * phi_u_x[i * space_dim + d];
          }
//--------------    
          
//==========FILLING WITH THE EQUATIONS ===========
	// *** phi_i loop ***
        for (unsigned i = 0; i < nDof_max; i++) {
	  
//--------------    
	      double laplace_res_du_u_i = 0.;
              if ( i < nDof_u ) {
                  for (unsigned kdim = 0; kdim < space_dim; kdim++) {
                       laplace_res_du_u_i             +=  phi_u_x   [i * space_dim + kdim] * sol_u_x_gss[kdim];
	             }
              }
//--------------    
              
// removed laplace_beltrami  
	      
//======================Residuals=======================
          // FIRST ROW
          
          
          
          if (i < nDof_u)                      Res[0      + i] +=  jacXweight_qp * ( phi_u[i] * (  100.0 ) - laplace_res_du_u_i);
//           if (i < nDof_u)                      Res[0      + i] += jacXweight_qp * ( phi_u[i] * (  1. ) - laplace_beltrami_res_du_u_i);
//======================Residuals=======================
	      
          if (assembleMatrix) {
	    
            // *** phi_j loop ***
            for (unsigned j = 0; j < nDof_max; j++) {


//--------------    
              double laplace_mat_du_u_i_j = 0.;

              if ( i < nDof_u && j < nDof_u ) {
                  for (unsigned kdim = 0; kdim < space_dim; kdim++) {
                         laplace_mat_du_u_i_j           += phi_u_x   [i * space_dim + kdim] *
                                                           phi_u_x   [j * space_dim + kdim];
	              }
             }
//--------------    


// removed laplace_beltrami  

              //============ delta_state row ============================
              //DIAG BLOCK delta_state - state
		  if ( i < nDof_u && j < nDof_u )       Jac[ (0 + i) * nDof_AllVars   + 	(0 + j) ]  += jacXweight_qp * laplace_mat_du_u_i_j;
// 		  if ( i < nDof_u && j < nDof_u )       Jac[ (0 + i) * nDof_AllVars   + 	(0 + j) ]  += jacXweight_qp * laplace_beltrami_mat_du_u_i_j; ///@todo On a flat domain, this must coincide with the standard Laplacian, so we can do a double check with this
            } // end phi_j loop
          } // endif assemble_matrix

        } // end phi_i loop
        
      } // end gauss point loop


    RES->add_vector_blocked(Res, l2GMap_AllVars);

    if (assembleMatrix) {
      JAC->add_matrix_blocked(Jac, l2GMap_AllVars, l2GMap_AllVars);
    }
    
  } //end element loop for each process

  RES->close();

  if (assembleMatrix) JAC->close();

     //print JAC and RES to files
    const unsigned nonlin_iter = 0/*mlPdeSys->GetNonlinearIt()*/;
    assemble_jacobian< double, double >::print_global_jacobian(assembleMatrix, ml_prob, JAC, nonlin_iter);
    assemble_jacobian< double, double >::print_global_residual(ml_prob, RES, nonlin_iter);
  

  // ***************** END ASSEMBLY *******************

  return;
}

