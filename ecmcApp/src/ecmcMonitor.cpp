#include "ecmcMonitor.hpp"

#include <stdio.h>

ecmcMonitor::ecmcMonitor(ecmcAxisData *axisData)
{
  initVars();
  data_=axisData;
  errorReset();
}

ecmcMonitor::ecmcMonitor(ecmcAxisData *axisData,bool enableAtTargetMon, bool enableLagMon)
{
  initVars();
  data_=axisData;
  enableAtTargetMon_=enableAtTargetMon;
  enableLagMon_=enableLagMon;
}

void ecmcMonitor::initVars()
{
  enable_=false;
  atTargetTol_=0;
  atTargetTime_=0;
  enableAtTargetMon_=true;
  posLagTol_=0;
  posLagTime_=0;
  enableLagMon_=true;
  atTargetCounter_=0;
  lagMonCounter_=0;
  lagError_=0;
  maxVel_=0;
  enableMaxVelMon_=true;
  maxVelCounterDrive_=0;
  maxVelCounterTraj_=0;
  maxVelDriveILDelay_=0;
  maxVelTrajILDelay_=200; //200 cycles
  maxVelDriveILDelay_=maxVelTrajILDelay_*2; //400 cycles default
  enableHardwareInterlock_=false;
  cntrlOutputHL_=0;
  enableCntrlHLMon_=false;
  enableVelocityDiffMon_=false;
  velocityDiffCounter_=0;
  enableAlarmAtHardlimitBwd_=false;
  enableAlarmAtHardlimitFwd_=false;
  hardBwdOld_=false;
  hardFwdOld_=false;
  velDiffTimeTraj_=100;
  velDiffTimeDrive_=100;
  velDiffMaxDiff_=0;

  switchFilterCounter_=0;
  memset(&limitFwdFilterBuffer_,0,sizeof(limitFwdFilterBuffer_));
  memset(&limitBwdFilterBuffer_,0,sizeof(limitBwdFilterBuffer_));
  memset(&homeFilterBuffer_,0,sizeof(homeFilterBuffer_));
  interlockStatusOld_=ECMC_INTERLOCK_NONE;
}

ecmcMonitor::~ecmcMonitor()
{
  ;
}

void ecmcMonitor::execute()
{

  //Limits
  checkLimits();

  //At target
  checkAtTarget();

  //External interlock (on ethercat I/O)

  if(enableHardwareInterlock_ && !data_->interlocks_.hardwareInterlock && enable_)
  {
    setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_MON_EXTERNAL_HARDWARE_INTERLOCK);
  }

  //Lag error
  checkPositionLag();

  //Max Vel
  checkMaxVelocity();

  //Controller output HL
  checkCntrlMaxOutput();

  //Controller output increase at limit switches monitoring
  checkVelocityDiff();

  data_->refreshInterlocks();

  if(interlockStatusOld_!=data_->interlocks_.interlockStatus){
    printInterlockStatus(data_->interlocks_.interlockStatus);
  }
  interlockStatusOld_=data_->interlocks_.interlockStatus;
}

bool ecmcMonitor::getAtTarget()
{
  return data_->status_.atTarget;
}

bool ecmcMonitor::getHardLimitFwd()
{
  return data_->status_.limitFwd;
}

bool ecmcMonitor::getHardLimitBwd()
{
  return data_->status_.limitBwd;
}

void ecmcMonitor::setAtTargetTol(double tol)
{
  if(atTargetTol_!=tol){
    LOGINFO15("%s/%s:%d: axis[%d].monitor.atTargetTolerance=%lf.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,tol);
  }
  atTargetTol_=tol;
}

double ecmcMonitor::getAtTargetTol()
{
  return atTargetTol_;
}

void ecmcMonitor::setAtTargetTime(int time)
{
  if(atTargetTime_!=time){
    LOGINFO15("%s/%s:%d: axis[%d].monitor.atTargetTime=%d.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,time);
  }
  atTargetTime_=time;
}

int ecmcMonitor::getAtTargetTime()
{
  return atTargetTime_;
}

void ecmcMonitor::setEnableAtTargetMon(bool enable)
{
  if(enableAtTargetMon_!=enable){
    LOGINFO15("%s/%s:%d: axis[%d].monitor.enableAtTargetMon=%d.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,enable);
  }
  enableAtTargetMon_=enable;
}

bool ecmcMonitor::getEnableAtTargetMon()
{
  return enableAtTargetMon_;
}

void ecmcMonitor::setPosLagTol(double tol)
{
  if(posLagTol_!=tol){
    LOGINFO15("%s/%s:%d: axis[%d].monitor.posLagTol=%lf.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,tol);
  }
  posLagTol_=tol;
}

double ecmcMonitor::getPosLagTol()
{
  return posLagTol_;
}

int ecmcMonitor::setVelDiffTimeTraj(int time)
{
  if(velDiffTimeTraj_!=time){
    LOGINFO15("%s/%s:%d: axis[%d].monitor.velDiffTimeTraj=%d.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,time);
  }
  velDiffTimeTraj_=time;
  return 0;
}

int ecmcMonitor::setVelDiffTimeDrive(int time)
{
  if(velDiffTimeDrive_!=time){
    LOGINFO15("%s/%s:%d: axis[%d].monitor.velDiffTimeDrive=%d.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,time);
  }
  velDiffTimeDrive_=time;
  return 0;
}

void ecmcMonitor::setPosLagTime(int time)
{
  if(posLagTime_!=time){
    LOGINFO15("%s/%s:%d: axis[%d].monitor.posLagTime=%d.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,time);
  }
  posLagTime_=time;
}

int ecmcMonitor::getPosLagTime()
{
  return posLagTime_;
}

void ecmcMonitor::setEnableLagMon(bool enable)
{
  if(enableLagMon_!=enable){
    LOGINFO15("%s/%s:%d: axis[%d].monitor.enablePosLagMon=%d.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,enable);
  }
  enableLagMon_=enable;
}

bool ecmcMonitor::getEnableLagMon()
{
  return enableLagMon_;
}

bool ecmcMonitor::getHomeSwitch()
{
  return data_->status_.homeSwitch;
}

void ecmcMonitor::readEntries(){
  uint64_t tempRaw=0;

  //Hard limit BWD
  if (readEcEntryValue(0,&tempRaw)) {
    setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_MON_ENTRY_READ_FAIL);
    return;
  }

  if(data_->status_.limitBwd!=(tempRaw>0)){
    LOGINFO15("%s/%s:%d: axis[%d].monitor.limitBwd=%d.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,tempRaw>0);
  }

  data_->status_.limitBwd=tempRaw>0;

  //Hard limit FWD
  if(readEcEntryValue(1,&tempRaw)){
    setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_MON_ENTRY_READ_FAIL);
    return;
  }

  if(data_->status_.limitFwd!=(tempRaw>0)){
    LOGINFO15("%s/%s:%d: axis[%d].monitor.limitFwd=%d.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,tempRaw>0);
  }

  data_->status_.limitFwd=tempRaw>0;

  //Home
  if(readEcEntryValue(2,&tempRaw)){
    setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_MON_ENTRY_READ_FAIL);
    return;
  }

  if(data_->status_.homeSwitch!=(tempRaw>0)){
    LOGINFO15("%s/%s:%d: axis[%d].monitor.homeSwitch=%d.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,tempRaw>0);
  }

  data_->status_.homeSwitch=tempRaw>0;

  //Refresh filtered switches
  filterSwitches();

  if(enableHardwareInterlock_){
    if(readEcEntryValue(3,&tempRaw)){
      setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_MON_ENTRY_READ_FAIL);
      return;
    }
    if(data_->interlocks_.hardwareInterlock!=(tempRaw>0)){
      LOGINFO15("%s/%s:%d: axis[%d].monitor.hardwareInterlock=%d.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,tempRaw>0);
    }
    data_->interlocks_.hardwareInterlock=tempRaw>0;
  }
}

void ecmcMonitor::setEnable(bool enable)
{
  enable_=enable;
}

bool ecmcMonitor::getEnable()
{
  return enable_;
}

int ecmcMonitor::validate()
{
  int error=validateEntryBit(0);
  if(error){ //Hard limit BWD
    return  setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_MON_ENTRY_HARD_BWD_NULL);
  }

  error=validateEntryBit(1);
  if(error){ //Hard limit FWD
    return  setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_MON_ENTRY_HARD_FWD_NULL);
  }

  error=validateEntryBit(2);
  if(error){ //Home
    return  setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_MON_ENTRY_HOME_NULL);
  }

  if(enableHardwareInterlock_){
    error=validateEntryBit(3);
    if(error){ //External interlock
      return  setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_MON_ENTRY_EXT_INTERLOCK_NULL);
    }
  }

  return 0;
}

int ecmcMonitor::setMaxVel(double vel)
{
  if(maxVel_!=vel){
    LOGINFO15("%s/%s:%d: axis[%d].monitor.maxVel=%lf.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,vel);
  }

  maxVel_=vel;
  return 0;
}

int ecmcMonitor::setEnableMaxVelMon(bool enable)
{
  if(enableMaxVelMon_!=enable){
    LOGINFO15("%s/%s:%d: axis[%d].monitor.enableMaxVelMon=%d.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,enable);
  }
  enableMaxVelMon_=enable;
  return 0;
}

bool ecmcMonitor::getEnableMaxVelMon()
{
  return enableMaxVelMon_;
}

int ecmcMonitor::setMaxVelDriveTime(int time)
{
  if(maxVelDriveILDelay_!=time){
    LOGINFO15("%s/%s:%d: axis[%d].monitor.maxVelDriveTime=%d.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,time);
  }
  maxVelDriveILDelay_=time;
  return 0;
}

int ecmcMonitor::setMaxVelTrajTime(int time)
{
  if(maxVelTrajILDelay_!=time){
    LOGINFO15("%s/%s:%d: axis[%d].monitor.maxVelTrajTime=%d.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,time);
  }
  maxVelTrajILDelay_=time;
  return 0;
}

int ecmcMonitor::reset()
{
  LOGINFO15("%s/%s:%d: axis[%d].monitor.reset.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_);

  data_->status_.atTarget=false;
  atTargetCounter_=0;
  lagMonCounter_=0;
  maxVelCounterDrive_=0;
  maxVelCounterTraj_=0;
  velocityDiffCounter_=0;
  data_->clearInterlocks();
  return 0;
}

void ecmcMonitor::errorReset()
{
  reset();
  ecmcError::errorReset();
}

int ecmcMonitor::setEnableHardwareInterlock(bool enable)
{
   if(enable){
     int error=validateEntryBit(3);
     if(error){
       return setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_MON_ENTRY_HARDWARE_INTERLOCK_NULL);
     }
   }
   if(enableHardwareInterlock_!=enable){
     LOGINFO15("%s/%s:%d: axis[%d].monitor.enableHardwareInterlock=%d.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,enable);
   }
   enableHardwareInterlock_=enable;
   return 0;
}

int ecmcMonitor::setCntrlOutputHL(double outputHL)
{
  if(cntrlOutputHL_!=outputHL){
    LOGINFO15("%s/%s:%d: axis[%d].monitor.cntrlOutputHL=%lf.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,outputHL);
  }
  cntrlOutputHL_=outputHL;
  return 0;
}

int ecmcMonitor::setEnableCntrlHLMon(bool enable)
{
  if(enableCntrlHLMon_!=enable){
    LOGINFO15("%s/%s:%d: axis[%d].monitor.enableCntrlHLMon=%d.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,enable);
  }
  enableCntrlHLMon_=enable;
  return 0;
}

bool ecmcMonitor::getEnableCntrlHLMon()
{
  return enableCntrlHLMon_;
}

int ecmcMonitor::setEnableVelocityDiffMon(bool enable)
{
  if(enableVelocityDiffMon_!=enable){
    LOGINFO15("%s/%s:%d: axis[%d].monitor.enableVelocityDiffMon=%d.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,enable);
  }
  enableVelocityDiffMon_=enable;
  return 0;
}

bool ecmcMonitor::getEnableVelocityDiffMon()
{
  return enableVelocityDiffMon_;
}

int ecmcMonitor::setEnableHardLimitBWDAlarm(bool enable)
{
  if(enableAlarmAtHardlimitBwd_!=enable){
    LOGINFO15("%s/%s:%d: axis[%d].monitor.enableAlarmAtHardlimitBwd=%d.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,enable);
  }
  enableAlarmAtHardlimitBwd_=enable;
  return 0;
}

int ecmcMonitor::setEnableHardLimitFWDAlarm(bool enable)
{
  if(enableAlarmAtHardlimitFwd_!=enable){
    LOGINFO15("%s/%s:%d: axis[%d].monitor.enableAlarmAtHardlimitFwd=%d.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,enable);
  }
  enableAlarmAtHardlimitFwd_=enable;
  return 0;
}

int ecmcMonitor::setEnableSoftLimitBwd(bool enable)
{
  if(data_->command_.enableSoftLimitBwd!=enable){
    LOGINFO15("%s/%s:%d: axis[%d].monitor.enableSoftLimitBwd=%d.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,enable);
  }
  data_->command_.enableSoftLimitBwd=enable;
  return 0;
}

int ecmcMonitor::setEnableSoftLimitFwd(bool enable)
{
  if(data_->command_.enableSoftLimitFwd!=enable){
    LOGINFO15("%s/%s:%d: axis[%d].monitor.enableSoftLimitFwd=%d.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,enable);
  }
  data_->command_.enableSoftLimitFwd=enable;
  return 0;
}

int ecmcMonitor::setSoftLimitBwd(double limit)
{
  if(data_->command_.softLimitBwd!=limit){
    LOGINFO15("%s/%s:%d: axis[%d].monitor.softLimitBwd=%lf.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,limit);
  }

  data_->command_.softLimitBwd=limit;
  return 0;
}

int ecmcMonitor::setSoftLimitFwd(double limit)
{
  if(data_->command_.softLimitFwd!=limit){
    LOGINFO15("%s/%s:%d: axis[%d].monitor.softLimitFwd=%lf.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,limit);
  }
  data_->command_.softLimitFwd=limit;
  return 0;
}

int ecmcMonitor::checkLimits()
{
  hardBwdOld_=data_->status_.limitBwd;
  hardFwdOld_=data_->status_.limitFwd;

  //Both limit switches
  data_->interlocks_.bothLimitsLowInterlock=!data_->status_.limitBwd && !data_->status_.limitFwd;
  if(data_->interlocks_.bothLimitsLowInterlock){
    return setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_MON_BOTH_LIMIT_INTERLOCK);
  }

  //Bwd limit switch
  if(!data_->status_.limitBwd && (data_->status_.currentVelocitySetpoint<0 || data_->status_.currentPositionSetpoint<data_->status_.currentPositionSetpointOld) ){
    data_->interlocks_.bwdLimitInterlock=true;
    if(enableAlarmAtHardlimitBwd_){
      return setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_MON_HARD_LIMIT_BWD_INTERLOCK);
    }
  }
  else {
    data_->interlocks_.bwdLimitInterlock=false;
  }

  //Fwd limit switch
  if(!data_->status_.limitFwd && (data_->status_.currentVelocitySetpoint>0 || data_->status_.currentPositionSetpoint>data_->status_.currentPositionSetpointOld)){
    data_->interlocks_.fwdLimitInterlock=true;
    if(enableAlarmAtHardlimitFwd_){
      return setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_MON_HARD_LIMIT_FWD_INTERLOCK);
    }
  }
  else{
    data_->interlocks_.fwdLimitInterlock=false;
  }

  //Bwd soft limit switch
  bool virtSoftlimitBwd=(data_->status_.currentPositionSetpoint < data_->command_.softLimitBwd)
      && (/*data_->status_.currentVelocitySetpoint<0 ||*/ data_->status_.currentPositionSetpoint < data_->status_.currentPositionSetpointOld);
  if(virtSoftlimitBwd && data_->status_.busy && data_->command_.enableSoftLimitBwd){
    data_->interlocks_.bwdSoftLimitInterlock=true;
    return setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_MON_SOFT_LIMIT_BWD_INTERLOCK);
  }
  else {
    data_->interlocks_.bwdSoftLimitInterlock=false;
  }

  bool virtSoftlimitFwd=(data_->status_.currentPositionSetpoint > data_->command_.softLimitFwd)
      && (/*data_->status_.currentVelocitySetpoint>0||*/ data_->status_.currentPositionSetpoint>data_->status_.currentPositionSetpointOld);
  //Fwd soft limit switch
  if(virtSoftlimitFwd && data_->status_.busy && data_->command_.enableSoftLimitFwd){
    data_->interlocks_.fwdSoftLimitInterlock=true;
    return setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_MON_SOFT_LIMIT_FWD_INTERLOCK);
  }
  else{
    data_->interlocks_.fwdSoftLimitInterlock=false;
  }
  return 0;
}

int ecmcMonitor::checkAtTarget()
{
  bool atTarget=false;
  if(enableAtTargetMon_ && data_->status_.enabled){
    if(std::abs(data_->status_.currentTargetPosition-data_->status_.currentPositionActual)<atTargetTol_){
      if (atTargetCounter_<=atTargetTime_){
        atTargetCounter_++;
      }
      if(atTargetCounter_>atTargetTime_){
	  atTarget=true;
      }
    }
    else{
      atTargetCounter_=0;
    }
  }
  else{
      atTarget=true;
  }

  if(data_->status_.atTarget!=atTarget){
    LOGINFO15("%s/%s:%d: axis[%d].monitor.atTarget=%d.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,atTarget);
  }

  data_->status_.atTarget=atTarget;
  return 0;
}

int ecmcMonitor::checkPositionLag()
{
  bool lagErrorTraj=false;
  bool lagErrorDrive=false;

  if(enableLagMon_ && !(lagErrorDrive)){
    lagError_=std::abs(data_->status_.currentPositionActual-data_->status_.currentPositionSetpoint);

    if(lagError_>posLagTol_){
      if(lagMonCounter_<=posLagTime_*2){
        lagMonCounter_++;
      }
      if(lagMonCounter_>posLagTime_){
        lagErrorTraj=true;
      }
      if(lagMonCounter_>=posLagTime_*2){  //interlock the drive in twice the time..
        lagErrorDrive=true;
      }
    }
    else{
      lagMonCounter_=0;
    }
  }
  data_->interlocks_.lagDriveInterlock=lagErrorDrive;
  data_->interlocks_.lagTrajInterlock=lagErrorTraj;

   if(lagErrorDrive || lagErrorTraj){
     return setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_MON_MAX_POSITION_LAG_EXCEEDED);
   }
  return 0;
}

int ecmcMonitor::checkVelocityDiff()
{

  double currentSetVelocityToDrive= data_->status_.cntrlOutput; //cntrlOutput_/cntrlKff_;
  bool velocityDiffErrorDrive=false;
  bool velocityDiffErrorTraj=false;

  if(!enableVelocityDiffMon_ ){
      velocityDiffCounter_=0;
      velocityDiffErrorDrive=false;
      velocityDiffErrorTraj=false;
      return 0;
  }

  if(std::abs(currentSetVelocityToDrive-data_->status_.currentVelocityActual)>velDiffMaxDiff_){
    velocityDiffCounter_++;
  }
  else{
    velocityDiffCounter_=0;
  }

  if(velocityDiffCounter_>velDiffTimeTraj_){
    velocityDiffErrorTraj=true;
  }

  if(velocityDiffCounter_>velDiffTimeDrive_){
    velocityDiffErrorDrive=true;
  }

  data_->interlocks_.velocityDiffDriveInterlock=velocityDiffErrorDrive;
  data_->interlocks_.velocityDiffTrajInterlock=velocityDiffErrorTraj;

  if(velocityDiffErrorDrive || velocityDiffErrorTraj){
    return setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_MON_VELOCITY_DIFFERENCE_EXCEEDED);
  }

  return 0;
}

int ecmcMonitor::checkMaxVelocity()
{
  if((std::abs(data_->status_.currentVelocityActual)>maxVel_ || std::abs(data_->status_.currentVelocitySetpoint)>maxVel_) && enableMaxVelMon_){
      if(maxVelCounterTraj_ <= maxVelTrajILDelay_){
        maxVelCounterTraj_++;
      }
   }
   else{
     maxVelCounterTraj_=0;
   }

   if(!data_->interlocks_.maxVelocityTrajInterlock){
       data_->interlocks_.maxVelocityTrajInterlock=maxVelCounterTraj_>=maxVelTrajILDelay_;
   }

   if(data_->interlocks_.maxVelocityTrajInterlock &&  maxVelCounterDrive_<= maxVelDriveILDelay_){
     maxVelCounterDrive_++;
   }
   else{
     maxVelCounterDrive_=0;
   }

   data_->interlocks_.maxVelocityDriveInterlock=data_->interlocks_.maxVelocityTrajInterlock && maxVelCounterDrive_>=maxVelDriveILDelay_;

   if(data_->interlocks_.maxVelocityDriveInterlock || data_->interlocks_.maxVelocityTrajInterlock){
     return setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_MON_MAX_VELOCITY_EXCEEDED);
   }
   return 0;
}

int ecmcMonitor::checkCntrlMaxOutput()
{
  if(enableCntrlHLMon_ && std::abs(data_->status_.cntrlOutput)>cntrlOutputHL_){
    data_->interlocks_.cntrlOutputHLDriveInterlock=true;
    data_->interlocks_.cntrlOutputHLTrajInterlock=true;
    return setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_MON_CNTRL_OUTPUT_EXCEED_LIMIT);
  }
  return 0;
}

int ecmcMonitor::getEnableAlarmAtHardLimit()
{
  return enableAlarmAtHardlimitBwd_ || enableAlarmAtHardlimitFwd_;
}

double ecmcMonitor::getSoftLimitBwd()
{
  return data_->command_.softLimitBwd;
}

double ecmcMonitor::getSoftLimitFwd()
{
  return data_->command_.softLimitFwd;
}

bool ecmcMonitor::getEnableSoftLimitBwd()
{
  return data_->command_.enableSoftLimitBwd;
}

bool ecmcMonitor::getEnableSoftLimitFwd()
{
  return data_->command_.enableSoftLimitFwd;
}

bool ecmcMonitor::getAtSoftLimitBwd()
{
  return data_->command_.enableSoftLimitBwd && data_->status_.currentPositionActual<=data_->command_.softLimitBwd;
}

bool ecmcMonitor::getAtSoftLimitFwd()
{
  return data_->command_.enableSoftLimitFwd && data_->status_.currentPositionActual>=data_->command_.softLimitFwd;
}

int ecmcMonitor::setVelDiffMaxDifference(double velo)
{
  if(velDiffMaxDiff_!=std::abs(velo)){
    LOGINFO15("%s/%s:%d: axis[%d].monitor.velDiffMax=%lf.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,std::abs(velo));
  }
  velDiffMaxDiff_=std::abs(velo);
  return 0;
}

int ecmcMonitor::filterSwitches()
{
  //Simple filtering of switches (average of last cycles)
  if(switchFilterCounter_>=ECMC_MON_SWITCHES_FILTER_CYCLES){
    switchFilterCounter_=0;
  }
  limitFwdFilterBuffer_[switchFilterCounter_]=data_->status_.limitFwd;
  limitBwdFilterBuffer_[switchFilterCounter_]=data_->status_.limitBwd;
  homeFilterBuffer_[switchFilterCounter_]=data_->status_.homeSwitch;

  int limFwdSum=0;
  int limBwdSum=0;
  int limHomeSum=0;
  for(int i=0;i< ECMC_MON_SWITCHES_FILTER_CYCLES;i++){
    limFwdSum=limFwdSum+limitFwdFilterBuffer_[i];
    limBwdSum=limBwdSum+limitBwdFilterBuffer_[i];
    limHomeSum=limHomeSum+homeFilterBuffer_[i];
  }
  data_->status_.limitFwdFiltered=limFwdSum > ECMC_MON_SWITCHES_FILTER_CYCLES/2;
  data_->status_.limitBwdFiltered=limBwdSum > ECMC_MON_SWITCHES_FILTER_CYCLES/2;
  data_->status_.homeSwitchFiltered=limHomeSum > ECMC_MON_SWITCHES_FILTER_CYCLES/2;

  switchFilterCounter_++;
  return 0;
}

void ecmcMonitor::printInterlockStatus(interlockTypes ilock)
{
  switch(ilock){
    case ECMC_INTERLOCK_NONE:
	LOGINFO15("%s/%s:%d: axis[%d].monitor.interlockStatus=%s.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,"ECMC_INTERLOCK_NONE");
      break;
    case ECMC_INTERLOCK_SOFT_BWD:
	LOGINFO15("%s/%s:%d: axis[%d].monitor.interlockStatus=%s.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,"ECMC_INTERLOCK_SOFT_BWD");
      break;
    case ECMC_INTERLOCK_SOFT_FWD:
	LOGINFO15("%s/%s:%d: axis[%d].monitor.interlockStatus=%s.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,"ECMC_INTERLOCK_SOFT_FWD");
      break;
    case ECMC_INTERLOCK_HARD_BWD:
	LOGINFO15("%s/%s:%d: axis[%d].monitor.interlockStatus=%s.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,"ECMC_INTERLOCK_HARD_BWD");
      break;
    case ECMC_INTERLOCK_HARD_FWD:
	LOGINFO15("%s/%s:%d: axis[%d].monitor.interlockStatus=%s.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,"ECMC_INTERLOCK_HARD_FWD");
      break;
    case ECMC_INTERLOCK_NO_EXECUTE:
	LOGINFO15("%s/%s:%d: axis[%d].monitor.interlockStatus=%s.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,"ECMC_INTERLOCK_NO_EXECUTE");
      break;
    case ECMC_INTERLOCK_POSITION_LAG:
	LOGINFO15("%s/%s:%d: axis[%d].monitor.interlockStatus=%s.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,"ECMC_INTERLOCK_POSITION_LAG");
      break;
    case ECMC_INTERLOCK_BOTH_LIMITS:
	LOGINFO15("%s/%s:%d: axis[%d].monitor.interlockStatus=%s.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,"ECMC_INTERLOCK_BOTH_LIMITS");
      break;
    case ECMC_INTERLOCK_EXTERNAL:
	LOGINFO15("%s/%s:%d: axis[%d].monitor.interlockStatus=%s.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,"ECMC_INTERLOCK_EXTERNAL");
      break;
    case ECMC_INTERLOCK_TRANSFORM:
	LOGINFO15("%s/%s:%d: axis[%d].monitor.interlockStatus=%s.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,"ECMC_INTERLOCK_TRANSFORM");
      break;
    case ECMC_INTERLOCK_MAX_SPEED:
	LOGINFO15("%s/%s:%d: axis[%d].monitor.interlockStatus=%s.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,"ECMC_INTERLOCK_MAX_SPEED");
      break;
    case ECMC_INTERLOCK_CONT_HIGH_LIMIT:
	LOGINFO15("%s/%s:%d: axis[%d].monitor.interlockStatus=%s.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,"ECMC_INTERLOCK_CONT_HIGH_LIMIT");
      break;
    case ECMC_INTERLOCK_CONT_OUT_INCREASE_AT_LIMIT_SWITCH:
	LOGINFO15("%s/%s:%d: axis[%d].monitor.interlockStatus=%s.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,"ECMC_INTERLOCK_CONT_OUT_INCREASE_AT_LIMIT_SWITCH");
      break;
    case ECMC_INTERLOCK_AXIS_ERROR_STATE:
	LOGINFO15("%s/%s:%d: axis[%d].monitor.interlockStatus=%s.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,"ECMC_INTERLOCK_AXIS_ERROR_STATE");
      break;
    case ECMC_INTERLOCK_UNEXPECTED_LIMIT_SWITCH_BEHAVIOUR:
	LOGINFO15("%s/%s:%d: axis[%d].monitor.interlockStatus=%s.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,"ECMC_INTERLOCK_UNEXPECTED_LIMIT_SWITCH_BEHAVIOUR");
      break;
    case ECMC_INTERLOCK_VELOCITY_DIFF:
	LOGINFO15("%s/%s:%d: axis[%d].monitor.interlockStatus=%s.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,"ECMC_INTERLOCK_VELOCITY_DIFF");
      break;
    case ECMC_INTERLOCK_ETHERCAT_MASTER_NOT_OK:
	LOGINFO15("%s/%s:%d: axis[%d].monitor.interlockStatus=%s.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,"ECMC_INTERLOCK_ETHERCAT_MASTER_NOT_OK");
      break;
    default:
        //Should never get here..
	LOGINFO15("%s/%s:%d: axis[%d].monitor.interlockStatus=%d.\n",__FILE__, __FUNCTION__, __LINE__,data_->axisId_,ilock);
      break;
  }
}
