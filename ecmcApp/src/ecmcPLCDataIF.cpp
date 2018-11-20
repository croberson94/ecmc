/*
 *
 *  Created on: Oct 4: 2018
 *      Author: anderssandstrom
 */

#include "ecmcPLCDataIF.h"

ecmcPLCDataIF::ecmcPLCDataIF(ecmcAxisBase *axis,char *axisVarName)
{
  errorReset();
  initVars();
  axis_=axis;
  varName_=axisVarName;
  exprTkVarName_=axisVarName;
  source_=ECMC_RECORDER_SOURCE_AXIS;
  dataSourceAxis_=parseAxisDataSource(axisVarName);
  if(dataSourceAxis_==ECMC_AXIS_DATA_NONE){
    LOGERR("%s/%s:%d: ERROR: Axis data Source Undefined  (0x%x).\n",__FILE__, __FUNCTION__, __LINE__,ERROR_PLC_AXIS_DATA_TYPE_ERROR);
  }
}

ecmcPLCDataIF::ecmcPLCDataIF(ecmcDataStorage *ds,char *dsVarName)
{
  errorReset();
  initVars();
  ds_=ds;
  varName_=dsVarName;
  exprTkVarName_=dsVarName;
  source_=ECMC_RECORDER_SOURCE_DATA_STORAGE;
  dataSourceDs_=parseDataStorageDataSource(dsVarName);
  if(dataSourceDs_==ECMC_DATA_STORAGE_DATA_NONE){
    LOGERR("%s/%s:%d: ERROR: Axis data Source Undefined  (0x%x).\n",__FILE__, __FUNCTION__, __LINE__,ERROR_PLC_AXIS_DATA_TYPE_ERROR);
  }
}

ecmcPLCDataIF::ecmcPLCDataIF(ecmcEc *ec,char *ecVarName)
{
  errorReset();
  initVars();
  ec_=ec;
  varName_=ecVarName;
  exprTkVarName_=ecVarName;
  source_=ECMC_RECORDER_SOURCE_ETHERCAT;
  parseAndLinkEcDataSource(ecVarName);
}

ecmcPLCDataIF::ecmcPLCDataIF(char *varName,ecmcDataSourceType dataSource)
{
  errorReset();
  initVars();
  varName_=varName;
  exprTkVarName_=varName;
  source_=dataSource;
}

ecmcPLCDataIF::~ecmcPLCDataIF()
{

}

void ecmcPLCDataIF::initVars()
{
  axis_=0;
  ds_=0;
  ec_=0;
  data_=0;
  varName_="";
  exprTkVarName_="";
  dataSourceAxis_=ECMC_AXIS_DATA_NONE;
  dataSourceDs_=ECMC_DATA_STORAGE_DATA_NONE;
  source_=ECMC_RECORDER_SOURCE_NONE;
  readOnly_=0;
}

double& ecmcPLCDataIF::getDataRef(){
  return data_;
}

int ecmcPLCDataIF::read()
{
  int errorCode=0;
  switch(source_){
    case ECMC_RECORDER_SOURCE_NONE:
      errorCode=ERROR_PLC_SOURCE_INVALID;
      break;

    case ECMC_RECORDER_SOURCE_ETHERCAT:
      errorCode=readEc();
      break;

    case ECMC_RECORDER_SOURCE_AXIS:
      errorCode=readAxis();
      break;

    case ECMC_RECORDER_SOURCE_STATIC_VAR:
      return 0;
      break;

    case ECMC_RECORDER_SOURCE_GLOBAL_VAR:
      return 0;
      break;

    case ECMC_RECORDER_SOURCE_DATA_STORAGE:
      errorCode=readDs();
      break;  
  }

  dataRead_=data_;
  return setErrorID(__FILE__,__FUNCTION__,__LINE__,errorCode);
}

int ecmcPLCDataIF::write()
{
  //Only write if data changed between read and write
  if(data_==dataRead_ || readOnly_){
    return 0;
  }
  switch(source_){
    case ECMC_RECORDER_SOURCE_NONE:
      return setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_PLC_SOURCE_INVALID);
      break;

    case ECMC_RECORDER_SOURCE_ETHERCAT:
      return writeEc();
      break;

    case ECMC_RECORDER_SOURCE_AXIS:
      return writeAxis();
      break;

    case ECMC_RECORDER_SOURCE_STATIC_VAR:
      return 0;
      break;

   case ECMC_RECORDER_SOURCE_GLOBAL_VAR:
      return 0;
      break;

   case ECMC_RECORDER_SOURCE_DATA_STORAGE:
      return writeDs();
      break;  
  }

  return ERROR_PLC_SOURCE_INVALID;
}

int ecmcPLCDataIF::readEc()
{
  uint64_t value;
  int errorCode=readEcEntryValue(ECMC_PLC_EC_ENTRY_INDEX,&value);
  if(errorCode){
    return setErrorID(__FILE__,__FUNCTION__,__LINE__,errorCode);
  }

  data_=(double)value; //Risk of data loss
  /*printf("READ DATA %s: %lf (%"PRIu64"),\n",varName_.c_str(),data_,value);
  uint64_t temp=value;
  while (temp) {
    if (temp & 1)
        printf("1");
    else
        printf("0");

    temp >>= 1;
  }
  printf("\n");*/
  return 0;
}

int ecmcPLCDataIF::writeEc()
{
  
  uint64_t value=(uint64_t)data_;

  /*printf("WRITE DATA %s: %lf (%"PRIu64"),\n",varName_.c_str(),data_,value);
  uint64_t temp=value;
  while (temp) {
    if (temp & 1)
        printf("1");
    else
        printf("0");

    temp >>= 1;
  }
  printf("\n");*/

  int errorCode=writeEcEntryValue(ECMC_PLC_EC_ENTRY_INDEX,value);
  if(errorCode){
    return setErrorID(__FILE__,__FUNCTION__,__LINE__,errorCode);
  }
  return 0;
}

int ecmcPLCDataIF::readDs()
{
 if(ds_==NULL){
    return ERROR_PLC_DATA_STORAGE_NULL;
  }

  int errorCode=0;
  double tempData=0;

  switch(dataSourceDs_){
    case ECMC_DATA_STORAGE_DATA_NONE:
      data_=0;
      break;

    case ECMC_DATA_STORAGE_DATA_APPEND:
      data_=NAN;
      break; 

    case ECMC_DATA_STORAGE_DATA_SIZE:
      data_=ds_->getSize();
      break; 

    case ECMC_DATA_STORAGE_DATA_INDEX:
      data_=ds_->getCurrentIndex();
      break; 

    case ECMC_DATA_STORAGE_DATA_ERROR:      
      data_=ds_->getErrorID();
      break;     

    case ECMC_DATA_STORAGE_DATA_DATA:
      errorCode=ds_->getDataElement(&tempData);
      if(!errorCode){
        data_=tempData;
      }
      break;

    case ECMC_DATA_STORAGE_DATA_CLEAR:
      data_=NAN;      
      break;

    case ECMC_DATA_STORAGE_DATA_FULL:      
      data_=(double)ds_->isStorageFull();
      break;

    default:
      return setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_PLC_DATA_STORGAE_DATA_TYPE_ERROR);
      break;
  }

  return 0; 
}

int ecmcPLCDataIF::writeDs()
{
 if(ds_==NULL){
    return ERROR_PLC_DATA_STORAGE_NULL;
  }

  switch(dataSourceDs_){
    case ECMC_DATA_STORAGE_DATA_NONE:
      data_=0;
      break;

    case ECMC_DATA_STORAGE_DATA_APPEND:      
      if(data_==data_){ //Check for NAN (only append new value)        
        ds_->appendData(data_);
      }
      break;

    case ECMC_DATA_STORAGE_DATA_SIZE:
      ds_->setBufferSize(data_);
      break;

    case ECMC_DATA_STORAGE_DATA_INDEX:
      ds_->setCurrentPosition(data_);
      break; 

    case ECMC_DATA_STORAGE_DATA_ERROR:      
      ;
      break;

    case ECMC_DATA_STORAGE_DATA_DATA:
      ds_->setDataElement(data_);      
      break; 
         
    case ECMC_DATA_STORAGE_DATA_CLEAR:      
      if(data_==data_ && data_>0){ //Check for NAN (only append new value)
        ds_->clearBuffer();
      }
      break; 

    case ECMC_DATA_STORAGE_DATA_FULL:      
      ;
      break; 
    default:
      return setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_PLC_DATA_STORGAE_DATA_TYPE_ERROR);
      break;
  }
  return 0;
}


int ecmcPLCDataIF::readAxis()
{
  if(axis_==NULL){
    return ERROR_PLC_AXIS_NULL;
  }

  if(axis_->getTraj()==NULL){
    return ERROR_PLC_TRAJ_NULL;
  }

  ecmcAxisStatusType *axisData=axis_->getDebugInfoDataPointer();

  switch(dataSourceAxis_){
    case ECMC_AXIS_DATA_NONE:
      data_=0;
      break;
    case ECMC_AXIS_DATA_AXIS_ID:
      data_=(double)axisData->axisID;
      break;
    case ECMC_AXIS_DATA_POS_SET:
      data_=axisData->onChangeData.positionSetpoint;
      break;
    case ECMC_AXIS_DATA_POS_ACT:
      data_=axisData->onChangeData.positionActual;
      break;
    case ECMC_AXIS_DATA_CNTRL_ERROR:
      data_=axisData->onChangeData.cntrlError;
      break;
    case ECMC_AXIS_DATA_POS_TARGET:
      data_=axisData->onChangeData.positionTarget;
      break;
    case ECMC_AXIS_DATA_POS_ERROR:
      data_=axisData->onChangeData.positionError;
      break;
    case ECMC_AXIS_DATA_POS_RAW:
      data_=(double)axisData->onChangeData.positionRaw;  //Risc of data loss
      break;
    case ECMC_AXIS_DATA_CNTRL_OUT:
      data_=axisData->onChangeData.cntrlOutput;
      break;
    case ECMC_AXIS_DATA_VEL_SET:
      data_=axisData->onChangeData.velocitySetpoint;
      break;
    case ECMC_AXIS_DATA_VEL_ACT:
      data_=axisData->onChangeData.velocityActual;
      break;
    case ECMC_AXIS_DATA_VEL_SET_FF_RAW:
      data_=axisData->onChangeData.velocityFFRaw;
      break;
    case ECMC_AXIS_DATA_VEL_SET_RAW:
      data_=(double)axisData->onChangeData.velocitySetpointRaw;
      break;
    case ECMC_AXIS_DATA_CYCLE_COUNTER:
      data_=(double)axisData->cycleCounter;
      break;
    case ECMC_AXIS_DATA_ERROR:
      data_=(double)axisData->onChangeData.error;
      break;
    case ECMC_AXIS_DATA_COMMAND:
      data_=(double)axisData->onChangeData.command;
      break;
    case ECMC_AXIS_DATA_CMD_DATA:
      data_=(double)axisData->onChangeData.cmdData;
      break;
    case ECMC_AXIS_DATA_SEQ_STATE:
      data_=(double)axisData->onChangeData.seqState;
      break;
    case ECMC_AXIS_DATA_INTERLOCK_TYPE:      
      data_=(double)axisData->onChangeData.trajInterlock==0;
      break;
    case ECMC_AXIS_DATA_TRAJ_SOURCE:
      data_=(double)axisData->onChangeData.trajSource;
      break;
    case ECMC_AXIS_DATA_ENC_SOURCE:
      data_=(double)axisData->onChangeData.encSource;
      break;
    case ECMC_AXIS_DATA_ENABLE:
      data_=(double)axisData->onChangeData.enable;
      break;
    case ECMC_AXIS_DATA_ENABLED:
      data_=(double)axisData->onChangeData.enabled;
      break;
    case ECMC_AXIS_DATA_EXECUTE:
      data_=(double)axisData->onChangeData.execute;
      break;
    case ECMC_AXIS_DATA_BUSY:
      data_=(double)axisData->onChangeData.busy;
      break;
    case ECMC_AXIS_DATA_AT_TARGET:
      data_=(double)axisData->onChangeData.atTarget;
      break;
    case ECMC_AXIS_DATA_HOMED:
      data_=(double)axisData->onChangeData.homed;
      break;
    case ECMC_AXIS_DATA_LIMIT_BWD:
      data_=(double)axisData->onChangeData.limitBwd;
      break;
    case ECMC_AXIS_DATA_LIMIT_FWD:
      data_=(double)axisData->onChangeData.limitFwd;
      break;
    case ECMC_AXIS_DATA_HOME_SWITCH:
      data_=(double)axisData->onChangeData.homeSwitch;
      break;
    case ECMC_AXIS_DATA_RESET:
      data_=0;
      break;
    case ECMC_AXIS_DATA_VEL_TARGET_SET:
      data_=(double )axis_->getSeq()->getTargetVel();
      break;
      break;
    case ECMC_AXIS_DATA_ACC_TARGET_SET:
      data_=(double ) axis_->getTraj()->getAcc();
      break;
    case ECMC_AXIS_DATA_DEC_TARGET_SET:
      data_=(double )axis_->getTraj()->getDec();
      break;
    case ECMC_AXIS_DATA_SOFT_LIMIT_BWD:
      data_=axis_->getMon()->getSoftLimitBwd();
      break;
    case ECMC_AXIS_DATA_SOFT_LIMIT_FWD:
      data_=axis_->getMon()->getSoftLimitFwd();
      break;
    case ECMC_AXIS_DATA_SOFT_LIMIT_BWD_ENABLE:
      data_=(bool)axis_->getMon()->getEnableSoftLimitBwd();
      break;
    case ECMC_AXIS_DATA_SOFT_LIMIT_FWD_ENABLE:
      data_=(bool)axis_->getMon()->getEnableSoftLimitFwd();
      break;
    case ECMC_AXIS_DATA_TRAJ_DIRECTION:      
      switch(axis_->getAxisSetDirection()){
        case ECMC_DIR_BACKWARD:
          data_=-1;
          break;
        case ECMC_DIR_FORWARD:
          data_=1;
          break;
        case ECMC_DIR_STANDSTILL:
          data_=0;
          break;
      }
      break;
    case ECMC_AXIS_DATA_ENC_HOMEPOS:
      data_=axis_->getSeq()->getHomePosition();
      break;
    case ECMC_AXIS_DATA_BLOCK_COM:
      data_=axis_->getBlockExtCom();
      break;
    default:
      return setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_PLC_AXIS_DATA_TYPE_ERROR);
      break;
  }

  return 0;
}

int ecmcPLCDataIF::writeAxis()
{
  if(axis_==NULL){
    return ERROR_PLC_AXIS_NULL;
  }

  if(axis_->getTraj()==NULL){
    return ERROR_PLC_TRAJ_NULL;
  }

  if(axis_->getMon()==NULL){
    return ERROR_PLC_MON_NULL;
  }

  //Only write commands if changed
  switch(dataSourceAxis_){
    case ECMC_AXIS_DATA_NONE:
      return 0;
      break;
    case ECMC_AXIS_DATA_AXIS_ID:
      return 0;
      break;
    case ECMC_AXIS_DATA_POS_SET:
      return 0;
      break;
    case ECMC_AXIS_DATA_POS_ACT:
      return 0;
      break;
    case ECMC_AXIS_DATA_CNTRL_ERROR:
      return 0;
      break;
    case ECMC_AXIS_DATA_POS_TARGET:
      axis_->getSeq()->setTargetPos(data_);
      return 0;
      break;
    case ECMC_AXIS_DATA_POS_ERROR:
      return 0;
      break;
    case ECMC_AXIS_DATA_POS_RAW:
      return 0;
      break;
    case ECMC_AXIS_DATA_CNTRL_OUT:
      return 0;
      break;
    case ECMC_AXIS_DATA_VEL_SET:
      return 0;
      break;
    case ECMC_AXIS_DATA_VEL_ACT:
      return 0;
      break;
    case ECMC_AXIS_DATA_VEL_SET_FF_RAW:
      return 0;
      break;
    case ECMC_AXIS_DATA_VEL_SET_RAW:
      return 0;
      break;
    case ECMC_AXIS_DATA_CYCLE_COUNTER:
      return 0;
      break;
    case ECMC_AXIS_DATA_ERROR:
      return 0;
      break;
    case ECMC_AXIS_DATA_COMMAND:
      return axis_->setCommand((motionCommandTypes)data_);
      break;
    case ECMC_AXIS_DATA_CMD_DATA:
      return axis_->setCmdData((int)data_);
      break;
    case ECMC_AXIS_DATA_SEQ_STATE:
      return 0;
      break;
    case ECMC_AXIS_DATA_INTERLOCK_TYPE:      
      return axis_->getMon()->setPLCInterlock(data_==0);
      break;
    case ECMC_AXIS_DATA_TRAJ_SOURCE:
      return 0;
      break;
    case ECMC_AXIS_DATA_ENC_SOURCE:
      return 0;
      break;
    case ECMC_AXIS_DATA_ENABLE:
      return axis_->setEnable((bool)data_);
      break;
    case ECMC_AXIS_DATA_ENABLED:
      return 0;
      break;
    case ECMC_AXIS_DATA_EXECUTE:
      return axis_->setExecute((bool)data_);
      break;
    case ECMC_AXIS_DATA_BUSY:
      return 0;
      break;
    case ECMC_AXIS_DATA_AT_TARGET:
      return 0;
      break;
    case ECMC_AXIS_DATA_HOMED:
      return 0;
      break;
    case ECMC_AXIS_DATA_LIMIT_BWD:
      return 0;
      break;
    case ECMC_AXIS_DATA_LIMIT_FWD:
      return 0;
      break;
    case ECMC_AXIS_DATA_HOME_SWITCH:
      return 0;
      break;
    case ECMC_AXIS_DATA_RESET:
      if(data_){
        axis_->errorReset();
      }
      return 0;
      break;
    case ECMC_AXIS_DATA_VEL_TARGET_SET:
      axis_->getSeq()->setTargetVel(data_);
      break;
    case ECMC_AXIS_DATA_ACC_TARGET_SET:
      axis_->getTraj()->setAcc(data_);
      break;
    case ECMC_AXIS_DATA_DEC_TARGET_SET:
      axis_->getTraj()->setDec(data_);
      break;
    case ECMC_AXIS_DATA_SOFT_LIMIT_BWD:
      axis_->getMon()->setSoftLimitBwd(data_);
      break;
    case ECMC_AXIS_DATA_SOFT_LIMIT_FWD:
      axis_->getMon()->setSoftLimitFwd(data_);
      break;
    case ECMC_AXIS_DATA_SOFT_LIMIT_BWD_ENABLE:
      axis_->getMon()->setEnableSoftLimitBwd((bool)data_);
      break;
    case ECMC_AXIS_DATA_SOFT_LIMIT_FWD_ENABLE:
      axis_->getMon()->setEnableSoftLimitFwd((bool)data_);
      break;
    case ECMC_AXIS_DATA_TRAJ_DIRECTION:
      return 0;
      break;
    case ECMC_AXIS_DATA_ENC_HOMEPOS:
      axis_->getSeq()->setHomePosition(data_);
      break;
    case ECMC_AXIS_DATA_BLOCK_COM:
      axis_->setBlockExtCom(data_);
      break;

    default:
      return setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_PLC_AXIS_DATA_TYPE_ERROR);
      break;
  }
  return 0;
}

ecmcDataStorageType ecmcPLCDataIF::parseDataStorageDataSource(char * axisDataSource)
{
  char *varName;

  varName=strstr(axisDataSource,ECMC_PLC_DATA_STORAGE_STR);
  if(!varName){
    return ECMC_DATA_STORAGE_DATA_NONE;
  }
  varName=strstr(axisDataSource,".");
  if(!varName){
    return ECMC_DATA_STORAGE_DATA_NONE;
  }
  varName++;

  int npos=strcmp(varName,ECMC_DATA_STORAGE_DATA_APPEND_STR);
  if(npos==0){
    return ECMC_DATA_STORAGE_DATA_APPEND;
  }

  npos=strcmp(varName,ECMC_DATA_STORAGE_DATA_INDEX_STR);
  if(npos==0){
   return ECMC_DATA_STORAGE_DATA_INDEX;
  }

  npos=strcmp(varName,ECMC_DATA_STORAGE_DATA_ERROR_STR);
  if(npos==0){
    return ECMC_DATA_STORAGE_DATA_ERROR;
  }

  npos=strcmp(varName,ECMC_DATA_STORAGE_DATA_SIZE_STR);
  if(npos==0){
    return ECMC_DATA_STORAGE_DATA_SIZE;
  }

  npos=strcmp(varName,ECMC_DATA_STORAGE_DATA_CLEAR_STR);
  if(npos==0){
    return ECMC_DATA_STORAGE_DATA_CLEAR;
  }

  npos=strcmp(varName,ECMC_DATA_STORAGE_DATA_DATA_STR);
  if(npos==0){
    return ECMC_DATA_STORAGE_DATA_DATA;
  }

  npos=strcmp(varName,ECMC_DATA_STORAGE_DATA_FULL_STR);
  if(npos==0){
    return ECMC_DATA_STORAGE_DATA_FULL;
  }

  return ECMC_DATA_STORAGE_DATA_NONE;
}

ecmcAxisDataType ecmcPLCDataIF::parseAxisDataSource(char * axisDataSource)
{
  char *varName;

  varName=strstr(axisDataSource,ECMC_AX_STR);
  if(!varName){
    return ECMC_AXIS_DATA_NONE;
  }
  varName=strstr(axisDataSource,".");
  if(!varName){
    return ECMC_AXIS_DATA_NONE;
  }
  varName++;

  int npos=strcmp(varName,ECMC_AXIS_DATA_STR_AXIS_ID);
  if(npos==0){
    return ECMC_AXIS_DATA_AXIS_ID;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_POS_SET);
  if(npos==0){
   return ECMC_AXIS_DATA_POS_SET;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_POS_ACT);
  if(npos==0){
    return ECMC_AXIS_DATA_POS_ACT;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_POS_TARGET);
  if(npos==0){
    return ECMC_AXIS_DATA_POS_TARGET;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_POS_ERROR);
  if(npos==0){
    return ECMC_AXIS_DATA_POS_ERROR;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_POS_RAW);
  if(npos==0){
    return ECMC_AXIS_DATA_POS_RAW;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_CNTRL_OUT);
  if(npos==0){
    return ECMC_AXIS_DATA_CNTRL_OUT;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_VEL_SET);
  if(npos==0){
    return ECMC_AXIS_DATA_VEL_SET;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_VEL_ACT);
  if(npos==0){
    return ECMC_AXIS_DATA_VEL_ACT;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_VEL_SET_FF_RAW);
  if(npos==0){
    return ECMC_AXIS_DATA_VEL_SET_FF_RAW;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_VEL_SET_RAW);
  if(npos==0){
    return ECMC_AXIS_DATA_VEL_SET_RAW;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_CYCLE_COUNTER);
  if(npos==0){
    return ECMC_AXIS_DATA_CYCLE_COUNTER;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_ERROR);
  if(npos==0){
    return ECMC_AXIS_DATA_ERROR;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_COMMAND);
  if(npos==0){
    return ECMC_AXIS_DATA_COMMAND;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_CMD_DATA);
  if(npos==0){
    return ECMC_AXIS_DATA_CMD_DATA;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_SEQ_STATE);
  if(npos==0){
    return ECMC_AXIS_DATA_SEQ_STATE;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_INTERLOCK_TYPE);
  if(npos==0){
    return ECMC_AXIS_DATA_INTERLOCK_TYPE;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_TRAJ_SOURCE);
  if(npos==0){
    return ECMC_AXIS_DATA_TRAJ_SOURCE;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_ENC_SOURCE);
  if(npos==0){
    return ECMC_AXIS_DATA_ENC_SOURCE;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_ENABLE);
  if(npos==0){
    return ECMC_AXIS_DATA_ENABLE;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_ENABLED);
  if(npos==0){
    return ECMC_AXIS_DATA_ENABLED;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_EXECUTE);
  if(npos==0){
    return ECMC_AXIS_DATA_EXECUTE;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_BUSY);
  if(npos==0){
    return ECMC_AXIS_DATA_BUSY;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_AT_TARGET);
  if(npos==0){
    return ECMC_AXIS_DATA_AT_TARGET ;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_HOMED);
  if(npos==0){
    return ECMC_AXIS_DATA_HOMED  ;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_LIMIT_BWD);
  if(npos==0){
    return ECMC_AXIS_DATA_LIMIT_BWD;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_LIMIT_FWD);
  if(npos==0){
    return ECMC_AXIS_DATA_LIMIT_FWD;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_HOME_SWITCH);
  if(npos==0){
    return ECMC_AXIS_DATA_HOME_SWITCH;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_RESET);
  if(npos==0){
    return ECMC_AXIS_DATA_RESET;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_VEL_TARGET_SET);
  if(npos==0){
    return ECMC_AXIS_DATA_VEL_TARGET_SET;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_ACC_TARGET_SET);
  if(npos==0){
    return ECMC_AXIS_DATA_ACC_TARGET_SET;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_DEC_TARGET_SET);
  if(npos==0){
    return ECMC_AXIS_DATA_DEC_TARGET_SET;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_SOFT_LIMIT_BWD);
  if(npos==0){
    return ECMC_AXIS_DATA_SOFT_LIMIT_BWD;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_SOFT_LIMIT_FWD);
  if(npos==0){
    return ECMC_AXIS_DATA_SOFT_LIMIT_FWD;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_SOFT_LIMIT_BWD_ENABLE);
  if(npos==0){
    return ECMC_AXIS_DATA_SOFT_LIMIT_BWD_ENABLE;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_SOFT_LIMIT_FWD_ENABLE);
  if(npos==0){
    return ECMC_AXIS_DATA_SOFT_LIMIT_FWD_ENABLE;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_TRAJ_DIRECTION);
  if(npos==0){
    return ECMC_AXIS_DATA_TRAJ_DIRECTION;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_ENC_HOMEPOS);
  if(npos==0){
    return ECMC_AXIS_DATA_ENC_HOMEPOS;
  }

  npos=strcmp(varName,ECMC_AXIS_DATA_STR_BLOCK_COM);
  if(npos==0){
    return ECMC_AXIS_DATA_BLOCK_COM;
  }

  return ECMC_AXIS_DATA_NONE;
}

int ecmcPLCDataIF::parseAndLinkEcDataSource(char* ecDataSource)
{
  int masterId=0;
  int slaveId=0;
  int bitId=0;
  char alias[EC_MAX_OBJECT_PATH_CHAR_LENGTH];  
  int errorCode=parseEcPath(ecDataSource, &masterId, &slaveId, alias,&bitId);
  if(errorCode){
    LOGERR("%s/%s:%d: ERROR: Parse %s failed (0x%x).\n",__FILE__, __FUNCTION__, __LINE__,ecDataSource,errorCode);
    return setErrorID(__FILE__,__FUNCTION__,__LINE__,errorCode);
  }

  if(ec_->getMasterIndex()!=masterId){
    LOGERR("%s/%s:%d: ERROR: Master %s not configured (0x%x).\n",__FILE__, __FUNCTION__, __LINE__,ecDataSource,ERROR_PLC_EC_MASTER_INVALID);
    return setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_PLC_EC_MASTER_INVALID);
  }

  ecmcEcSlave *slave=NULL;
  if(slaveId>=0){
    slave=ec_->findSlave(slaveId);
  }
  else{
    //Change exprtk var name ('-' not allowed in var name)
    std::stringstream ss;
    if(bitId>=0){
      ss << ECMC_EC_STR << masterId << "."ECMC_DUMMY_SLAVE_STR << -slaveId << "." << alias << "." << bitId  ;
    }
    else{
      ss << ECMC_EC_STR << masterId << "."ECMC_DUMMY_SLAVE_STR << -slaveId << "." << alias;
    }
    exprTkVarName_= ss.str();
    slave=ec_->getSlave(slaveId);
  }

  if(slave==NULL){
    LOGERR("%s/%s:%d: ERROR: Slave %s not configured (0x%x).\n",__FILE__, __FUNCTION__, __LINE__,ecDataSource,ERROR_PLC_EC_SLAVE_NULL);
    return setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_PLC_EC_SLAVE_NULL);
  }

  std::string sEntryID=alias;

  ecmcEcEntry *entry=slave->findEntry(sEntryID);

  if(entry==NULL){
    LOGERR("%s/%s:%d: ERROR: Entry %s not configured (0x%x).\n",__FILE__, __FUNCTION__, __LINE__,ecDataSource,ERROR_PLC_EC_ENTRY_NULL);
    return setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_PLC_EC_ENTRY_NULL);
  }

  errorCode=setEntryAtIndex(entry,ECMC_PLC_EC_ENTRY_INDEX,bitId);
  if(errorCode){
    return setErrorID(__FILE__,__FUNCTION__,__LINE__,errorCode);
  }
  
  errorCode=validateEntry(ECMC_PLC_EC_ENTRY_INDEX);
  if(errorCode){
    return setErrorID(__FILE__,__FUNCTION__,__LINE__,errorCode);
  }

  return 0;
}

int ecmcPLCDataIF::parseEcPath(char* ecPath, int *master,int *slave, char*alias,int *bit)
{
  int masterId=0;
  int slaveId=0;
  int bitId=0;
  int nvals=0;

  nvals = sscanf(ecPath,ECMC_EC_STR"%d."ECMC_SLAVE_CHAR"%d."ECMC_PLC_EC_ALIAS_FORMAT".%d",&masterId,&slaveId,alias,&bitId);
  if (nvals == 4){
	  *master=masterId;
	  *slave=slaveId;
	  *bit=bitId;
    return 0;
  }
  
  nvals = sscanf(ecPath, ECMC_EC_STR"%d."ECMC_SLAVE_CHAR"%d."ECMC_PLC_EC_ALIAS_FORMAT,&masterId,&slaveId,alias);
  if (nvals == 3){
	  *master=masterId;
	  *slave=slaveId;
	  *bit=-1;
    return 0;
  }
  return ERROR_PLC_EC_VAR_NAME_INVALID;
}

const char *ecmcPLCDataIF::getVarName()
{
  return varName_.c_str();
}

const char *ecmcPLCDataIF::getExprTkVarName()
{
  return exprTkVarName_.c_str();
}

int ecmcPLCDataIF::validate()
{
  if(getErrorID()){
    return getErrorID();
  }

  switch(source_){
    case ECMC_RECORDER_SOURCE_NONE:
      return setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_PLC_SOURCE_INVALID);
      break;

    case ECMC_RECORDER_SOURCE_ETHERCAT:
      if(!ec_){
        return setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_PLC_EC_NULL);
      }
      else{
	      return 0;
      }
      break;

    case ECMC_RECORDER_SOURCE_AXIS:
      if(!axis_){
        return setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_PLC_AXIS_NULL);
      }
      else{
	      return 0;
      }
      break;

    case ECMC_RECORDER_SOURCE_STATIC_VAR:
      return 0;
      break;

    case ECMC_RECORDER_SOURCE_GLOBAL_VAR:
      return 0;
      break;

    case ECMC_RECORDER_SOURCE_DATA_STORAGE:
      if(!ds_){
        return setErrorID(__FILE__,__FUNCTION__,__LINE__,ERROR_PLC_DATA_STORAGE_NULL);
      }
      else{
	      return 0;
      }
      break;
  }
  return ERROR_PLC_SOURCE_INVALID;
}

double ecmcPLCDataIF::getData()
{
  return data_;
}

void ecmcPLCDataIF::setData(double data)
{
  data_=data;
}

int ecmcPLCDataIF::setReadOnly(int readOnly)
{
  readOnly_=readOnly;
  return 0;
}