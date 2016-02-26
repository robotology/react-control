/* 
 * Copyright: (C) 2015 iCub Facility - Istituto Italiano di Tecnologia
 * Author: Alessandro Roncone <alessandro.roncone@iit.it>
 * website: www.robotcub.org
 * author website: http://alecive.github.io
 * 
 * Permission is granted to copy, distribute, and/or modify this program
 * under the terms of the GNU General Public License, version 2 or any
 * later version published by the Free Software Foundation.
 *
 * A copy of the license can be found at
 * http://www.robotcub.org/icub/license/gpl.txt
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
*/

#include <limits>
#include <sstream>
#include <cmath>

#include <IpTNLP.hpp>
#include <IpIpoptApplication.hpp>

#include "assert.h"

#include "reactIpOpt.h"

#define CAST_IPOPTAPP(x)             (static_cast<IpoptApplication*>(x))
#define EXTRA_MARGIN_SHOULDER_INEQ_RAD 0.05 //each of the ineq. constraints for shoulder joints will have an extra safety marging of 0.05 rad on each side - i.e. the actual allowed range will be smaller
#define EXTRA_MARGIN_GENERAL_INEQ_RAD 0.0

using namespace std;
using namespace yarp::os;
using namespace yarp::sig;
using namespace yarp::math;
using namespace iCub::ctrl;
using namespace iCub::iKin;
using namespace iCub::skinDynLib;
using namespace Ipopt;


/*************optimization problem representation********************************************************/
class react_NLP : public TNLP
{
private:
    // Copy constructor: not implemented.
    react_NLP(const react_NLP&);
    // Assignment operator: not implemented.
    react_NLP &operator=(const react_NLP&);

protected:
    // The name of the class instance (fixed for now to react_NLP)
    string name;    
    bool useMemory;
    bool useFilter;
    // The verbosity level (fixed for now to 0)
    int verbosity;
    // The chain that will undergo the task - N.B. if the controller is using a model, the chain received has been already set to the configuration (modeled joint positions)
    iKinChain &chain;
    // The dimensionality of the task ~ nr DOF in the chain ~ nr. primal variables (for now 7 for arm joints, 10 if torso is enabled)
    unsigned int dim;

    // The desired position to attain
    yarp::sig::Vector xd;
    // The current position
    yarp::sig::Vector x0;

    // The delta T with which ipopt needs to solve the task - time in the task, not comp. time - delta x = delta T * J * q_dot 
    double dT;
    // The joint limits; joints in rows; first col minima, second col maxima
    //will be extracted from the chain
    yarp::sig::Matrix q_lim; 
    //N.B. Here within IPOPT, we will keep everyting in radians - joint pos in rad, joint vel in rad/s; same for limits 
    // The desired final joint velocities - the solution of this iteration 
    yarp::sig::Vector q_dot_d;
    // The initial joint velocities - solution from prev. step 
    yarp::sig::Vector q_dot_0;
    // The current joint velocities - for internal steps of ipopt, not carrying a particular meaning
    yarp::sig::Vector q_dot;
    // The maximum allowed speed at the joints; joints in rows; first col minima, second col maxima
    //They may be set adaptively by avoidanceHandler from the outside - currently through constructor
    yarp::sig::Matrix v_lim;
    yarp::sig::Matrix v_bounds; //identical structure like v_lim, but ensures that joint pos limits will be respected
    
    // The cost function
    yarp::sig::Vector cost_func;
    // The gradient of the cost function
    yarp::sig::Vector grad_cost_func;
    
    yarp::sig::Matrix J0;
    // The jacobian associated with the cost function
    //yarp::sig::Matrix J_cst;

    // Weights set to the joint limits
    yarp::sig::Vector W;
    // Derivative of said weights w.r.t q
    yarp::sig::Vector W_dot;
    // The minimum weight given to the joint limits bound function (1.0)
    double W_min;   
    // The gamma of the weight given to the joint limits bound function (W_min+W_gamma=W_max, default 3.0)
    double W_gamma; 
    // The guard ratio 
    double guardRatio;

    // The torso reduction rate at for the max velocities sent to the torso(default 3.0)
    double torsoReduction;  
    //for smooth changes in joint velocities
    yarp::sig::Vector kps; //for modeling the motor behavior
    iCub::ctrl::Filter *fi;
    yarp::sig::Vector filt_num;
    yarp::sig::Vector filt_den;
    std::deque<yarp::sig::Vector> filt_u;
    std::deque<yarp::sig::Vector> filt_y;
    
    bool boundSmoothnessFlag; 
    double boundSmoothnessValue; //new joint vel can differ by max boundSmoothness from the previous one (rad)
    
    //joint position limits handled in a smooth way - see Pattacini et al. 2010 - iCub Cart. control - Fig. 8
    yarp::sig::Vector qGuard;
    yarp::sig::Vector qGuardMinInt, qGuardMinExt, qGuardMinCOG;
    yarp::sig::Vector qGuardMaxInt, qGuardMaxExt, qGuardMaxCOG;
    
    bool APPLY_INEQ_CONSTRAINTS; 
    double shou_m, shou_n, elb_m, elb_n; //for ineq constraints

    bool firstGo;

    /************************************************************************/
    virtual void computeQuantities(const Number *x)
    {
        printMessage(9,"[computeQuantities] START dim: %i \n", dim);
        yarp::sig::Vector new_q_dot(dim,0.0);

        for (Index i=0; i<(int)dim; i++)
        {
            new_q_dot[i]=x[i];
        }
        if (!(q_dot==new_q_dot) || firstGo)
        {
            firstGo=false;
            q_dot=new_q_dot;
            //yarp::sig::Matrix J1=chain.GeoJacobian();
            //submatrix(J1,J_cst,0,2,0,dim-1);
            
            yarp::sig::Vector filt_q_dot(dim,0.0);
            if (useFilter){
                filt_q_dot=filt_num[0]*q_dot;
                for (size_t j=1; j<filt_num.size(); j++)
                    filt_q_dot+=filt_num[j]*filt_u[j-1];
                for (size_t k=1; k<filt_den.size(); k++)
                    filt_q_dot-=filt_den[k]*filt_y[k-1];
                filt_q_dot/=filt_den[0];
                //printf("[react_NLP:computeQuantities]: q_dot: %s\n",q_dot.toString().c_str());
                //printf("[react_NLP:computeQuantities]: filt_q_dot: %s\n",filt_q_dot.toString().c_str());
                //printf("[react_NLP:computeQuantities]: filt_num[0]/filt_den[0]: %f \n",filt_num[0]/filt_den[0]);
            }
            else{
                    filt_q_dot = q_dot;
            }
            //printf("[react_NLP:computeQuantities]: q_dot: %s\n",q_dot.toString().c_str());
            //printf("[react_NLP:computeQuantities]: filt_q_dot: %s\n",filt_q_dot.toString().c_str());
            //printf("[react_NLP:computeQuantities]: filt_num[0]/filt_den[0]: %f \n",filt_num[0]/filt_den[0]);
            if (useMemory){
                yarp::sig::Vector filt_q_dot_after_kp = filt_q_dot;
                for (size_t l=0; l<dim; l++)
                    filt_q_dot_after_kp[l] =filt_q_dot_after_kp[l] * kps[l];
                    
                 cost_func=xd-(x0 + dT*(J0*filt_q_dot_after_kp)); 
                 // Ugo's: delta_x=xr-(x0+(kp*dt)*(J0*filt_v));
                for (Ipopt::Index i=0; i<dim; i++)
                   grad_cost_func[i]=-2.0*(kps[i]*dT)*dot(cost_func,J0.getCol(i)*(filt_num[0]/filt_den[0]));
                     //Ugo's: grad_cost_func=2.0*cost_func*(-kp*dT*J_cst)*(filt_num[0]/filt_den[0]); 
                //cost_func=xd -(x0 + kp*dT*J_cst*filt_q_dot);    
                //grad_cost_func=2.0*cost_func*(-kp*dT*J_cst)*(filt_num[0]/filt_den[0]);
            }
            else{   
                printMessage(9,"[react_NLP:computeQuantities]: ipopt running with no memory flag.\n");
                cost_func=xd-(x0 + (J0*filt_q_dot));    
                // Ugo's with memory: delta_x=xr-(x0+(kp*dt)*(J0*filt_v));
                for (Ipopt::Index i=0; i<dim; i++)
                    grad_cost_func[i]=-2.0*(dT)*dot(cost_func,J0.getCol(i)*(filt_num[0]/filt_den[0]));
                     //Ugo's, with memory:   grad_f[i]=-2.0*(kp*dt)*dot(delta_x,J0.getCol(i)*(filt_num[0]/filt_den[0]));
            }
            printMessage(10,"[react_NLP:computeQuantities]: cost func: %s\n",cost_func.toString().c_str());
            printMessage(10,"[react_NLP:computeQuantities]: grad cost func: %s\n",grad_cost_func.toString().c_str());
           
           
        }
        printMessage(9,"[computeQuantities] OK x: %s\n",IPOPT_Number_toString(x,CTRL_RAD2DEG).c_str());
    }

    /************************************************************************/
    string IPOPT_Number_toString(const Number* x, const double multiplier=1.0)
    {
        std::ostringstream ss;
        for (Index i=0; i<(Index)dim; i++)
        {
            ss << x[i]*multiplier << " ";
        }
        return ss.str();
    }

    
    /********to ensure joint pos limits but in a smooth way around the limits****************************************/
    void computeGuard()
    {
        for (unsigned int i=0; i<dim; i++)
        {
            qGuard[i]=0.25*guardRatio*(chain(i).getMax()-chain(i).getMin());

            qGuardMinExt[i]=chain(i).getMin()+qGuard[i];
            qGuardMinInt[i]=qGuardMinExt[i]  +qGuard[i];
            qGuardMinCOG[i]=0.5*(qGuardMinExt[i]+qGuardMinInt[i]);

            qGuardMaxExt[i]=chain(i).getMax()-qGuard[i];
            qGuardMaxInt[i]=qGuardMaxExt[i]  -qGuard[i];
            qGuardMaxCOG[i]=0.5*(qGuardMaxExt[i]+qGuardMaxInt[i]);
        }

        printMessage(4,"qGuard       %s\n",(CTRL_RAD2DEG*qGuard).toString(3,3).c_str());
        printMessage(4,"qGuardMinExt %s\n",(CTRL_RAD2DEG*qGuardMinExt).toString(3,3).c_str());
        printMessage(4,"qGuardMinCOG %s\n",(CTRL_RAD2DEG*qGuardMinCOG).toString(3,3).c_str());
        printMessage(4,"qGuardMinInt %s\n",(CTRL_RAD2DEG*qGuardMinInt).toString(3,3).c_str());
        printMessage(4,"qGuardMaxInt %s\n",(CTRL_RAD2DEG*qGuardMaxInt).toString(3,3).c_str());
        printMessage(4,"qGuardMaxCOG %s\n",(CTRL_RAD2DEG*qGuardMaxCOG).toString(3,3).c_str());
        printMessage(4,"qGuardMaxExt %s\n",(CTRL_RAD2DEG*qGuardMaxExt).toString(3,3).c_str());
    }
    
    /***********velocity limits are shaped to guarantee that joint pos limits are respected******************************************/
    void computeBounds()
    {
        for (size_t i=0; i<dim; i++)
        {
            double qi=chain(i).getAng();
            if ((qi>=qGuardMinInt[i]) && (qi<=qGuardMaxInt[i]))
                v_bounds(i,0)=v_bounds(i,1)=1.0;
            else if ((qi<=qGuardMinExt[i]) || (qi>=qGuardMaxExt[i]))
                v_bounds(i,0)=v_bounds(i,1)=0.0;
            else if (qi<qGuardMinInt[i])
            {
                v_bounds(i,0)=0.5*(1.0+tanh(+10.0*(qi-qGuardMinCOG[i])/qGuard[i]));
                v_bounds(i,1)=1.0;
            }
            else
            {
                v_bounds(i,0)=1.0;
                v_bounds(i,1)=0.5*(1.0+tanh(-10.0*(qi-qGuardMaxCOG[i])/qGuard[i]));
            }
        }
        
        for (size_t i=0; i<dim; i++)
        {
            v_bounds(i,0)*=v_lim(i,0); //we scale the limits imposed from outside by these extra bounds and set it to v_bounds
            v_bounds(i,1)*=v_lim(i,1);
        }
        
        printMessage(4,"computeBounds (deg) \n      %s\n",(CTRL_RAD2DEG*v_bounds).toString(3,3).c_str());
      
    }
    
    /************************************************************************/
    int printMessage(const int l, const char *f, ...) const
    {
        if (verbosity>=l)
        {
            fprintf(stdout,"[%s] ",name.c_str());

            va_list ap;
            va_start(ap,f);
            int ret=vfprintf(stdout,f,ap);
            va_end(ap);
            return ret;
        }
        else
            return -1;
    }


public:
    /***** 8 pure virtual functions from TNLP class need to be implemented here **********/
    /************************************************************************/
    react_NLP(iKinChain &c, const yarp::sig::Vector &_xd, const yarp::sig::Vector &_q_dot_0,std::deque<yarp::sig::Vector> &_q_dot_memory, 
             double _dT, const yarp::sig::Matrix &_v_lim, const bool _useMemory, const yarp::sig::Vector _kps, const bool _useFilter, iCub::ctrl::Filter *_fi, bool _boundSmoothnessFlag, double _boundSmoothnessValue,int _verbosity) : chain(c), xd(_xd), q_dot_0(_q_dot_0),dT(_dT), v_lim(_v_lim), useMemory(_useMemory), kps(_kps),useFilter(_useFilter), fi(_fi),boundSmoothnessFlag(_boundSmoothnessFlag),boundSmoothnessValue(_boundSmoothnessValue),
             verbosity(_verbosity) 
    {
        name="react_NLP";
        
        printMessage(10,"react_NLP()");
        // Time should always be positive
        if (dT<0.0)
           dT=0.05;
        x0.resize(3,0.0);
        
        dim=chain.getDOF(); //only active (not blocked) joints - e.g. 7 for the arm joints, 10 if torso is included
        //printf("[react_NLP()]: dim ~ nrDOF of chain: %d \n",dim);
     
        if (!((dim == 7) || (dim == 10)) ){ 
            yError("react_NLP(): unexpected nr DOF on the chain : %d\n",dim);
        }
        
        yAssert(kps.length() == dim);
        for (int i=0;i<dim;i++){
            if (kps[i] < 0.0)
                kps[i] = 1.0;
        }
        
        if (useMemory){
            yAssert(dim == _q_dot_memory[0].length());
            q_lim.resize(dim,2);
            for (size_t r=0; r<dim; r++)
            {
                q_lim(r,0)=chain(r).getMin();
                q_lim(r,1)=chain(r).getMax();
            }
            Integrator I(dT,chain.getAng(),q_lim); // we initialize the integrator to the current joint pos 
            for (size_t i=0; i<_q_dot_memory.size(); i++)
                I.integrate(kps[i]*_q_dot_memory[i]);
            chain.setAng(I.get()); //! Now we set the chain to the predicted configuration after n time steps corresponding to the buffer size as per the lag td of the motor model - ipOpt will be asked to solve the problem there
            x0=chain.EndEffPosition();
        }
        
        J0=chain.GeoJacobian().submatrix(0,2,0,dim-1);
        
        if(useFilter){
            fi->getCoeffs(filt_num,filt_den);
            fi->getStates(filt_u,filt_y);
      
            /*printf("[react_NLP()] filter states: \n");
            printf("numerator: %s: \n",filt_num.toString(3,3).c_str());
            printf("denominator: %s: \n",filt_den.toString(3,3).c_str());
            printf("filt_u input states: ");
            std::deque<yarp::sig::Vector>::iterator it = filt_u.begin();
            while (it != filt_u.end()){
                printf("%s \n",(*it).toString(3,3).c_str());
                it++;
            }
            printf("filt_y output states: ");
            std::deque<yarp::sig::Vector>::iterator it2 = filt_y.begin();
            while (it2 != filt_y.end()){
                printf("%s \n",(*it2).toString(3,3).c_str());
                it2++;
            }*/
        }
        else{
           filt_num.resize(1,1.0);
           filt_den.resize(1,1.0);
        }
        printMessage(10,"react_NLP(): filters initialized\n");
        
        q_dot.resize(dim,0.0);
        q_dot_d.resize(dim,0.0);
        
        v_bounds = v_lim; //will be initialized like that, but then inside computeBounds actually computed
        
        cost_func.resize(3,0.0); //cost function is defined in cartesian space - position of end-effector
        grad_cost_func.resize(dim,0.0); //these are derivatives with respect to the number of joints
        
        //J_cst.resize(3,dim); J_cst.zero(); // The jacobian associated with the cost function

        W.resize(dim,0.0);
        W_dot.resize(dim,0.0);
        W_min=1.0;
        W_gamma=3.0;
        guardRatio=0.1; // changing from 0.4 (orig Ale) to 0.1 - to comply with reactController-sim where the guard will be used to adapt velocity limits;

        torsoReduction=3.0;
       
        qGuard.resize(dim,0.0);
        qGuardMinInt.resize(dim,0.0);
        qGuardMinExt.resize(dim,0.0);
        qGuardMaxInt.resize(dim,0.0);
        qGuardMaxExt.resize(dim,0.0);
        qGuardMinCOG.resize(dim,0.0);
        qGuardMaxCOG.resize(dim,0.0);
        
                
        APPLY_INEQ_CONSTRAINTS = true;
        if (APPLY_INEQ_CONSTRAINTS){
            double joint1_0, joint1_1;
            double joint2_0, joint2_1;
            joint1_0= 28.0*CTRL_DEG2RAD;
            joint1_1= 23.0*CTRL_DEG2RAD;
            joint2_0=-37.0*CTRL_DEG2RAD;
            joint2_1= 80.0*CTRL_DEG2RAD;
            shou_m=(joint1_1-joint1_0)/(joint2_1-joint2_0);
            shou_n=joint1_0-shou_m*joint2_0;

            double joint3_0, joint3_1;
            double joint4_0, joint4_1;
            joint3_0= 85.0*CTRL_DEG2RAD;
            joint3_1=105.0*CTRL_DEG2RAD;
            joint4_0= 90.0*CTRL_DEG2RAD;
            joint4_1= 40.0*CTRL_DEG2RAD;
            elb_m=(joint4_1-joint4_0)/(joint3_1-joint3_0);
            elb_n=joint4_0-elb_m*joint3_0;
        }
        else{
                shou_m = 0.0; shou_n=0.0; elb_m = 0.0; elb_n=0.0;
        }
            
        printMessage(10,"react_NLP(): will call computeGuard\n");
        computeGuard();
        printMessage(10,"react_NLP(): will call computeBounds\n");
        computeBounds();
        
        firstGo=true;
    }
    
    /************************************************************************/
    yarp::sig::Vector get_q_dot_d() { return q_dot_d; }

    /************************************************************************/
    bool get_nlp_info(Index& n, Index& m, Index& nnz_jac_g, Index& nnz_h_lag,
                      IndexStyleEnum& index_style)
    {
        n=dim; // number of vars in the problem (dim(x)) ~ n DOF in chain in our case 
        if (APPLY_INEQ_CONSTRAINTS){
            m=6; // nr constraints - dim(g(x))
            nnz_jac_g= 13; // the jacobian of g has dim non zero entries
        }
        else{
            m=0; // nr constraints - dim(g(x))
            nnz_jac_g= 0; // the jacobian of g has dim non zero entries
        }
        nnz_h_lag=0; //number of nonzero entries in the Hessian
        index_style=TNLP::C_STYLE;
        printMessage(7,"[get_nlp_info]\tn: %i m: %i nnz_jac_g: %i\n",n,m,nnz_jac_g);
        
        return true;
    }
    
    /************************************************************************/
    bool get_starting_point(Index n, bool init_x, Number* x, bool init_z,
                            Number* z_L, Number* z_U, Index m, bool init_lambda,
                            Number* lambda)
    {
        assert(init_x == true); //initial value for x
        assert(init_z == false); // false => no initial value for bound multipliers z_L, z_U
        assert(init_lambda == false); // false => no initial value for constraint multipliers

        for (Index i=0; i<n; i++){
            x[i]=q_dot_0[i]; //initializing the primal variables 
            //inside IPOPT, the variables optimized ("primal variables") are x (in our case they are q_dot - 
            // watch out that you don't confuse them with x - Cartesian pos)
        }
        printMessage(3,"[get_starting_pnt] x_0: %s\n",IPOPT_Number_toString(x,CTRL_RAD2DEG).c_str());

        return true;
    }

    /************************************************************************/
    bool get_bounds_info(Index n, Number* x_l, Number* x_u, Index m, Number* g_l,
                         Number* g_u)
    {
        //x_l, x_u - limits for the primal variables - in our case joint velocities
  
        for (Index i=0; i<n; i++)
        {
            if (boundSmoothnessFlag)
            {
                // The joints velocities will be constrained by the v_lim constraints (after possible external modification by avoidanceHandlers) and 
                // and the previous state (that is our current initial state), in order to avoid abrupt changes
                double smoothnessMin = std::min(v_bounds(i,1),q_dot_0[i]-boundSmoothnessValue); //the smooth min vel should not be bigger than max vel
                double smoothnessMax = std::max(v_bounds(i,0),q_dot_0[i]+boundSmoothnessValue); //the smooth max vel should not be smaller than min vel
                
                x_l[i]=max(v_bounds(i,0),smoothnessMin); //lower bound
                x_u[i]=min(v_bounds(i,1),smoothnessMax); //upper bound
                
                
                if (n==10 && i<3) //special handling of torso joints - should be moving less
                {
                    x_l[i]=max(v_bounds(i,0)/torsoReduction,smoothnessMin);
                    x_u[i]=min(v_bounds(i,1)/torsoReduction,smoothnessMax);
                }

                // printf("-V_max*CTRL_DEG2RAD %g\tq_dot_0[i]-boundSmoothness*CTRL_DEG2RAD %g\n",
                //         -V_max*CTRL_DEG2RAD,    q_dot_0[i]-boundSmoothness*CTRL_DEG2RAD);
            }
            else{
                // The joints velocities will be constrained by the v_lim constraints (after possible external modification by avoidanceHandlers) 
                x_l[i]=v_bounds(i,0); //lower bound
                x_u[i]=v_bounds(i,1); //upper bound
                     
                if (n==10 && i<3) //special handling of torso joints - should be moving less
                {
                    x_l[i]=v_bounds(i,0)/torsoReduction;
                    x_u[i]=v_bounds(i,1)/torsoReduction;
                }
            }
        }
        
        
        printMessage(3,"[get_bounds_info (deg)]   x_l: %s\n", IPOPT_Number_toString(x_l,CTRL_RAD2DEG).c_str());
        printMessage(3,"[get_bounds_info (deg)]   x_u: %s\n", IPOPT_Number_toString(x_u,CTRL_RAD2DEG).c_str());
        
        
        
        if (APPLY_INEQ_CONSTRAINTS){
            double lowerBoundInf=-std::numeric_limits<double>::max();
            double upperBoundInf=std::numeric_limits<double>::max();
            //Limits of shoulder assembly in real robot - taken from iKinIpOpt.cpp, iCubShoulderConstr::update(void*)
            //1st ineq constraint -347deg/1.71 < q_0 - q_1
            //2nd ineq constr., -366.57deg /1.71 < q_0 - q_1 - q_2 < 112.42deg / 1.71 
            //-66.6 deg < q_1 + q_2 < 213.3 deg
            g_l[0]= (-347/1.71)*CTRL_DEG2RAD + EXTRA_MARGIN_SHOULDER_INEQ_RAD;
            g_u[0]= 4.0*M_PI - EXTRA_MARGIN_SHOULDER_INEQ_RAD; //the difference of two joint angles should never exceed 2 * 360deg 
            g_l[1]= (-366.57/1.71)*CTRL_DEG2RAD + EXTRA_MARGIN_SHOULDER_INEQ_RAD;
            g_u[1]= (112.42 / 1.71) * CTRL_DEG2RAD - EXTRA_MARGIN_SHOULDER_INEQ_RAD;
            g_l[2]= -66.6*CTRL_DEG2RAD + EXTRA_MARGIN_SHOULDER_INEQ_RAD;
            g_u[2]= 213.3*CTRL_DEG2RAD - EXTRA_MARGIN_SHOULDER_INEQ_RAD;
            //  constraints to prevent arm from touching torso
            g_l[3]= shou_n + EXTRA_MARGIN_GENERAL_INEQ_RAD;
            g_u[3] = upperBoundInf - EXTRA_MARGIN_GENERAL_INEQ_RAD;
        // constraints to prevent the forearm from hitting the arm
            g_l[4]= lowerBoundInf + EXTRA_MARGIN_GENERAL_INEQ_RAD;
            g_u[4] = elb_n - EXTRA_MARGIN_GENERAL_INEQ_RAD;
            g_l[5] = -elb_n + EXTRA_MARGIN_GENERAL_INEQ_RAD;
            g_u[5] = upperBoundInf - EXTRA_MARGIN_GENERAL_INEQ_RAD;
            
            printMessage(4,"[get_bounds_info (deg)]   g_l: %s\n", IPOPT_Number_toString(g_l,CTRL_RAD2DEG).c_str());
            printMessage(4,"[get_bounds_info (deg)]   g_u: %s\n", IPOPT_Number_toString(g_u,CTRL_RAD2DEG).c_str());
        }
       
        return true;
    }
    
    /*******Return the value of the objective function at the point x **************************************************/
    bool eval_f(Index n, const Number* x, bool new_x, Number& obj_value)
    {
        computeQuantities(x); //computes the cost function (global variable)

        obj_value=norm2(cost_func);
        printMessage(7,"[eval_f] OK\t\tcost_func: %s\tobj_value %g\n",cost_func.toString().c_str(),obj_value);

        return true;
    }
    
    /************** Return the gradient of the objective function at the point x*********************************************************/
    bool eval_grad_f(Index n, const Number* x, bool new_x, Number* grad_f)
    {
        computeQuantities(x);

        for (Index i=0; i<n; i++)
            grad_f[i]=grad_cost_func[i];
            
        printMessage(7,"[eval_grad_f] OK\n");
        return true;
    }
    
    /********** g will take care of cable constraints in shoulder assembly (g0-g2), arm to torso (g3) and upper arm to arm (g5,g6)************/
    bool eval_g(Index n, const Number* x, bool new_x, Index m, Number* g)
    {
        if(APPLY_INEQ_CONSTRAINTS){
            if(n==10){ //we have 3 torso joints and 7 arm joints
                g[0] =  chain(3).getAng()+kps[3]*dT*x[3] - (chain(4).getAng()+kps[4]*dT*x[4]);   //1st ineq constraint -347deg/1.71 < q_0 - q_1
                //2nd ineq constr., -366.57deg /1.71 < q_0 - q_1 - q_2 < 112.42deg / 1.71 
                g[1] = chain(3).getAng()+kps[3]*dT*x[3] - (chain(4).getAng()+kps[4]*dT*x[4]) - (chain(5).getAng()+kps[5]*dT*x[5]);
                g[2] = chain(4).getAng()+kps[4]*dT*x[4] + (chain(5).getAng()+kps[5]*dT*x[5]); //-66.6 deg < q_1 + q_2 < 213.3 deg
                g[3] = chain(4).getAng()+kps[4]*dT*x[4] - shou_m*(chain(5).getAng()+kps[5]*dT*x[5]); // shou_n < q1 - shou_m * q2
                g[4] = -elb_m*(chain(6).getAng()+kps[6]*dT*x[6]) + chain(7).getAng()+kps[7]*dT*x[7];
                g[5] = elb_m*(chain(6).getAng()+kps[6]*dT*x[6]) + chain(7).getAng()+kps[7]*dT*x[7];
                return true;
            
            }
            else if (n==7){ //only arm joints
                g[0] =  chain(0).getAng()+kps[0]*dT*x[0] - (chain(1).getAng()+kps[1]*dT*x[1]);   //1st ineq constraint -347deg/1.71 < q_0 - q_1
                g[1] = chain(0).getAng()+kps[0]*dT*x[0] - (chain(1).getAng()+kps[1]*dT*x[1]) - (chain(2).getAng()+kps[2]*dT*x[2]);
                g[2] = chain(1).getAng()+kps[1]*dT*x[1] + (chain(2).getAng()+kps[2]*dT*x[2]); //-66.6 deg < q_1 + q_2 < 213.3 deg
                g[3] = chain(1).getAng()+kps[1]*dT*x[1] - shou_m*(chain(2).getAng()+kps[2]*dT*x[2]); // shou_n < q1 - shou_m * q2
                g[4] = -elb_m*(chain(3).getAng()+kps[3]*dT*x[3]) + chain(4).getAng()+kps[4]*dT*x[4];
                g[5] = elb_m*(chain(3).getAng()+kps[3]*dT*x[3]) + chain(4).getAng()+kps[4]*dT*x[4];
                return true;
            }
        }
        return false;
        
    }
    
    /************************************************************************/
    bool eval_jac_g(Index n, const Number* x, bool new_x, Index m, Index nele_jac,
                    Index* iRow, Index *jCol, Number* values)
    {
        if(APPLY_INEQ_CONSTRAINTS){ 
            if (n==10){
                if (values == NULL){ //return the structure of the Jacobian
                    iRow[0] = 0; jCol[0]= 3;
                    iRow[1] = 0; jCol[1]= 4;
                    iRow[2] = 1; jCol[2]= 3;
                    iRow[3] = 1; jCol[3]= 4;
                    iRow[4] = 1; jCol[4]= 5;
                    iRow[5] = 2; jCol[5]= 4;
                    iRow[6] = 2; jCol[6]= 5;
                    iRow[7] = 3; jCol[7]= 4;
                    iRow[8] = 3; jCol[8]= 5;
                    iRow[9] = 4; jCol[9]= 6;
                    iRow[10] = 4; jCol[10]= 7;
                    iRow[11] = 5; jCol[11]= 6;
                    iRow[12] = 5; jCol[12]= 7;
                }
                else{  //return the values of the Jacobian of the constraints
                    values[0]= kps[3]*dT;
                    values[1]= -kps[4]*dT;
                    values[2]= kps[3]*dT;
                    values[3]= -kps[4]*dT;
                    values[4]= -kps[5]*dT;
                    values[5]= kps[4]*dT;
                    values[6]= kps[5]*dT;   
                    values[7]= kps[4]*dT;
                    values[8]= -shou_m*kps[5]*dT;
                    values[9]= -elb_m*kps[6]*dT;
                    values[10]= kps[6]*dT;
                    values[11]= elb_m*kps[7]*dT;
                    values[12]= kps[7]*dT;
                }
                return true;
            
            }
            else if (n==7){
                if (values == NULL){ //return the structure of the Jacobian
                    iRow[0] = 0; jCol[0]= 0;
                    iRow[1] = 0; jCol[1]= 1;
                    iRow[2] = 1; jCol[2]= 0;
                    iRow[3] = 1; jCol[3]= 1;
                    iRow[4] = 1; jCol[4]= 2;
                    iRow[5] = 2; jCol[5]= 1;
                    iRow[6] = 2; jCol[6]= 2;
                    iRow[7] = 3; jCol[7]= 1;
                    iRow[8] = 3; jCol[8]= 2;
                    iRow[9] = 4; jCol[9]= 3;
                    iRow[10] = 4; jCol[10]= 4;
                    iRow[11] = 5; jCol[11]= 3;
                    iRow[12] = 5; jCol[12]= 4;
                }
                else{  //return the values of the Jacobian of the constraints
                    values[0]= kps[0]*dT;
                    values[1]= -kps[1]*dT;
                    values[2]= kps[0]*dT;
                    values[3]= -kps[1]*dT;
                    values[4]= -kps[2]*dT;
                    values[5]= kps[1]*dT;
                    values[6]= kps[2]*dT;   
                    values[7]= kps[1]*dT;
                    values[8]= -shou_m*kps[2]*dT;
                    values[9]= -elb_m*kps[3]*dT;
                    values[10]= kps[4]*dT;
                    values[11]= elb_m*kps[3]*dT;
                    values[12]= kps[4]*dT;
                }
                return true;
            }
        }
        else
            return false;
        
    }
    
    /************************************************************************/
    // bool eval_h(Index n, const Number* x, bool new_x, Number obj_factor,
    //             Index m, const Number* lambda, bool new_lambda,
    //             Index nele_hess, Index* iRow, Index* jCol, Number* values)
    // {
    //     // Empty for now
    //     computeQuantities(x);

    //     return true;
    // }

    /************************************************************************/
    void finalize_solution(SolverReturn status, Index n, const Number* x,
                           const Number* z_L, const Number* z_U, Index m,
                           const Number* g, const Number* lambda, Number obj_value,
                           const IpoptData* ip_data, IpoptCalculatedQuantities* ip_cq)
    {
        // Let's write the solution to the console
        printMessage(4,"[finalize_solution] Solution of the primal variables, x: %s\n",
                                        IPOPT_Number_toString(x,CTRL_RAD2DEG).c_str());
        printMessage(4,"[finalize_solution] Solution of the bound multipliers: z_L and z_U\n");
        printMessage(4,"[finalize_solution (deg))] z_L: %s\n",
                        IPOPT_Number_toString(z_L,CTRL_RAD2DEG).c_str());
        printMessage(4,"[finalize_solution (deg)] z_U: %s\n",
                        IPOPT_Number_toString(z_U,CTRL_RAD2DEG).c_str());
        printMessage(4,"[finalize_solution (deg)] q(t+1): %s\n",
                        IPOPT_Number_toString(g,CTRL_RAD2DEG).c_str());
        printMessage(4,"[finalize_solution] Objective value f(x*) = %e\n", obj_value);
        for (Index i=0; i<n; i++)
            q_dot_d[i]=x[i];
    }

    /************************************************************************/
    virtual ~react_NLP() { }  
};


/************************************************************************/
reactIpOpt::reactIpOpt(const iKinChain &c, const double _tol, const bool _useMemory, const yarp::sig::Vector _kps, const bool _useFilter, iCub::ctrl::Filter *_fil,
                       const unsigned int verbose) :
                       chainCopy(c), useMemory(_useMemory), kps(_kps), useFilter(_useFilter), fil(_fil),verbosity(verbose)
{
    //reactIpOpt makes a copy of the original chain, which may me modified here; then it will be passed as reference to react_NLP in reactIpOpt::solve
    chainCopy.setAllConstraints(false); // this is required since IpOpt initially relaxes constraints
    //note that this is about limits not about joints being blocked (which is preserved)

    App=new IpoptApplication();

    CAST_IPOPTAPP(App)->Options()->SetNumericValue("tol",_tol);
    CAST_IPOPTAPP(App)->Options()->SetNumericValue("acceptable_tol",_tol);
    CAST_IPOPTAPP(App)->Options()->SetIntegerValue("acceptable_iter",10);
    CAST_IPOPTAPP(App)->Options()->SetStringValue("mu_strategy","adaptive");
    CAST_IPOPTAPP(App)->Options()->SetIntegerValue("print_level",verbose);

    // CAST_IPOPTAPP(App)->Options()->SetStringValue("jacobian_approximation","finite-difference-values");
    CAST_IPOPTAPP(App)->Options()->SetStringValue("nlp_scaling_method","gradient-based");
    CAST_IPOPTAPP(App)->Options()->SetStringValue("derivative_test","none");
    // CAST_IPOPTAPP(App)->Options()->SetStringValue("derivative_test","first-order");
    CAST_IPOPTAPP(App)->Options()->SetStringValue("derivative_test_print_all","yes");
    // CAST_IPOPTAPP(App)->Options()->SetStringValue("print_timing_statistics","yes");
    // CAST_IPOPTAPP(App)->Options()->SetStringValue("print_options_documentation","no");
    // CAST_IPOPTAPP(App)->Options()->SetStringValue("skip_finalize_solution_call","yes");

    CAST_IPOPTAPP(App)->Options()->SetIntegerValue("max_iter",std::numeric_limits<int>::max());
    CAST_IPOPTAPP(App)->Options()->SetStringValue("hessian_approximation","limited-memory");

    Ipopt::ApplicationReturnStatus status = CAST_IPOPTAPP(App)->Initialize();
    if (status != Ipopt::Solve_Succeeded)
        yError("Error during initialization!");
}


/************************************************************************/
double reactIpOpt::getTol() const
{
    double tol;
    CAST_IPOPTAPP(App)->Options()->GetNumericValue("tol",tol,"");
    return tol;
}


/************************************************************************/
void reactIpOpt::setVerbosity(const unsigned int verbose)
{
    CAST_IPOPTAPP(App)->Options()->SetIntegerValue("print_level",verbose);
    CAST_IPOPTAPP(App)->Initialize();
}


/************************************************************************/
yarp::sig::Vector reactIpOpt::solve(const yarp::sig::Vector &xd, const yarp::sig::Vector &q, const yarp::sig::Vector &q_dot_0,std::deque<yarp::sig::Vector> &q_dot_memory, double dt, const yarp::sig::Matrix &v_lim, bool boundSmoothnessFlag, double boundSmoothnessValue, int *exit_code)
{
    
    chainCopy.setAng(q); //these differ from the real positions in the case of positionDirect mode
    SmartPtr<react_NLP> nlp=new react_NLP(chainCopy,xd,q_dot_0,q_dot_memory,dt,v_lim,useMemory,kps,useFilter, fil,boundSmoothnessFlag,boundSmoothnessValue, verbosity);
       
    CAST_IPOPTAPP(App)->Options()->SetNumericValue("max_cpu_time",dt);
    ApplicationReturnStatus status=CAST_IPOPTAPP(App)->OptimizeTNLP(GetRawPtr(nlp));

    if (exit_code!=NULL)
        *exit_code=status;

    return nlp->get_q_dot_d();
}

/************************************************************************/
reactIpOpt::~reactIpOpt()
{
    delete CAST_IPOPTAPP(App);
}


