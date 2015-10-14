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

#include <yarp/os/Log.h>
#include <yarp/os/RpcClient.h>
#include <yarp/os/ResourceFinder.h>
#include <yarp/os/RFModule.h>

#include <yarp/math/Math.h>
 
#include <iostream>
#include <string.h> 

#include "reactCtrlThread.h"

using namespace yarp;
using namespace yarp::os;
using namespace yarp::sig;
using namespace yarp::math;

using namespace std;

/**
* \ingroup reactController
*
*  
*/
class reactController: public RFModule
{
private:
    reactCtrlThread *rctCtrlThrd;
    RpcClient            rpcClnt;
    RpcServer            rpcSrvr;

    string robot;
    string name;
    string part;

    int verbosity,rate,record;

    bool autoconnect;

public:
    reactController()
    {
        rctCtrlThrd=0;

        robot     = "icubSim";
        name      = "reactController";
        part      = "left_arm";

        verbosity =    0;    // verbosity
        rate      =  100;    // rate of the reactCtrlThread

        autoconnect = false;
    }

    bool respond(const Bottle &command, Bottle &reply)
    {
        int ack =Vocab::encode("ack");
        int nack=Vocab::encode("nack");

        if (command.size()>0)
        {
            switch (command.get(0).asVocab())
            {
                //-----------------
                case VOCAB4('c','o','n','n'):
                {
                    yarp::os::Network yarpNetwork;
                    if (yarpNetwork.connect("/skinManager/skin_events:o",("/"+name+"/contacts:i").c_str()))
                        reply.addVocab(ack);
                    else
                        reply.addVocab(nack);
                    return true;
                }

                //-----------------
                case VOCAB4('s','t','a','r'):
                {
                    reply.addVocab(nack);
                    return true;
                }

                //-----------------
                case VOCAB4('d','i','s','c'):
                {
                    yarp::os::Network yarpNetwork;
                    if (yarpNetwork.disconnect("/skinManager/skin_events:o",("/"+name+"/contacts:i").c_str()))
                        reply.addVocab(ack);
                    else
                        reply.addVocab(nack);
                    return true;
                }

                //-----------------
                case VOCAB4('s','t','o','p'):
                {
                    if (rctCtrlThrd)
                    {
                        yInfo("REACT CONTROLLER: Stopping threads..");
                        rctCtrlThrd->stop();
                        delete rctCtrlThrd;
                        rctCtrlThrd=0;
                    }
                    reply.addVocab(ack);
                    return true;
                }

                //-----------------
                case VOCAB3('s','e','t'):
                {
                    if (command.get(1).asString() == "xd")
                    {
                        yarp::sig::Vector xd(3,0.0);
                        if (command.size()>=5)
                        {
                            for (int i = 0; i < 3; i++)
                            {
                                xd[i] = command.get(2+i).asDouble();
                            }
                            if (rctCtrlThrd->setNewTarget(xd))
                            {
                                reply.addVocab(ack);
                            }
                            else
                            {
                                reply.addVocab(nack);
                            }
                        }
                        else
                            reply.addVocab(nack);
                    }
                    return true;
                }

                //-----------------
                default:
                    return RFModule::respond(command,reply);
            }
        }

        reply.addVocab(nack);
        return true;
    }

    bool configure(ResourceFinder &rf)
    {
        //******************************************************
        //********************** CONFIGS ***********************
            autoconnect    = rf.check("autoconnect");

            if (autoconnect)
            {
                yInfo("[reactController] Autoconnect flag set to ON");
            }
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

        //******************* PART ******************
            if (rf.check("part"))
            {
                part = rf.find("part").asString();
                if (part=="left")
                {
                    part="left_arm";
                }
                else if (part=="right")
                {
                    part="right_arm";
                }
                else if (part!="left_arm" && part!="right_arm")
                {
                    part="left_arm";
                    yWarning("[reactController] part was not in the admissible values. Using %s as default.",part.c_str());
                }
                yInfo("[reactController] part to use is: %s", part.c_str());
            }
            else yInfo("[reactController] Could not find part option in the config file; using %s as default",part.c_str());

        //******************* VERBOSE ******************
            if (rf.check("verbosity"))
            {
                verbosity = rf.find("verbosity").asInt();
                yInfo("[reactController] verbosity set to %i", verbosity);
            }
            else yInfo("[reactController] Could not find verbosity option in the config file; using %i as default",verbosity);

        //****************** rate ******************
            if (rf.check("rate"))
            {
                rate = rf.find("rate").asInt();
                yInfo("[reactController] rateThread working at %i ms.",rate);
            }
            else yInfo("[reactController] Could not find rate in the config file; using %i as default",rate);

        //************* THREAD *************
        rctCtrlThrd = new reactCtrlThread(rate, name, robot, part, verbosity, autoconnect);
        bool strt = rctCtrlThrd -> start();
        if (!strt)
        {
            delete rctCtrlThrd;
            rctCtrlThrd = 0;
            yError("reactCtrlThread wasn't instantiated!!");
            return false;
        }
        rpcClnt.open(("/"+name+"/rpc:o").c_str());
        rpcSrvr.open(("/"+name+"/rpc:i").c_str());
        attach(rpcSrvr);

        return true;
    }

    bool close()
    {
        yInfo("REACT CONTROLLER: Stopping threads..");
        if (rctCtrlThrd)
        {
            rctCtrlThrd->stop();
            delete rctCtrlThrd;
            rctCtrlThrd=0;
        }

        return true;
    }

    double getPeriod()  { return 1.0; }
    bool updateModule() { return true; }
};

/**
* Main function.
*/
int main(int argc, char * argv[])
{
    yarp::os::Network yarp;

    ResourceFinder rf;
    rf.setVerbose(false);
    rf.setDefaultContext("reactController");
    rf.setDefaultConfigFile("reactController.ini");
    rf.configure(argc,argv);

    if (rf.check("help"))
    {   
        yInfo(""); 
        yInfo("Options:");
        yInfo("");
        yInfo("   --context     path:  where to find the called resource");
        yInfo("   --from        from:  the name of the .ini file.");
        yInfo("   --name        name:  the name of the module (default reactController).");
        yInfo("   --robot       robot: the name of the robot. Default icubSim.");
        yInfo("   --part        part:  the arm to use. Default left_arm.");
        yInfo("   --rate        rate:  the period used by the thread. Default 100ms.");
        yInfo("   --verbosity   int:   verbosity level (default 0).");
        yInfo("");
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