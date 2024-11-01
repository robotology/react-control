/* 
 * Copyright: (C) 2015 iCub Facility - Istituto Italiano di Tecnologia
 * Authors: Alessandro Roncone <alessandro.roncone@yale.edu>, Matej Hoffmann <matej.hoffmann@iit.it>
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

/**
\defgroup reactController reactController

A module able to do stuff.

Date first release: 30/10/2015

CopyPolicy: Released under the terms of the GNU GPL v2.0.

\section intro_sec Description
None for now.

\section lib_sec Libraries 
None for now.

\section parameters_sec Parameters

--context    \e path
- Where to find the called resource.

--from       \e from
- The name of the .ini file with the configuration parameters.

--name       \e name
- The name of the module (default reactController).

--robot      \e rob
- The name of the robot (either "icub" or "icubSim"). Default icubSim.

--rate       \e rate
- The period used by the thread. Default 100ms.

--verbosity  \e verb
- Verbosity level (default 0). The higher is the verbosity, the more
  information is printed out.

\author: Alessandro Roncone
*/ 

#include <yarp/os/RFModule.h>

#include "reactCtrlThread.h"
#include "reactController_IDL.h"

using namespace yarp;
using namespace yarp::os;
using namespace yarp::math;


/**
* \ingroup reactController
*
*  
*/
class reactController: public RFModule, public reactController_IDL
{
private:
    reactCtrlThread *rctCtrlThrd;
    particleThread    *prtclThrd;
    RpcServer            rpcSrvr;

    std::string robot;       // Name of the robot
    std::string  name;       // Name of the module
    std::string  main_part;       // Part to use
    std::string  second_part;       // Second part to use (can be None)

    int     verbosity;  // Verbosity level
    int   rctCtrlRate;  // rate of the reactCtrlThread
    int     prtclRate;  // rate of the particleThread

    bool disableTorso;  // flag to know if the torso has to be used or not

    bool gazeControl; //will follow target with gaze
    bool stiffInteraction; //stiff vs. compliant interaction mode

    double  trajSpeed;  // trajectory speed
    double        tol;  // Tolerance of the ipopt task. The solver exits if norm2(x_d-x)<tol.
    double  globalTol;  // global tolerance of the task. The controller exits if norm(x_d-x)<globalTol
    double       vMax;  // max velocity set for the joints
    double restPosWeight; // Weight of the reaching joint rest position task (disabled if 0.0)
    double timeLimit;  // time limit to reach target
    
    std::string referenceGen; // either "uniformParticle" (constant velocity with particleThread) or "minJerk"
    //or "none" (will directly apply the target - used especially in the mode when targets are streamed)  
    
    bool hittingConstraints; //inequality constraints for safety of shoudler assembly and to prevent self-collisions torso-upper arm, upper-arm - forearm  
    bool orientationControl; //if orientation should be controlled as well
    double selfColPoints; // minimum distance between robot body parts (-1 to turn off self-collision avoidance)
    
    bool tactileCollisionPointsOn; //if on, will be reading collision points from /skinEventsAggregator/skin_events_aggreg:o
    bool visualCollisionPointsOn; //if on, will be reading predicted collision points from visuoTactileRF/pps_activations_aggreg:o
    bool proximityCollisionPointsOn; //if on will be reading predicted collision points from proximity sensor

    //setting visualization in iCub simulator; the visualizations in iCubGui constitute an independent pipeline
    // (currently the iCubGui ones are on and cannot be toggled on/off from the outside)
    bool visualizeTargetInSim; // will use the yarp rpc /icubSim/world to visualize the target
    bool visualizeParticleInSim; // will use the yarp rpc /icubSim/world to visualize the particle (trajectory - intermediate targets)
    bool visualizeCollisionPointsInSim; // will visualize the (potential) collision points in iCub simulator 

public:
    reactController()
    {
        rctCtrlThrd=nullptr;
        prtclThrd=nullptr;

        robot =         "icubSim";
        name  = "reactController";
        main_part  = "left_arm";
        second_part = "None";
        verbosity    =     0;
        rctCtrlRate  =    10;    
        prtclRate    =    10;
        disableTorso = false;
        gazeControl = false;
        stiffInteraction = true;
        trajSpeed    =   0.1;
        tol          =  1e-5;
        globalTol    =  1e-2;
        vMax         =  20.0;
        restPosWeight = 0.0;
        timeLimit   = 10;

        referenceGen = "minJerk";
        hittingConstraints = true;
        orientationControl = true;
        selfColPoints = -1;
        
        tactileCollisionPointsOn = true;
        visualCollisionPointsOn = true;
        proximityCollisionPointsOn = true;

        if(robot == "icubSim"){
            visualizeTargetInSim = true;
            visualizeParticleInSim = true;
            visualizeCollisionPointsInSim = true;
        }
        else{
            visualizeTargetInSim = false;
            visualizeParticleInSim = false;
            visualizeCollisionPointsInSim = false;
        }
    }

    bool set_xd(const yarp::sig::Vector& _xd) override
    {
        if (_xd.size()>=3)
        {
            yInfo(" ");
            yInfo("[reactController] received new x_d: %s", _xd.toString(3,3).c_str());
            return rctCtrlThrd->setNewTarget(_xd, false);
        }
        return false;
    }

    bool set_relative_xd(const yarp::sig::Vector& _rel_xd) override
    {
        if (_rel_xd.size()>=3)
        {
            yInfo(" ");
            yInfo("[reactController] received new relative x_d: %s", _rel_xd.toString(3,3).c_str());
            return rctCtrlThrd->setNewRelativeTarget(_rel_xd);
        }
        return false;
    }

    bool set_relative_circular_xd(const double _radius, const double _frequency) override
    {
        yInfo(" ");
        yInfo("[reactController] received new relative circular x_d: radius %f, frequency: %f.",_radius,_frequency);
        if ((_radius>=0.0) && (_radius <= 0.3) && (_frequency >=0.0) && (_frequency<=1.0)  )
        {
            return rctCtrlThrd->setNewCircularTarget(_radius,_frequency);
        }
        yWarning("[reactController] set_relative_circular_xd(): expecting radius <0,0.3>, frequency <0,1>");
        return false;

    }
    
    bool set_streaming_xd() override
    {
        yInfo(" ");
        yInfo("[reactController] will be reading reaching targets from a port.");
        return rctCtrlThrd->setStreamingTarget();   
    }

    bool set_6d(const yarp::sig::Vector& _xd, const yarp::sig::Vector& _od) override
    {
        if (_xd.size()>=3 && _od.size() >=4)
        {
            yInfo(" ");
            yInfo("[reactController] received new x_d: %s\t o_d: %s", _xd.toString(3).c_str(), _od.toString(3).c_str());
            return rctCtrlThrd->setNewTarget(_xd, _od, false);
        }
        yInfo("[reactController] return false\n");
        return false;
    }

    bool set_both_xd(const yarp::sig::Vector& _xd, const yarp::sig::Vector& _xd2) override
    {
        if (_xd.size()>=3 && _xd2.size() >=3)
        {
            yInfo(" ");
            yInfo("[reactController] received new x_d: %s\t x2_d: %s", _xd.toString(3).c_str(), _xd2.toString(3).c_str());
            return rctCtrlThrd->setBothTargets(_xd, _xd2);
        }
        yInfo("[reactController] return false\n");
        return false;
    }

    bool set_p_both_xd(const yarp::sig::Vector& _xd, const yarp::sig::Vector& _xd2, int32_t m_arm_constr) override
    {
        if (_xd.size()>=3 && _xd2.size() >=3)
        {
            yInfo(" ");
            yInfo("[reactController] received new x_d: %s\t x2_d: %s", _xd.toString(3).c_str(), _xd2.toString(3).c_str());
            return rctCtrlThrd->setBothTargets(_xd, _xd2, m_arm_constr);
        }
        yInfo("[reactController] return false\n");
        return false;
    }

    bool set_both_6d(const yarp::sig::Vector& _xd, const yarp::sig::Vector& _od,
                     const yarp::sig::Vector& _xd2,const yarp::sig::Vector& _od2) override
    {
        if (_xd.size()>=3 && _od.size() >=4 && _xd2.size()>=3 && _od2.size() >=4)
        {
            yInfo(" ");
            yInfo("[reactController] received new x_d: %s\t o_d: %s", _xd.toString(3).c_str(), _od.toString(3).c_str());
            yInfo("[reactController] received new x2_d: %s\t o2_d: %s", _xd2.toString(3).c_str(), _od2.toString(3).c_str());
            return rctCtrlThrd->setBothTargets(_xd, _od, _xd2, _od2);
        }
        yInfo("[reactController] return false\n");
        return false;
    }



    bool go_home() override
    {
        yInfo(" ");
        yInfo("[reactController] robot will move to his home configuration.");
        return rctCtrlThrd->goHome();
    }

    bool hold_position() override
    {
        yInfo(" ");
        yInfo("[reactController] robot will hold his current position.");
        return rctCtrlThrd->holdPosition();
    }
    
    bool set_tol(const double _tol) override
    {
        return rctCtrlThrd->setTol(_tol);
    }

    double get_tol() override
    {
        return rctCtrlThrd->getTol();
    }

    bool set_v_max(const double _v_max) override
    {
        return rctCtrlThrd->setVMax(_v_max);
    }

    double get_v_max() override
    {
        return rctCtrlThrd->getVMax();
    }

    bool set_traj_speed(const double _traj_speed) override
    {
        return rctCtrlThrd->setTrajSpeed(_traj_speed);
    }

    int get_verbosity() override
    {
        return rctCtrlThrd->getVerbosity();
    }

    int get_state() override
    {
        return rctCtrlThrd->getState();
    }

    bool set_verbosity(const int32_t _verbosity) override
    {
        yInfo("[reactController] Setting verbosity to %i",_verbosity);
        return rctCtrlThrd->setVerbosity(_verbosity);
    }

    bool setup_new_particle(const yarp::sig::Vector& _x_0_vel) override
    {
        if (referenceGen == "uniformParticle")
        {
            yarp::sig::Vector _x_0 = _x_0_vel.subVector(0,2);
            yarp::sig::Vector _vel = _x_0_vel.subVector(3,5);
            yInfo("[reactController] Setting up new particle.. x_0: %s\tvel: %s\n",
                _x_0.toString(3,3).c_str(), _vel.toString(3,3).c_str());
            return prtclThrd->setupNewParticle(_x_0,_vel);
        }
        yWarning("[reactController] to command the particle, referenceGen needs to be set to uniformParticle");
        return false;
    }

    bool reset_particle(const yarp::sig::Vector& _x_0) override
    {
        if (referenceGen == "uniformParticle")
        {
            if (_x_0.size()<3)
            {
                return false;
            }
            yInfo("[reactController] Resetting particle to %s..",_x_0.toString(3,3).c_str());
            return prtclThrd->resetParticle(_x_0);
        }
        yWarning("[reactController] to command the particle, referenceGen needs to be set to uniformParticle");
        return false;
    }

    bool particle_stop() override
    {
        if (referenceGen == "uniformParticle")
        {
            yInfo("[reactController] Stopping particle..");
            return prtclThrd->stopParticle();
        }
        yWarning("[reactController] to command the particle, referenceGen needs to be set to uniformParticle");
        return false;
    }

    yarp::sig::Vector get_particle() override
    {
        if (referenceGen == "uniformParticle")
        {
            yInfo("[reactController] Getting particle..");
            return prtclThrd->getParticle();
        }
        yWarning("[reactController] to command the particle, referenceGen needs to be set to uniformParticle");
        yarp::sig::Vector v(3,0.0);
        return v;

    }

    bool enable_torso() override
    {
        yInfo("[reactController] Enabling torso..");
        return rctCtrlThrd->enableTorso();
    }

    bool disable_torso() override
    {
        yInfo("[reactController] Disabling torso..");
        return rctCtrlThrd->disableTorso();
    }

    bool stop() override
    {
        yInfo("[reactController] Stopping control by going to position mode..");
        return rctCtrlThrd->stopControlAndSwitchToPositionMode();
    }

    bool configure(ResourceFinder &rf) override
    {
        //******************************************************
        //******************* NAME ******************
            if (rf.check("name"))
            {
                name = rf.find("name").asString();
                yInfo("[reactController] Module name set to %s", name.c_str());
            }
            else yInfo("[reactController] Module name set to default, i.e. %s", name.c_str());
            setName(name.c_str());

        //******************* ROBOT ******************
            if (rf.check("robot"))
            {
                robot = rf.find("robot").asString();
                yInfo("[reactController] Robot is: %s", robot.c_str());
            }
            else yInfo("[reactController] Could not find robot option in the config file; using %s as default",robot.c_str());

         //******************* VERBOSE ******************
            if (rf.check("verbosity"))
            {
                verbosity = rf.find("verbosity").asInt32();
                yInfo("[reactController] verbosity set to %i", verbosity);
            }
            else yInfo("[reactController] Could not find verbosity option in the config file; using %i as default",verbosity);

        //****************** rctCtrlRate ******************
            if (rf.check("rctCtrlRate"))
            {
                rctCtrlRate = rf.find("rctCtrlRate").asInt32();
                yInfo("[reactController] rctCTrlThread working at %i ms.",rctCtrlRate);
            }
            else yInfo("[reactController] Could not find rctCtrlRate in the config file; using %i as default",rctCtrlRate);

         //******************* PART ******************
            if (rf.check("part"))
            {
                auto* parts = rf.find("part").asList();
                main_part = parts->get(0).asString();
                second_part = parts->size() > 1 ? parts->get(1).asString() : "None";
                if (main_part == "left" || main_part == "right") main_part += "_arm";
                if (second_part == "left" || second_part == "right") second_part += "_arm";

                if (main_part!="left_arm" && main_part!="right_arm")
                {
                    main_part="left_arm";
                    yWarning("[reactController] main part was not in the admissible values. Using %s as default.",main_part.c_str());
                }
                if (second_part!="left_arm" && second_part!="right_arm")
                {
                    second_part="None";
                    yWarning("[reactController] second part was not in the admissible values. Using %s as default.",second_part.c_str());
                }
                yInfo("[reactController] part to use is: %s", main_part.c_str());
                yInfo("[reactController] second part to use is: %s", second_part.c_str());
            }
            else yInfo("[reactController] Could not find part option in the config file; using %s as default",main_part.c_str());

        //********************** CONFIGS ***********************
            if (rf.check("disableTorso"))
            {
                if(rf.find("disableTorso").asString()=="on")
                {
                    disableTorso = true;
                    yInfo("[reactController] disableTorso flag set to on.");
                }
                else{
                    disableTorso = false;
                    yInfo("[reactController] disableTorso flag set to off.");
                }
            }
            else
            {
                 yInfo("[reactController] Could not find disableTorso flag (on/off) in the config file; using %d as default",disableTorso);
            }
        
        //************** getting collision points either from aggregated skin events or from pps (predictions from vision)
        if (rf.check("tactileCollisionPoints"))
        {
            if(rf.find("tactileCollisionPoints").asString()=="on")
            {
                tactileCollisionPointsOn = true;
                yInfo("[reactController] tactileCollisionPoints flag set to on.");
            }
            else{
                tactileCollisionPointsOn = false;
                yInfo("[reactController] tactileCollisionPoints flag set to off.");
            }
        }
        else
        {
            yInfo("[reactController] Could not find tactileCollisionPoints flag (on/off) in the config file; using %d as default",tactileCollisionPointsOn);
        }
        
        if (rf.check("visualCollisionPoints"))
        {
            if(rf.find("visualCollisionPoints").asString()=="on")
            {
                visualCollisionPointsOn = true;
                yInfo("[reactController] visualCollisionPoints flag set to on.");
            }
            else{
                visualCollisionPointsOn = false;
                yInfo("[reactController] visualCollisionPoints flag set to off.");
            }
        }
        else
        {
            yInfo("[reactController] Could not find visualCollisionPoints flag (on/off) in the config file; using %d as default",visualCollisionPointsOn);
        }

        if (rf.check("proximityCollisionPoints"))
        {
            if(rf.find("proximityCollisionPoints").asString()=="on")
            {
                proximityCollisionPointsOn = true;
                yInfo("[reactController] proximityCollisionPoints flag set to on.");
            }
            else{
                proximityCollisionPointsOn = false;
                yInfo("[reactController] proximityCollisionPoints flag set to off.");
            }
        }
        else
        {
            yInfo("[reactController] Could not find proximityCollisionPoints flag (on/off) in the config file; using %d as default",proximityCollisionPointsOn);
        }
        
        //************************** gazeControl ******************************************************8
        if (rf.check("gazeControl"))
        {
            if(rf.find("gazeControl").asString()=="on")
            {
                gazeControl = true;
                yInfo("[reactController] gazeControl flag set to on.");
            }
            else{
                gazeControl = false;
                yInfo("[reactController] gazeControl flag set to off.");
            }
        }
        else
        {
            yInfo("[reactController] Could not find gazeControl flag (on/off) in the config file; using %d as default",gazeControl);
        }  
         
        //************************** gazeControl ******************************************************8
        if (rf.check("stiff"))
        {
            if(rf.find("stiff").asString()=="on")
            {
                stiffInteraction = true;
                yInfo("[reactController] stiff interaction flag set to on.");
            }
            else{
                stiffInteraction = false;
                yInfo("[reactController] stiff interaction flag set to off.");
            }
        }
        else
        {
            yInfo("[reactController] Could not find stiff flag (on/off) in the config file; using %d as default",stiffInteraction);
        }   
            
          //****************** globalTol ******************
            if (rf.check("globalTol"))
            {
                globalTol = rf.find("globalTol").asFloat64();
                yInfo("[reactController] globalTol to reach target set to %g m.",globalTol);
            }
            else yInfo("[reactController] Could not find globalTol in the config file; using %g as default",globalTol);


            //****************** timeLimit ******************
            if (rf.check("timeLimit"))
            {
                timeLimit = rf.find("timeLimit").asFloat64();
                yInfo("[reactController] timeLimit to reach target set to %g s.",timeLimit);
            }
            else yInfo("[reactController] Could not find timeLimit in the config file; using %g as default",timeLimit);


        //*** generating positions for end-effector - trajectory between current pos and final target
            if (rf.check("referenceGen"))
            {
                referenceGen = rf.find("referenceGen").asString();
                if((referenceGen!="uniformParticle") && (referenceGen!="minJerk") && (referenceGen!="none"))
                {
                    referenceGen="minJerk";
                    yWarning("[reactController] referenceGen was not in the admissible values (uniformParticle / minJerk / none). Using %s as default.",referenceGen.c_str());
                }
                else
                {
                    yInfo("[reactController] referenceGen to use is: %s", referenceGen.c_str());
                }
            }
            else yInfo("[reactController] Could not find referenceGen option in the config file; using %s as default",referenceGen.c_str());
            
             //****************** prtclRate ******************
            if (rf.check("prtclRate"))
            {
                prtclRate = rf.find("prtclRate").asInt32();
                yInfo("[reactController] particleThread period (if referenceGen == uniformParticle)  %i ms.",prtclRate);
            }
            else yInfo("[reactController] Could not find prtclRate in the config file; using %i as default",prtclRate);

        //****************** trajSpeed ******************
            if (rf.check("trajSpeed"))
            {
                trajSpeed = rf.find("trajSpeed").asFloat64();
                yInfo("[reactController] trajSpeed (if referenceGen == uniformParticle) set to %g s.",trajSpeed);
            }
            else yInfo("[reactController] Could not find trajSpeed in the config file; using %g as default",trajSpeed);

                    
          //****************** vMax ******************
            if (rf.check("vMax"))
            {
                vMax = rf.find("vMax").asFloat64();
                yInfo("[reactController] vMax (max joint vel) set to %g [deg/s].",vMax);
            }
            else yInfo("[reactController] Could not find vMax (max joint vel) in the config file; using %g [deg/s] as default",vMax);

            
            //****************** tol ******************
            if (rf.check("tol"))
            {
                tol = rf.find("tol").asFloat64();
                yInfo("[reactController] ipopt: tol set to %g m.",tol);
            }
            else yInfo("[reactController] Could not find tol in the config file; using %g as default",tol);
   
        
            //*********** hitting constraints *************************************************/
            if (rf.check("hittingConstraints"))
            {
                if(rf.find("hittingConstraints").asString()=="on")
                {
                    hittingConstraints = true;
                    yInfo("[reactController] hittingConstraints flag set to on.");
                }
                else
                {
                    hittingConstraints = false;
                    yInfo("[reactController] hittingConstraints flag set to off.");
                }
            }
            else
            {
                yInfo("[reactController] Could not find hittingConstraints flag (on/off) in the config file; using %d as default",hittingConstraints);
            }  
            
            //*********** orientation control *************************************************/
            if (rf.check("orientationControl"))
            {
                if(rf.find("orientationControl").asString()=="on"){

                    orientationControl = true;
                    yInfo("[reactController] orientationControl flag set to on.");
                }
                else
                {
                    orientationControl = false;
                    yInfo("[reactController] orientationControl flag set to off.");
                }
            }
            else
            {
                yInfo("[reactController] Could not find orientationControl flag (on/off) in the config file; using %d as default",orientationControl);
            }

            //****************** restPosWeight ******************
            if (rf.check("restPosWeight"))
            {
                restPosWeight = rf.find("restPosWeight").asFloat64();
                yInfo("[reactController] restPosWeight set to %g.",restPosWeight);
            }
            else yInfo("[reactController] Could not find restPosWeight in the config file; using %g as default",restPosWeight);
            //****************** self-collision points ******************
            if (rf.check("selfColPoints"))
            {
                selfColPoints = rf.find("selfColPoints").asFloat64();
                yInfo("[reactController] selfColPoints distance set to %g.",selfColPoints);
            }
            else yInfo("[reactController] Could not find restPosWeight in the config file; using %g as default",selfColPoints);


            //********************** Visualizations in simulator ***********************
//            if (robot == "icubSim"){
                if (rf.check("visualizeTargetInSim"))
                {
                    if(rf.find("visualizeTargetInSim").asString()=="on")
                    {
                        visualizeTargetInSim = true;
                        yInfo("[reactController] visualizeTargetInSim flag set to on.");
                    }
                    else
                    {
                        visualizeTargetInSim = false;
                        yInfo("[reactController] visualizeTargetInSim flag set to off.");
                    }
                }
                else
                {
                    yInfo("[reactController] Could not find visualizeTargetInSim flag (on/off) in the config file; using %d as default",visualizeTargetInSim);
                }
                
                if (rf.check("visualizeParticleInSim"))
                {
                    if(rf.find("visualizeParticleInSim").asString()=="on")
                    {
                        visualizeParticleInSim = true;
                        yInfo("[reactController] visualizeParticleInSim flag set to on.");
                    }
                    else
                    {
                        visualizeParticleInSim = false;
                        yInfo("[reactController] visualizeParticleInSim flag set to off.");
                    }
                }
                else
                {
                    yInfo("[reactController] Could not find visualizeParticleInSim flag (on/off) in the config file; using %d as default",visualizeParticleInSim);
                }
                if (rf.check("visualizeCollisionPointsInSim"))
                {
                    if(rf.find("visualizeCollisionPointsInSim").asString()=="on")
                    {
                        visualizeCollisionPointsInSim = true;
                        yInfo("[reactController] visualizeCollisionPointsInSim flag set to on.");
                    }
                    else
                    {
                        visualizeCollisionPointsInSim = false;
                        yInfo("[reactController] visualizeCollisionPointsInSim flag set to off.");
                    }
                }
                else
                {
                    yInfo("[reactController] Could not find visualizeCollisionPointsInSim flag (on/off) in the config file; using %d as default",visualizeCollisionPointsInSim);
                }
//            }
//            else{
//                visualizeTargetInSim = false;
//                yInfo("[reactController] visualizeTargetInSim flag set to off.");
//                visualizeParticleInSim = false;
//                yInfo("[reactController] visualizeParticleInSim flag set to off.");
//                visualizeCollisionPointsInSim = false;
//                yInfo("[reactController] visualizeCollisionPointsInSim flag set to off.");
//
//            }

        //************* THREAD ******************************
        
        if(referenceGen == "uniformParticle")
        {
            prtclThrd = new particleThread(prtclRate, name, verbosity);
            if (!prtclThrd->start())
            {
                delete prtclThrd;
                prtclThrd=nullptr;

                yError("[reactController] particleThread wasn't instantiated!!");
                return false;
            }
        }
        else prtclThrd = nullptr;
            
        rctCtrlThrd = new reactCtrlThread(rctCtrlRate, name, robot, main_part, second_part, verbosity,
                                          disableTorso, trajSpeed,
                                          globalTol, vMax, tol, timeLimit, referenceGen,
                                          tactileCollisionPointsOn,visualCollisionPointsOn, proximityCollisionPointsOn,
                                          gazeControl,stiffInteraction,
                                          hittingConstraints, orientationControl,
                                          visualizeTargetInSim, visualizeParticleInSim,
                                          visualizeCollisionPointsInSim, prtclThrd, restPosWeight, selfColPoints);
        if (!rctCtrlThrd->start())
        {
            delete rctCtrlThrd;
            rctCtrlThrd = nullptr;

            if (prtclThrd)
            {
                prtclThrd->stop();
                delete prtclThrd;
                prtclThrd=nullptr;
            }

            yError("[reactController] reactCtrlThread wasn't instantiated!!");
            return false;
        }

        rpcSrvr.open("/"+name+"/rpc:i");
        attach(rpcSrvr);

        return true;
    }

    /************************************************************************/
    bool attach(RpcServer &source) override
    {
        return this->yarp().attachAsServer(source);
    }

    bool close() override
    {
        yInfo("REACT CONTROLLER: Stopping threads..");
        if (rctCtrlThrd)
        {
            yInfo("REACT CONTROLLER: Stopping rctCtrlThrd...");
            rctCtrlThrd->stop();
            delete rctCtrlThrd;
            rctCtrlThrd=nullptr;
        }
        if (prtclThrd)
        {
            yInfo("REACT CONTROLLER: Stopping prtclThrd...");
            prtclThrd->stop();
            delete prtclThrd;
            prtclThrd=nullptr;
        }
        rpcSrvr.close();

        return true;
    }

    double getPeriod() override  { return 1.0; }
    bool updateModule() override { return true; }
};

/**
* Main function.
*/
int main(int argc, char * argv[])
{
    yarp::os::Network yarp;

    ResourceFinder rf;
    rf.setDefaultContext("react-control");
    rf.setDefaultConfigFile("reactController.ini");
    rf.configure(argc,argv);

    if (rf.check("help"))
    {   
        yInfo(" "); 
        yInfo("Options:");
        yInfo(" ");
        yInfo("   --context     path:  where to find the called resource");
        yInfo("   --from        from:  the name of the .ini file.");
        yInfo("   --name        name:  the name of the module (default reactController).");
        yInfo("   --robot       robot: the name of the robot. Default icubSim.");
        yInfo("   --part        part:  the arm to use. Default left_arm.");
        yInfo("   --rate        rate:  the period used by the thread. Default 100ms.");
        yInfo("   --verbosity   int:   verbosity level (default 0).");
        yInfo(" ");
        return 0;
    }
    
    if (!yarp.checkNetwork())
    {
        yError("No Network!!!");
        return -1;
    }

    reactController rctCtrl;
    return rctCtrl.runModule(rf);
}
// empty line to make gcc happy
