// This is an automatically-generated file.
// It could get re-generated if the ALLOW_IDL_GENERATION flag is on.

#include <reactController_IDL.h>
#include <yarp/os/idl/WireTypes.h>



class reactController_IDL_set_xd : public yarp::os::Portable {
public:
  yarp::sig::Vector _xd;
  bool _return;
  void init(const yarp::sig::Vector& _xd);
  virtual bool write(yarp::os::ConnectionWriter& connection);
  virtual bool read(yarp::os::ConnectionReader& connection);
};

class reactController_IDL_set_relative_xd : public yarp::os::Portable {
public:
  yarp::sig::Vector _rel_xd;
  bool _return;
  void init(const yarp::sig::Vector& _rel_xd);
  virtual bool write(yarp::os::ConnectionWriter& connection);
  virtual bool read(yarp::os::ConnectionReader& connection);
};

class reactController_IDL_set_tol : public yarp::os::Portable {
public:
  double _tol;
  bool _return;
  void init(const double _tol);
  virtual bool write(yarp::os::ConnectionWriter& connection);
  virtual bool read(yarp::os::ConnectionReader& connection);
};

class reactController_IDL_set_traj_time : public yarp::os::Portable {
public:
  double _traj_time;
  bool _return;
  void init(const double _traj_time);
  virtual bool write(yarp::os::ConnectionWriter& connection);
  virtual bool read(yarp::os::ConnectionReader& connection);
};

class reactController_IDL_set_verbosity : public yarp::os::Portable {
public:
  int32_t _verbosity;
  bool _return;
  void init(const int32_t _verbosity);
  virtual bool write(yarp::os::ConnectionWriter& connection);
  virtual bool read(yarp::os::ConnectionReader& connection);
};

class reactController_IDL_get_verbosity : public yarp::os::Portable {
public:
  int32_t _return;
  void init();
  virtual bool write(yarp::os::ConnectionWriter& connection);
  virtual bool read(yarp::os::ConnectionReader& connection);
};

class reactController_IDL_setup_new_particle : public yarp::os::Portable {
public:
  yarp::sig::Vector _x_0_vel;
  bool _return;
  void init(const yarp::sig::Vector& _x_0_vel);
  virtual bool write(yarp::os::ConnectionWriter& connection);
  virtual bool read(yarp::os::ConnectionReader& connection);
};

class reactController_IDL_get_particle : public yarp::os::Portable {
public:
  yarp::sig::Vector _return;
  void init();
  virtual bool write(yarp::os::ConnectionWriter& connection);
  virtual bool read(yarp::os::ConnectionReader& connection);
};

bool reactController_IDL_set_xd::write(yarp::os::ConnectionWriter& connection) {
  yarp::os::idl::WireWriter writer(connection);
  if (!writer.writeListHeader(3)) return false;
  if (!writer.writeTag("set_xd",1,2)) return false;
  if (!writer.write(_xd)) return false;
  return true;
}

bool reactController_IDL_set_xd::read(yarp::os::ConnectionReader& connection) {
  yarp::os::idl::WireReader reader(connection);
  if (!reader.readListReturn()) return false;
  if (!reader.readBool(_return)) {
    reader.fail();
    return false;
  }
  return true;
}

void reactController_IDL_set_xd::init(const yarp::sig::Vector& _xd) {
  _return = false;
  this->_xd = _xd;
}

bool reactController_IDL_set_relative_xd::write(yarp::os::ConnectionWriter& connection) {
  yarp::os::idl::WireWriter writer(connection);
  if (!writer.writeListHeader(4)) return false;
  if (!writer.writeTag("set_relative_xd",1,3)) return false;
  if (!writer.write(_rel_xd)) return false;
  return true;
}

bool reactController_IDL_set_relative_xd::read(yarp::os::ConnectionReader& connection) {
  yarp::os::idl::WireReader reader(connection);
  if (!reader.readListReturn()) return false;
  if (!reader.readBool(_return)) {
    reader.fail();
    return false;
  }
  return true;
}

void reactController_IDL_set_relative_xd::init(const yarp::sig::Vector& _rel_xd) {
  _return = false;
  this->_rel_xd = _rel_xd;
}

bool reactController_IDL_set_tol::write(yarp::os::ConnectionWriter& connection) {
  yarp::os::idl::WireWriter writer(connection);
  if (!writer.writeListHeader(3)) return false;
  if (!writer.writeTag("set_tol",1,2)) return false;
  if (!writer.writeDouble(_tol)) return false;
  return true;
}

bool reactController_IDL_set_tol::read(yarp::os::ConnectionReader& connection) {
  yarp::os::idl::WireReader reader(connection);
  if (!reader.readListReturn()) return false;
  if (!reader.readBool(_return)) {
    reader.fail();
    return false;
  }
  return true;
}

void reactController_IDL_set_tol::init(const double _tol) {
  _return = false;
  this->_tol = _tol;
}

bool reactController_IDL_set_traj_time::write(yarp::os::ConnectionWriter& connection) {
  yarp::os::idl::WireWriter writer(connection);
  if (!writer.writeListHeader(4)) return false;
  if (!writer.writeTag("set_traj_time",1,3)) return false;
  if (!writer.writeDouble(_traj_time)) return false;
  return true;
}

bool reactController_IDL_set_traj_time::read(yarp::os::ConnectionReader& connection) {
  yarp::os::idl::WireReader reader(connection);
  if (!reader.readListReturn()) return false;
  if (!reader.readBool(_return)) {
    reader.fail();
    return false;
  }
  return true;
}

void reactController_IDL_set_traj_time::init(const double _traj_time) {
  _return = false;
  this->_traj_time = _traj_time;
}

bool reactController_IDL_set_verbosity::write(yarp::os::ConnectionWriter& connection) {
  yarp::os::idl::WireWriter writer(connection);
  if (!writer.writeListHeader(3)) return false;
  if (!writer.writeTag("set_verbosity",1,2)) return false;
  if (!writer.writeI32(_verbosity)) return false;
  return true;
}

bool reactController_IDL_set_verbosity::read(yarp::os::ConnectionReader& connection) {
  yarp::os::idl::WireReader reader(connection);
  if (!reader.readListReturn()) return false;
  if (!reader.readBool(_return)) {
    reader.fail();
    return false;
  }
  return true;
}

void reactController_IDL_set_verbosity::init(const int32_t _verbosity) {
  _return = false;
  this->_verbosity = _verbosity;
}

bool reactController_IDL_get_verbosity::write(yarp::os::ConnectionWriter& connection) {
  yarp::os::idl::WireWriter writer(connection);
  if (!writer.writeListHeader(2)) return false;
  if (!writer.writeTag("get_verbosity",1,2)) return false;
  return true;
}

bool reactController_IDL_get_verbosity::read(yarp::os::ConnectionReader& connection) {
  yarp::os::idl::WireReader reader(connection);
  if (!reader.readListReturn()) return false;
  if (!reader.readI32(_return)) {
    reader.fail();
    return false;
  }
  return true;
}

void reactController_IDL_get_verbosity::init() {
  _return = 0;
}

bool reactController_IDL_setup_new_particle::write(yarp::os::ConnectionWriter& connection) {
  yarp::os::idl::WireWriter writer(connection);
  if (!writer.writeListHeader(4)) return false;
  if (!writer.writeTag("setup_new_particle",1,3)) return false;
  if (!writer.write(_x_0_vel)) return false;
  return true;
}

bool reactController_IDL_setup_new_particle::read(yarp::os::ConnectionReader& connection) {
  yarp::os::idl::WireReader reader(connection);
  if (!reader.readListReturn()) return false;
  if (!reader.readBool(_return)) {
    reader.fail();
    return false;
  }
  return true;
}

void reactController_IDL_setup_new_particle::init(const yarp::sig::Vector& _x_0_vel) {
  _return = false;
  this->_x_0_vel = _x_0_vel;
}

bool reactController_IDL_get_particle::write(yarp::os::ConnectionWriter& connection) {
  yarp::os::idl::WireWriter writer(connection);
  if (!writer.writeListHeader(2)) return false;
  if (!writer.writeTag("get_particle",1,2)) return false;
  return true;
}

bool reactController_IDL_get_particle::read(yarp::os::ConnectionReader& connection) {
  yarp::os::idl::WireReader reader(connection);
  if (!reader.readListReturn()) return false;
  if (!reader.read(_return)) {
    reader.fail();
    return false;
  }
  return true;
}

void reactController_IDL_get_particle::init() {
}

reactController_IDL::reactController_IDL() {
  yarp().setOwner(*this);
}
bool reactController_IDL::set_xd(const yarp::sig::Vector& _xd) {
  bool _return = false;
  reactController_IDL_set_xd helper;
  helper.init(_xd);
  if (!yarp().canWrite()) {
    yError("Missing server method '%s'?","bool reactController_IDL::set_xd(const yarp::sig::Vector& _xd)");
  }
  bool ok = yarp().write(helper,helper);
  return ok?helper._return:_return;
}
bool reactController_IDL::set_relative_xd(const yarp::sig::Vector& _rel_xd) {
  bool _return = false;
  reactController_IDL_set_relative_xd helper;
  helper.init(_rel_xd);
  if (!yarp().canWrite()) {
    yError("Missing server method '%s'?","bool reactController_IDL::set_relative_xd(const yarp::sig::Vector& _rel_xd)");
  }
  bool ok = yarp().write(helper,helper);
  return ok?helper._return:_return;
}
bool reactController_IDL::set_tol(const double _tol) {
  bool _return = false;
  reactController_IDL_set_tol helper;
  helper.init(_tol);
  if (!yarp().canWrite()) {
    yError("Missing server method '%s'?","bool reactController_IDL::set_tol(const double _tol)");
  }
  bool ok = yarp().write(helper,helper);
  return ok?helper._return:_return;
}
bool reactController_IDL::set_traj_time(const double _traj_time) {
  bool _return = false;
  reactController_IDL_set_traj_time helper;
  helper.init(_traj_time);
  if (!yarp().canWrite()) {
    yError("Missing server method '%s'?","bool reactController_IDL::set_traj_time(const double _traj_time)");
  }
  bool ok = yarp().write(helper,helper);
  return ok?helper._return:_return;
}
bool reactController_IDL::set_verbosity(const int32_t _verbosity) {
  bool _return = false;
  reactController_IDL_set_verbosity helper;
  helper.init(_verbosity);
  if (!yarp().canWrite()) {
    yError("Missing server method '%s'?","bool reactController_IDL::set_verbosity(const int32_t _verbosity)");
  }
  bool ok = yarp().write(helper,helper);
  return ok?helper._return:_return;
}
int32_t reactController_IDL::get_verbosity() {
  int32_t _return = 0;
  reactController_IDL_get_verbosity helper;
  helper.init();
  if (!yarp().canWrite()) {
    yError("Missing server method '%s'?","int32_t reactController_IDL::get_verbosity()");
  }
  bool ok = yarp().write(helper,helper);
  return ok?helper._return:_return;
}
bool reactController_IDL::setup_new_particle(const yarp::sig::Vector& _x_0_vel) {
  bool _return = false;
  reactController_IDL_setup_new_particle helper;
  helper.init(_x_0_vel);
  if (!yarp().canWrite()) {
    yError("Missing server method '%s'?","bool reactController_IDL::setup_new_particle(const yarp::sig::Vector& _x_0_vel)");
  }
  bool ok = yarp().write(helper,helper);
  return ok?helper._return:_return;
}
yarp::sig::Vector reactController_IDL::get_particle() {
  yarp::sig::Vector _return;
  reactController_IDL_get_particle helper;
  helper.init();
  if (!yarp().canWrite()) {
    yError("Missing server method '%s'?","yarp::sig::Vector reactController_IDL::get_particle()");
  }
  bool ok = yarp().write(helper,helper);
  return ok?helper._return:_return;
}

bool reactController_IDL::read(yarp::os::ConnectionReader& connection) {
  yarp::os::idl::WireReader reader(connection);
  reader.expectAccept();
  if (!reader.readListHeader()) { reader.fail(); return false; }
  yarp::os::ConstString tag = reader.readTag();
  bool direct = (tag=="__direct__");
  if (direct) tag = reader.readTag();
  while (!reader.isError()) {
    // TODO: use quick lookup, this is just a test
    if (tag == "set_xd") {
      yarp::sig::Vector _xd;
      if (!reader.read(_xd)) {
        reader.fail();
        return false;
      }
      bool _return;
      _return = set_xd(_xd);
      yarp::os::idl::WireWriter writer(reader);
      if (!writer.isNull()) {
        if (!writer.writeListHeader(1)) return false;
        if (!writer.writeBool(_return)) return false;
      }
      reader.accept();
      return true;
    }
    if (tag == "set_relative_xd") {
      yarp::sig::Vector _rel_xd;
      if (!reader.read(_rel_xd)) {
        reader.fail();
        return false;
      }
      bool _return;
      _return = set_relative_xd(_rel_xd);
      yarp::os::idl::WireWriter writer(reader);
      if (!writer.isNull()) {
        if (!writer.writeListHeader(1)) return false;
        if (!writer.writeBool(_return)) return false;
      }
      reader.accept();
      return true;
    }
    if (tag == "set_tol") {
      double _tol;
      if (!reader.readDouble(_tol)) {
        reader.fail();
        return false;
      }
      bool _return;
      _return = set_tol(_tol);
      yarp::os::idl::WireWriter writer(reader);
      if (!writer.isNull()) {
        if (!writer.writeListHeader(1)) return false;
        if (!writer.writeBool(_return)) return false;
      }
      reader.accept();
      return true;
    }
    if (tag == "set_traj_time") {
      double _traj_time;
      if (!reader.readDouble(_traj_time)) {
        reader.fail();
        return false;
      }
      bool _return;
      _return = set_traj_time(_traj_time);
      yarp::os::idl::WireWriter writer(reader);
      if (!writer.isNull()) {
        if (!writer.writeListHeader(1)) return false;
        if (!writer.writeBool(_return)) return false;
      }
      reader.accept();
      return true;
    }
    if (tag == "set_verbosity") {
      int32_t _verbosity;
      if (!reader.readI32(_verbosity)) {
        reader.fail();
        return false;
      }
      bool _return;
      _return = set_verbosity(_verbosity);
      yarp::os::idl::WireWriter writer(reader);
      if (!writer.isNull()) {
        if (!writer.writeListHeader(1)) return false;
        if (!writer.writeBool(_return)) return false;
      }
      reader.accept();
      return true;
    }
    if (tag == "get_verbosity") {
      int32_t _return;
      _return = get_verbosity();
      yarp::os::idl::WireWriter writer(reader);
      if (!writer.isNull()) {
        if (!writer.writeListHeader(1)) return false;
        if (!writer.writeI32(_return)) return false;
      }
      reader.accept();
      return true;
    }
    if (tag == "setup_new_particle") {
      yarp::sig::Vector _x_0_vel;
      if (!reader.read(_x_0_vel)) {
        reader.fail();
        return false;
      }
      bool _return;
      _return = setup_new_particle(_x_0_vel);
      yarp::os::idl::WireWriter writer(reader);
      if (!writer.isNull()) {
        if (!writer.writeListHeader(1)) return false;
        if (!writer.writeBool(_return)) return false;
      }
      reader.accept();
      return true;
    }
    if (tag == "get_particle") {
      yarp::sig::Vector _return;
      _return = get_particle();
      yarp::os::idl::WireWriter writer(reader);
      if (!writer.isNull()) {
        if (!writer.writeListHeader(1)) return false;
        if (!writer.write(_return)) return false;
      }
      reader.accept();
      return true;
    }
    if (tag == "help") {
      std::string functionName;
      if (!reader.readString(functionName)) {
        functionName = "--all";
      }
      std::vector<std::string> _return=help(functionName);
      yarp::os::idl::WireWriter writer(reader);
        if (!writer.isNull()) {
          if (!writer.writeListHeader(2)) return false;
          if (!writer.writeTag("many",1, 0)) return false;
          if (!writer.writeListBegin(BOTTLE_TAG_INT, static_cast<uint32_t>(_return.size()))) return false;
          std::vector<std::string> ::iterator _iterHelp;
          for (_iterHelp = _return.begin(); _iterHelp != _return.end(); ++_iterHelp)
          {
            if (!writer.writeString(*_iterHelp)) return false;
           }
          if (!writer.writeListEnd()) return false;
        }
      reader.accept();
      return true;
    }
    if (reader.noMore()) { reader.fail(); return false; }
    yarp::os::ConstString next_tag = reader.readTag();
    if (next_tag=="") break;
    tag = tag + "_" + next_tag;
  }
  return false;
}

std::vector<std::string> reactController_IDL::help(const std::string& functionName) {
  bool showAll=(functionName=="--all");
  std::vector<std::string> helpString;
  if(showAll) {
    helpString.push_back("*** Available commands:");
    helpString.push_back("set_xd");
    helpString.push_back("set_relative_xd");
    helpString.push_back("set_tol");
    helpString.push_back("set_traj_time");
    helpString.push_back("set_verbosity");
    helpString.push_back("get_verbosity");
    helpString.push_back("setup_new_particle");
    helpString.push_back("get_particle");
    helpString.push_back("help");
  }
  else {
    if (functionName=="set_xd") {
      helpString.push_back("bool set_xd(const yarp::sig::Vector& _xd) ");
      helpString.push_back("Sets a new 3D Cartesian target ");
      helpString.push_back("@param _xd Vector that specifies the new target ");
      helpString.push_back("           (put it between brackets if asking for it through rpc). ");
      helpString.push_back("@return true/false on success/failure. ");
    }
    if (functionName=="set_relative_xd") {
      helpString.push_back("bool set_relative_xd(const yarp::sig::Vector& _rel_xd) ");
      helpString.push_back("Sets a new 3D Cartesian target relative to the current end-effector configuration ");
      helpString.push_back("@param _rel_xd Vector that specifies the new target relative to the current ");
      helpString.push_back("               end-effector configuration -- e.g. (0.0 0.0 0.05) should move ");
      helpString.push_back("               the end effector 5cm up ");
      helpString.push_back("               (put it between brackets if asking for it through rpc). ");
      helpString.push_back("@return true/false on success/failure. ");
    }
    if (functionName=="set_tol") {
      helpString.push_back("bool set_tol(const double _tol) ");
      helpString.push_back("Sets tolerance. ");
      helpString.push_back("@param _tol the solver exits if norm(x_d-x)<tol. ");
      helpString.push_back("@return true/false on success/failure. ");
    }
    if (functionName=="set_traj_time") {
      helpString.push_back("bool set_traj_time(const double _traj_time) ");
      helpString.push_back("Sets Trajectory Time. ");
      helpString.push_back("@param _traj_time  the time within which the solver has to solve the global task ");
      helpString.push_back("@return true/false on success/failure. ");
    }
    if (functionName=="set_verbosity") {
      helpString.push_back("bool set_verbosity(const int32_t _verbosity) ");
      helpString.push_back("Sets verbosity. ");
      helpString.push_back("@param _verbosity  the verbosity of the controller ");
      helpString.push_back("@return true/false on success/failure. ");
    }
    if (functionName=="get_verbosity") {
      helpString.push_back("int32_t get_verbosity() ");
      helpString.push_back("Gets verbosity. ");
      helpString.push_back("@return the verbosity of the controller ");
    }
    if (functionName=="setup_new_particle") {
      helpString.push_back("bool setup_new_particle(const yarp::sig::Vector& _x_0_vel) ");
      helpString.push_back("Setups a new particle with a given initial position and constant velocity ");
    }
    if (functionName=="get_particle") {
      helpString.push_back("yarp::sig::Vector get_particle() ");
    }
    if (functionName=="help") {
      helpString.push_back("std::vector<std::string> help(const std::string& functionName=\"--all\")");
      helpString.push_back("Return list of available commands, or help message for a specific function");
      helpString.push_back("@param functionName name of command for which to get a detailed description. If none or '--all' is provided, print list of available commands");
      helpString.push_back("@return list of strings (one string per line)");
    }
  }
  if ( helpString.empty()) helpString.push_back("Command not found");
  return helpString;
}

