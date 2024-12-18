//
// Created by Jakub Rozlivek on 7/14/21.
//

#include "reactOSQP.h"
#include <yarp/os/LogStream.h>
#include <yarp/math/SVD.h>
#include <fstream>


ArmHelper::ArmHelper(iCubArm *chain_, double dt_, int offset_,  double vmax_, const Vector& restPos,
                     bool hitting_constr_, int vars_offset_, int constr_offset_):
        arm(chain_), offset(offset_), dt(dt_), vmax(vmax_), adapt_w5(0), rest_jnt_pos(restPos),
        hit_constr(hitting_constr_), vars_offset(vars_offset_), constr_offset(constr_offset_)
{
    chain_dof = static_cast<int>(arm->getDOF())-offset;
    v0.resize(chain_dof, 0.0);
    v_lim.resize(chain_dof, 2);
    rest_w = {1, 100, 1, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5};
    J0 = arm->GeoJacobian();
    computeGuard();
}

void ArmHelper::init(const Vector &_xr, const Vector &_v0, const Matrix &_v_lim)
{
    yAssert(7 <= _xr.length());
    yAssert(v0.length() == _v0.length());
    yAssert((v_lim.rows() == _v_lim.rows()) && (v_lim.cols() == _v_lim.cols()));
    for (int r=0; r < _v_lim.rows(); r++)
    {
        yAssert(_v_lim(r, 0) <= _v_lim(r, 1));
    }
    v_lim= CTRL_DEG2RAD * _v_lim;
    v0= CTRL_DEG2RAD * _v0;
    q0=arm->getAng();
    const Matrix H0=arm->getH();
    const Vector p0=H0.getCol(3).subVector(0,2);
    const Vector pr=_xr.subVector(0, 2);
    const Vector ang=_xr.subVector(3,6);
    const Matrix R = axis2dcm(ang).submatrix(0,2,0,2)*H0.submatrix(0,2,0,2).transposed();
    v_des.resize(6,0);
    v_des.setSubvector(0, (pr-p0) / dt);
//    Vector v2 = dcm2rpy(R) / dt;
    Vector axang = dcm2axis(R);
    v_des.setSubvector(3, axang.subVector(0,2) * axang(3) / dt);
    J0=arm->GeoJacobian();
    const double manip_thr = 0.05;
    Matrix U,V;
    Vector S;
    yarp::math::SVD(J0, U, S, V);
    double man = 1.0;
    for (int i = 0; i < S.length(); i++)
    {
        man *= S(i);
    }
    adapt_w5 = (man < manip_thr)? 1 - man/manip_thr : 0;  // not used anymore
    adapt_w5 = adapt_w5 * adapt_w5 + 0.01;
//    printf("adapt_w5 = %g\n", adapt_w5);
    computeBounds();
}

void ArmHelper::computeGuard()
{
    const double guardRatio=0.1;
    qGuardMinExt.resize(chain_dof);
    qGuardMinInt.resize(chain_dof);
    qGuardMaxExt.resize(chain_dof);
    qGuardMaxInt.resize(chain_dof);
    iKinChain &chain=*arm->asChain();

    for (size_t i=0; i < chain_dof; i++)
    {
        const double q_min = chain(i+offset).getMin();
        const double q_max = chain(i+offset).getMax();
        const double qGuard=0.25*guardRatio*(q_max-q_min);

        qGuardMinExt[i]=q_min+qGuard;
        qGuardMinInt[i]=qGuardMinExt[i]+qGuard;
        qGuardMaxExt[i]=q_max-qGuard;
        qGuardMaxInt[i]=qGuardMaxExt[i]-qGuard;
    }
}

void ArmHelper::computeBounds()
{
    bounds.resize(chain_dof, 2);
    for (size_t i=0; i < chain_dof; i++)
    {
        const double qi=q0[i+offset];
        double dmin;
        double dmax;
        if ((qi>=qGuardMinInt[i]) && (qi<=qGuardMaxInt[i]))
        {
            dmin=dmax=1.0;
        }
        else if (qi<qGuardMinInt[i])
        {
            dmin=(qi<=qGuardMinExt[i] ? 0.0 : (qi- qGuardMinExt[i])/(qGuardMinInt[i]-qGuardMinExt[i]));
            dmax=1.0;
        }
        else
        {
            dmin=1.0;
            dmax=(qi>=qGuardMaxExt[i] ? 0.0 : (qi- qGuardMaxExt[i])/(qGuardMaxInt[i]-qGuardMaxExt[i]));
        }
        bounds(i, 0) = dmin*-vmax; //  std::max(dmin*-vmax, v_lim(i+offset, 0));  // apply joint limit bounds only when it is stricter than the avoidance limits
        bounds(i, 1) =  dmax*vmax; // std::min(, v_lim(i+offset, 1));
        if (bounds(i,0) > bounds(i,1))
        {
            bounds(i,0) = bounds(i,1) = 0;
        }
    }
}

void ArmHelper::updateBounds(Eigen::VectorXd& lowerBound, Eigen::VectorXd& upperBound, double pos_error)
{
    for (int i = 0; i < chain_dof; i++)
    {
        lowerBound[i+constr_offset] = bounds(i, 0);
        upperBound[i+constr_offset] = bounds(i, 1);
    }

    for (int i = 0; i < 3; ++i)
    {
        lowerBound[chain_dof+i+constr_offset] = -pos_error; // normal(i) != 0? -10 : 0; //  normal(i)/dt/10 : 0; // normal(i)/dt/10;
        upperBound[chain_dof+i+constr_offset] = pos_error; //normal(i) != 0?  pos_error : 0; //  normal(i)/dt/10 : 0;
    }

    for (int i = 3; i < 6; ++i)
    {
        lowerBound[chain_dof+i+constr_offset] = -std::numeric_limits<double>::max();
        upperBound[chain_dof+i+constr_offset] = std::numeric_limits<double>::max();
    }

    for (int i = 0; i < 6; ++i)
    {
        lowerBound[i+chain_dof+6+constr_offset] = v_des[i];
        upperBound[i+chain_dof+6+constr_offset] = v_des[i];
    }

    // shoulder's cables length
    lowerBound[6+chain_dof+6+constr_offset]=-347.00*CTRL_DEG2RAD-(1.71*(q0[3] - q0[4]));
    upperBound[6+chain_dof+6+constr_offset]=std::numeric_limits<double>::max();
    lowerBound[7+chain_dof+6+constr_offset]=-366.57*CTRL_DEG2RAD-(1.71*(q0[3]-q0[4]-q0[5]));
    upperBound[7+chain_dof+6+constr_offset]=112.42*CTRL_DEG2RAD-(1.71*(q0[3]-q0[4]-q0[5]));
    lowerBound[8+chain_dof+6+constr_offset]=-66.60*CTRL_DEG2RAD-(q0[4]+q0[5]);
    upperBound[8+chain_dof+6+constr_offset]=213.30*CTRL_DEG2RAD-(q0[4]+q0[5]);
    if (hit_constr)
    {
        // avoid hitting torso
        lowerBound[9+chain_dof+6+constr_offset]=-shou_n - (q0[4] + shou_m*q0[5]);
        upperBound[9+chain_dof+6+constr_offset]=std::numeric_limits<double>::max();

        // avoid hitting forearm
        lowerBound[10+chain_dof+6+constr_offset]=-std::numeric_limits<double>::max();
        upperBound[10+chain_dof+6+constr_offset]=elb_n - (-elb_m*q0[6] + q0[7]);
        lowerBound[11+chain_dof+6+constr_offset]=-elb_n - (elb_m*q0[6] + q0[7]);
        upperBound[11+chain_dof+6+constr_offset]=std::numeric_limits<double>::max();
    }
}

void ArmHelper::addConstraints(Eigen::SparseMatrix<double>& linearMatrix, int obs_contr) const
{
    for (int i = 0; i < chain_dof + 6; ++i)
    {
        linearMatrix.insert(i + constr_offset, i + vars_offset) = 1;
    }
    for (int i = 0; i < 6; ++i)
    {
        linearMatrix.insert(i + constr_offset + chain_dof + 6, vars_offset + chain_dof + i) = -1;
    }
    linearMatrix.insert(chain_dof + 12 + constr_offset, 3 + vars_offset- offset) = 1.71 * dt;
    linearMatrix.insert(chain_dof + 12 + constr_offset, 4 + vars_offset- offset) = -1.71 * dt;
    linearMatrix.insert(chain_dof + 12 + 1 + constr_offset, 3 + vars_offset- offset) = 1.71 * dt;
    linearMatrix.insert(chain_dof + 12 + 1 + constr_offset, 4 + vars_offset- offset) = -1.71 * dt;
    linearMatrix.insert(chain_dof + 12 + 1 + constr_offset, 5 + vars_offset- offset) = -1.71 * dt;
    linearMatrix.insert(chain_dof + 12 + 2 + constr_offset, 4 + vars_offset- offset) = dt;
    linearMatrix.insert(chain_dof + 12 + 2 + constr_offset,  5+ vars_offset- offset) = dt;
    if (hit_constr)
    {
        linearMatrix.insert(chain_dof + 12 + 3 + constr_offset, 4 + vars_offset- offset) = dt;
        linearMatrix.insert(chain_dof + 12 + 3 + constr_offset, 5 + vars_offset- offset) = shou_m * dt;
        linearMatrix.insert(chain_dof + 12 + 4 + constr_offset, 6 + vars_offset- offset) = -elb_m * dt;
        linearMatrix.insert(chain_dof + 12 + 4 + constr_offset, 7 + vars_offset- offset) = dt;
        linearMatrix.insert(chain_dof + 12 + 5 + constr_offset, 6 + vars_offset- offset) = elb_m * dt;
        linearMatrix.insert(chain_dof + 12 + 5 + constr_offset, 7 + vars_offset- offset) = dt;
    }

    for (int i = 0; i < 6; ++i)
    {
        linearMatrix.insert(i + constr_offset + chain_dof + 6, 0) = J0(i, 0);
        linearMatrix.insert(i + constr_offset + chain_dof + 6, 1) = J0(i, 1);
        linearMatrix.insert(i + constr_offset + chain_dof + 6, 2) = J0(i, 2);
        for (int j = 3; j < chain_dof+offset; ++j)
        {
            linearMatrix.insert(i + constr_offset + chain_dof + 6, j - offset+vars_offset) = J0(i, j);
        }
    }
    for (int i = 0; i < obs_contr; ++i) {
        for (int j = 0; j < 3; j++) {
            linearMatrix.insert(i + chain_dof + 12 + 3 + constr_offset + 3 * hit_constr, j) = 0.0;
        }

        for (int j = 3; j < chain_dof+offset; ++j) {
            linearMatrix.insert(i + chain_dof + 12 + 3 + constr_offset + 3 * hit_constr, j-offset + vars_offset) = 0.0;
        }
    }
}

//public:
/****************************************************************/
QPSolver::QPSolver(iCubArm *chain_, bool hitConstr, iCubArm* second_chain_, double vmax_, bool orientationControl_,
                             double dT_, const Vector& restPos, double restPosWeight_, const std::string& part_) :
        second_arm(nullptr), dt(dT_), w2(restPosWeight_), orig_w2(restPosWeight_), w3(10), w4(0.05), part(part_), obsConstrActive(false) // TODO: w3 = 10, w4 = 0.05 is original // for bubbles is w3 = 0.01 and w4 = 0.5, for one-arm exp w3=1, w4=0.05, for bimanual w3 =0.1 and w=0.5
{
    main_arm = std::make_unique<ArmHelper>(chain_, dt, 0, vmax_*CTRL_DEG2RAD, restPos, hitConstr);
    vars_offset = main_arm->chain_dof + 6; // + 1;
    constr_offset = main_arm->chain_dof + 12 + 3 + hitConstr * 3 + obs_constr_num/2;
    second_arm = second_chain_ ? std::make_unique<ArmHelper>(second_chain_, dt, 3, vmax_*CTRL_DEG2RAD, restPos,
                                                        hitConstr,vars_offset, constr_offset) : nullptr;
    if (!orientationControl_) w4 = 0;
    int vars = vars_offset;
    int constr = constr_offset;
    if (second_arm)
    {
        vars += second_arm->chain_dof + 6;
        constr += 6 + second_arm->chain_dof + 6 + 3 + hitConstr * 3 + obs_constr_num/2 + 6; //six constraint for distance between end effectors and same ori
    }
    hessian.resize(vars, vars);
    set_hessian();
    gradient.resize(vars);
    gradient.setZero();
    lowerBound.resize(constr);
    lowerBound.setZero();
    upperBound.resize(constr);
    upperBound.setZero();
    linearMatrix.resize(constr, vars);
    for (int i = 0; i < obs_constr_num/2; ++i)
    {
        lowerBound[i + main_arm->chain_dof + 12 + 3 + 3 * hitConstr] = -std::numeric_limits<double>::max();
        upperBound[i + main_arm->chain_dof + 12 + 3 + 3 * hitConstr] = std::numeric_limits<double>::max();
    }
    main_arm->addConstraints(linearMatrix, obs_constr_num/2);

    if (second_arm != nullptr) {
        for (int i = 0; i < obs_constr_num / 2; ++i) {
            lowerBound[i + second_arm->chain_dof + 12 + 3 + constr_offset + 3 * hitConstr] = -std::numeric_limits<double>::max();
            upperBound[i + second_arm->chain_dof + 12 + 3 + constr_offset + 3 * hitConstr] = std::numeric_limits<double>::max();
        }
        second_arm->addConstraints(linearMatrix, obs_constr_num/2);
        for (int i = 0; i < 6; i++) { // bimanual task constraints
            lowerBound[obs_constr_num/2 + i + second_arm->chain_dof + 12 + 3 + constr_offset + 3 * hitConstr] = -std::numeric_limits<double>::max();
            upperBound[obs_constr_num/2 + i + second_arm->chain_dof + 12 + 3 + constr_offset + 3 * hitConstr] = std::numeric_limits<double>::max();
            for (int j = 0; j < 3; j++) {
                linearMatrix.insert(obs_constr_num/2 + i + constr_offset + second_arm->chain_dof + 12 + 3 + 3 * second_arm->hit_constr, j) = main_arm->J0(i, j) - second_arm->J0(i, j);
            }
            for (int j = 0; j < 7; j++) {
                linearMatrix.insert(obs_constr_num/2 + i + constr_offset + second_arm->chain_dof + 12 + 3 + 3 * second_arm->hit_constr, j+3) = main_arm->J0(i, j+3);
                linearMatrix.insert(obs_constr_num/2 + i + constr_offset + second_arm->chain_dof + 12 + 3 + 3 * second_arm->hit_constr, j+vars_offset) = -second_arm->J0(i, j+3);
            }
        }

    }

    solver.data()->setNumberOfVariables(vars);
    solver.data()->setNumberOfConstraints(constr); // hiting_constraints*6
    solver.data()->setHessianMatrix(hessian);
    solver.data()->setGradient(gradient);
    solver.data()->setLinearConstraintsMatrix(linearMatrix);
    solver.data()->setLowerBound(lowerBound);
    solver.data()->setUpperBound(upperBound);
    solver.settings()->setMaxIteration(20000);
    solver.settings()->setAbsoluteTolerance(1e-4);
    solver.settings()->setRelativeTolerance(1e-4);
    solver.settings()->setTimeLimit(0.25*dt);
    solver.settings()->setCheckTermination(10);
    solver.settings()->setPolish(true);
    solver.settings()->setRho(0.001);
    solver.settings()->setPrimalInfeasibilityTollerance(1e-4);
    solver.settings()->setDualInfeasibilityTollerance(1e-4);
    solver.settings()->setVerbosity(false);

    solver.initSolver();
}
QPSolver::~QPSolver() = default;

/****************************************************************/
void QPSolver::init(const Vector &_xr, const Vector &_v0, const Matrix &_v_lim, double rest_pos_w,
                    const std::vector<yarp::sig::Vector>& Aobs, const std::vector<double> &bvals,
                    const std::vector<yarp::sig::Vector>& Aobs2, const std::vector<double> &bvals2,
                    const Vector &_xr2, const Vector &_v02, const Matrix &_v2_lim, bool ee_dist_constr_)
{
    eeDistConstr = ee_dist_constr_;
    obsConstrActive = false;
    w2 = (rest_pos_w >= 0) ? rest_pos_w : orig_w2;
    main_arm->init(_xr, _v0, _v_lim);
    if (second_arm) second_arm->init(_xr2, _v02, _v2_lim);
    for (int i = 0; i < obs_constr_num/2; ++i)
    {
        int j = 0;
        for (; j < Aobs[i].size(); ++j)
        {
            linearMatrix.coeffRef(i + main_arm->chain_dof + 12 + 3 + 3*main_arm->hit_constr, j) = Aobs[i][j];
        }
        for (; j < main_arm->chain_dof; ++j)
        {
            linearMatrix.coeffRef(i + main_arm->chain_dof + 12 + 3 + 3*main_arm->hit_constr, j) = 0.0;
        }
        if (bvals[i] < std::numeric_limits<double>::max())
        {
            obsConstrActive = true;
        }
        upperBound[i + main_arm->chain_dof + 12 + 3 + 3 * main_arm->hit_constr] = bvals[i];
    }
    if (second_arm != nullptr)
    {
        for (int i = 0; i < obs_constr_num/2; ++i) {
            int j = 0;
            for (; j < 3; j++) {
                linearMatrix.coeffRef(i + constr_offset + second_arm->chain_dof + 12 + 3 + 3*second_arm->hit_constr, j) = Aobs2[i][j];
            }
            for (; j < Aobs2[i].size(); ++j) {
                linearMatrix.coeffRef(i + constr_offset + second_arm->chain_dof + 12 + 3 + 3*second_arm->hit_constr, j + vars_offset-3) = Aobs2[i][j];
            }
            for (; j < 3+second_arm->chain_dof; ++j) {
                linearMatrix.coeffRef(i + constr_offset + second_arm->chain_dof + 12 + 3 + 3*second_arm->hit_constr, j + vars_offset-3) = 0.0;
            }
            if (bvals2[i] < std::numeric_limits<double>::max())
            {
                obsConstrActive = true;
            }
            upperBound[i + constr_offset + second_arm->chain_dof + 12 + 3 + 3 * second_arm->hit_constr] = bvals2[i];
        }
    }

    update_gradient();
    update_constraints();
}


/****************************************************************/
void QPSolver::update_bounds(double pos_error, bool main_arm_constr)
{
    if (pos_error > 0) solver.clearSolverVariables();
//    main_arm->updateBounds(lowerBound, upperBound, pos_error);
//    if (second_arm) second_arm->updateBounds(lowerBound, upperBound, std::numeric_limits<double>::max());

    main_arm->updateBounds(lowerBound, upperBound, main_arm_constr?pos_error:std::numeric_limits<double>::max());
    if (second_arm) {
        second_arm->updateBounds(lowerBound, upperBound, main_arm_constr ? std::numeric_limits<double>::max() : pos_error);

        const Vector x1 = main_arm->arm->getH().getCol(3).subVector(0, 2);
        const Vector x2 = second_arm->arm->getH().getCol(3).subVector(0, 2);
        const Vector o1 = main_arm->arm->EndEffPose(true).subVector(3,6);
        const Vector o2 = second_arm->arm->EndEffPose(true).subVector(3,6);
        const Vector ref_dist = {0.0, 0.15,0.0, 0.0,0.0,0.0};
        if (eeDistConstr) {
//            printf("Ori main arm %s\tOri second arm %s\n", main_arm->arm->EndEffPose(true).subVector(3,6).toString(3).c_str(), second_arm->arm->EndEffPose(true).subVector(3,6).toString(3).c_str());
            for (int i = 0; i < 3; i++) {
//                printf("%g\t", x2[i] - x1[i]);
                lowerBound[obs_constr_num / 2 + i + second_arm->chain_dof + 12 + 3 + constr_offset + 3 * main_arm->hit_constr] = (-1e-6 + ref_dist[i] + x2[i] - x1[i]) / dt;
                upperBound[obs_constr_num / 2 + i + second_arm->chain_dof + 12 + 3 + constr_offset + 3 * main_arm->hit_constr] = ( 1e-6 + ref_dist[i] + x2[i] - x1[i]) / dt;
            }
            for (int i = 4; i < 6; i++) {
//                printf("%g\t", o2[i]*o2[3] - o1[i]*o1[3]);
                lowerBound[obs_constr_num / 2 + i + second_arm->chain_dof + 12 + 3 + constr_offset + 3 * main_arm->hit_constr] = (-1e-5 + ref_dist[i] + o2[i]*o2[3] - o1[i]*o1[3]) / dt;
                upperBound[obs_constr_num / 2 + i + second_arm->chain_dof + 12 + 3 + constr_offset + 3 * main_arm->hit_constr] = ( 1e-5 + ref_dist[i] + o2[i]*o2[3] - o1[i]*o1[3]) / dt;
            }
//            printf("\n");
        }
//        lowerBound[obs_constr_num / 2 + 1 + second_arm->chain_dof + 12 + 3 + constr_offset + 3 * main_arm->hit_constr] = -std::numeric_limits<double>::max();
//        upperBound[obs_constr_num / 2 + 1 + second_arm->chain_dof + 12 + 3 + constr_offset + 3 * main_arm->hit_constr] = std::numeric_limits<double>::max();
    }
    solver.updateBounds(lowerBound,upperBound);
    update_hessian(pos_error);
}


void QPSolver::update_hessian(double pos_error)
{
//    for (int i = 0; i < 3; ++i)
//    {
//        hessian.coeffRef(chain_dof+i,chain_dof+i) = std::max(40*norm(main_arm->v_des.subVector(0,2)),2.0);
//    }
   // yInfo("Weights are %g %g\n", 10*norm(main_arm->v_des.subVector(0,2)), norm(main_arm->v_des.subVector(3,5)));
//    double scale = (pos_error > 0 && !obsConstrActive)? 20  : ((obsConstrActive)? 0.1 : 1);
    const double scale = (pos_error > 0)? 10  : 1;
//    for (int i = 3; i < 6; ++i)
//    {
//        hessian.coeffRef(main_arm->chain_dof+i,main_arm->chain_dof+i) = 2*w4/scale;
//    }

    if (second_arm != nullptr)
    {
        if (eeDistConstr) {
            for (int i = 0; i < 3; ++i) {
                hessian.coeffRef(second_arm->chain_dof + i + vars_offset, second_arm->chain_dof + i + vars_offset) = 2 * w3 / scale / 4;
            }
        }
        else
        {
            for (int i = 0; i < 3; ++i) {
                hessian.coeffRef(second_arm->chain_dof + i + vars_offset, second_arm->chain_dof + i + vars_offset) = 2 * w4 / scale; // changed for bubbles from 2 * w4 / scale to 2 * w3 / scale / 4
            }
        }
        for (int i = 3; i < 6; ++i)
        {
            hessian.coeffRef(second_arm->chain_dof+i+vars_offset,second_arm->chain_dof+i+vars_offset) = w4/scale;
        }
    }

    solver.updateHessianMatrix(hessian);
}

/****************************************************************/
void QPSolver::update_gradient()
{
    gradient.setZero();

    for (int i=0; i < main_arm->chain_dof; i++)
    {
        gradient[i] = w2 * main_arm->rest_w[i] * dt * 2 * (main_arm->q0[i] - main_arm->rest_jnt_pos[i]); // - w5 * main_arm->adapt_w5 * dt * main_arm->manip[i];
    }
    if (second_arm != nullptr) {
        for (int i=0; i < second_arm->chain_dof; i++)
        {
            gradient[i+vars_offset] = 0;//-2 * w1 * second_arm->v0[i] * min_type + w2 * second_arm->rest_w[i+3]*10 * dt * 2 * (second_arm->q0[i+3] - second_arm->rest_jnt_pos[i+3]) - w5 * second_arm->adapt_w5 * dt * second_arm->manip[i];
        }
    }
    solver.updateGradient(gradient);
}


/****************************************************************/
void QPSolver::set_hessian()
{
    for (int i = 0; i < main_arm->chain_dof; ++i)
    {
        hessian.insert(i, i) = 2 * main_arm->adapt_w5*main_arm->rest_w[i] + 2 * w2 * dt * dt * main_arm->rest_w[i];
    }

    for (int i = 0; i < 3; ++i)
    {
        hessian.insert(main_arm->chain_dof+i,main_arm->chain_dof+i) = 2*w3;
    }

    for (int i = 3; i < 6; ++i)
    {
        hessian.insert(main_arm->chain_dof+i,main_arm->chain_dof+i) = 2*w4;
    }
    if (second_arm != nullptr)
    {
        for (int i = 0; i < second_arm->chain_dof; ++i)
        {
            hessian.insert(i+vars_offset, i+vars_offset) = 2 * second_arm->adapt_w5*main_arm->rest_w[i+3] + 2 * w2 * dt * dt * second_arm->rest_w[i+3];
        }

        for (int i = 0; i < 3; ++i)
        {
            hessian.insert(second_arm->chain_dof+i+vars_offset,second_arm->chain_dof+i+vars_offset) = 2*10*w3;
        }

        for (int i = 3; i < 6; ++i)
        {
            hessian.insert(second_arm->chain_dof+i+vars_offset,second_arm->chain_dof+i+vars_offset) = 2*w4;
        }
    }
}


/****************************************************************/
void QPSolver::update_constraints()
{
    for (int i = 0; i < 6; ++i)
    {
        for (int j = 0; j < main_arm->chain_dof; ++j)
        {
            linearMatrix.coeffRef(i + main_arm->chain_dof + 6, j) = main_arm->J0(i, j);
        }
    }
    if (second_arm != nullptr)
    {
        for (int i = 0; i < 6; ++i)
        {
            linearMatrix.coeffRef(i + constr_offset + second_arm->chain_dof + 6, 0) = second_arm->J0(i, 0);
            linearMatrix.coeffRef(i + constr_offset + second_arm->chain_dof + 6, 1) = second_arm->J0(i, 1);
            linearMatrix.coeffRef(i + constr_offset + second_arm->chain_dof + 6, 2) = second_arm->J0(i, 2);
            for (int j = 0; j < second_arm->chain_dof; ++j)
            {
                linearMatrix.coeffRef(i + constr_offset + second_arm->chain_dof + 6, j+vars_offset) = second_arm->J0(i, j+3);
            }
        }

        for (int i = 0; i < 3; i++) { // bimanual task constraints
            for (int j = 0; j < 3; j++) {
                linearMatrix.coeffRef(obs_constr_num/2 + i + constr_offset + second_arm->chain_dof + 12 + 3 + 3 * second_arm->hit_constr, j) = main_arm->J0(i, j) - second_arm->J0(i, j);
            }
            for (int j = 0; j < 7; j++) {
                linearMatrix.coeffRef(obs_constr_num/2 + i + constr_offset + second_arm->chain_dof + 12 + 3 + 3 * second_arm->hit_constr, j+3) = main_arm->J0(i, j+3);
                linearMatrix.coeffRef(obs_constr_num/2 + i + constr_offset + second_arm->chain_dof + 12 + 3 + 3 * second_arm->hit_constr, j+vars_offset) = -second_arm->J0(i, j+3);
            }
        }
    }
    solver.updateLinearConstraintsMatrix(linearMatrix);
}

Vector QPSolver::get_resultInDegPerSecond(Matrix& bounds)
{
    Eigen::VectorXd sol = solver.getSolution();
    int dim = main_arm->chain_dof;
    if (second_arm) dim += second_arm->chain_dof;
    Vector v(dim,0.0);
    bounds.resize(dim,2);
    bounds.setSubmatrix(main_arm->bounds, 0,0);
    for (int i = 0; i < main_arm->chain_dof; ++i)
    {
        v[i] = std::max(std::min(sol[i], upperBound[i]), lowerBound[i]);
    }
    if (second_arm)
    {
        for (int i = 0; i < second_arm->chain_dof; ++i)
        {
            v[i + main_arm->chain_dof] = std::max(std::min(sol[i + vars_offset], upperBound[i + constr_offset]), lowerBound[i + constr_offset]);
        }
        bounds.setSubmatrix(second_arm->bounds, main_arm->chain_dof, 0);
    }
    return CTRL_RAD2DEG*v;
}

int QPSolver::optimize(double pos_error, bool main_arm_constr)
{
    update_bounds(pos_error, main_arm_constr);
    Eigen::VectorXd primalVar(hessian.rows());
    primalVar.setZero();
    for (int i = 0; i < main_arm->chain_dof; i++)
    {
        primalVar[i] = std::min(std::max(main_arm->bounds(i, 0), main_arm->v0[i]), main_arm->bounds(i, 1));
    }
    if (second_arm != nullptr)
    {
        for (int i = 0; i < second_arm->chain_dof; i++)
        {
            primalVar[i+vars_offset] = std::min(std::max(second_arm->bounds(i, 0), second_arm->v0[i+3]), second_arm->bounds(i, 1));
        }
    }
    solver.setPrimalVariable(primalVar);
    solver.solve();
    return static_cast<int>(solver.workspace()->info->status_val);
}
