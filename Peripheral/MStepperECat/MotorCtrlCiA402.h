#ifndef MOTORCTRLCIA402_H
#define MOTORCTRLCIA402_H

#include "Ethercat.h" // Include the EtherCAT Library

#include "RingBuf.hpp"

class MotorCtrlCiA402
{

public:
    EthercatDevice_CiA402 motdrv;
    
    int32_t MnPosAcc;

    int ModuleIdx;
    int MotorIdx;
    int MaxMotorSpeed;
    int DCCycleTime;
    enum RunningState
    {
        detached = -2,
        disabled = -1,
        attached = 0,
        enabled = 1,
        fault=7,
        waitForStable = 2,
        running = 3,
    };

    RunningState state = RunningState::detached;

    MotorCtrlCiA402(int ModuleIdx, int MotorIdx, int MaxMotorSpeed, int DCCycleTime)
    {
        this->ModuleIdx = ModuleIdx;
        this->MotorIdx = MotorIdx;
        this->MaxMotorSpeed = MaxMotorSpeed;
        this->DCCycleTime = DCCycleTime;
        state = RunningState::detached;
    }

    virtual void attach(EthercatMaster &master)
    {
        state = RunningState::attached;
    }
    virtual void detach()
    {
        motdrv.detach();
        state = RunningState::detached;
    }

    int adj_steps=0;
    virtual void enable(bool en = true)
    {
        if (en)
        {
            if (state == RunningState::attached || state == RunningState::disabled)
            {
                
                int cmdLoc=motdrv.driveGetPositionActualValue();
                MnPosAcc = cmdLoc;
                driveUpdate(0);
                state = RunningState::enabled;
                motdrv.driveEnable();
            }
        }
        else
        {
            adj_steps=0;
            state = RunningState::disabled;
            motdrv.driveDisable();
        }
    }

    virtual void update(int32_t goDist)
    {
        if (state == running)
        {   
            if(motdrv.driveGetState() == CIA402_FAULT)
            {
                state = RunningState::fault;
                return;
            }



            driveUpdate(goDist);
            return;
        }


        if (state == RunningState::enabled)
        {
            if (motdrv.driveGetState() == CIA402_OPERATION_ENABLED)
            {
                state = RunningState::waitForStable;
            }

            return;
        }
        if (state == RunningState::waitForStable)
        {
            state = RunningState::running;
            
            int cmdLoc=motdrv.driveGetPositionActualValue();
            MnPosAcc = cmdLoc;
            return;
        }
    }

protected:
    virtual void driveUpdate(int32_t goDist)
    {
    }
};



class MotorCtrlCiA402_position_deltaB3 : public MotorCtrlCiA402
{

public:
    uint32_t gearRatioA=(1<<24);
    uint32_t gearRatioB=(1<<24);
    MotorCtrlCiA402_position_deltaB3(int ModuleIdx, int MotorIdx, int MaxMotorSpeed, int DCCycleTime) : MotorCtrlCiA402(ModuleIdx, MotorIdx, MaxMotorSpeed, DCCycleTime)
    {
    }


    void attach(EthercatMaster &master)
    {
        MotorCtrlCiA402::attach(master);

        if(MotorIdx>=0)
            motdrv.attach(ModuleIdx, MotorIdx, master); // Attach each CiA402 device (motor) to the EtherCAT Master
        else
            motdrv.attach(ModuleIdx, master); // Attach each CiA402 device (motor) to the EtherCAT Master

        //E gear ratio  A/B (hardwear pulse count)/(input pulse count)
        motdrv.sdoDownload32(0x6093,1,gearRatioA);//A
        motdrv.sdoDownload32(0x6093,2,gearRatioB);//B
        

        motdrv.setDc(DCCycleTime);                  // Set Distributed Clock (DC) parameters for the motor

        //  motor[i].driveSetMaxMotorSpeed(10000 );
        //  motor[i].driveSetMode(CIA402_CSV_MODE); // Set each motor to Cyclic Synchronous Position (CSP) mode
        //  motor[i].sdoDownload32(0x5011, i+1, 6400, 10000);//Motor Pulse
        //  motor[i].sdoDownload8(0x608c+i*0x80, 0, 0, 10000); //Velocity dimension index

        motdrv.driveSetMaxMotorSpeed(MaxMotorSpeed);
        motdrv.driveSetMode(CIA402_CSP_MODE);


        {//to master

            uint32_t tarAddr=0x1A01;
            motdrv.sdoDownload8(tarAddr, 0, 0);//sub index 0: last index 0 for disable
            
            motdrv.sdoDownload32(tarAddr, 1, 0x60410010);
            motdrv.sdoDownload32(tarAddr, 2, 0x60640020);
            // motdrv.sdoDownload8(tarAddr, 0, 2);//sub index 0: last index
            motdrv.sdoDownload32(tarAddr, 3, 0x60FD0020);//digital input
            motdrv.sdoDownload8(tarAddr, 0, 3);
        }


        {//to slave

            uint32_t tarAddr=0x1601;
            motdrv.sdoDownload8(tarAddr, 0, 0);//sub index 0: last index 0 for disable
            
            motdrv.sdoDownload32(tarAddr, 1, 0x60400010);
            motdrv.sdoDownload32(tarAddr, 2, 0x607A0020);
            // motdrv.sdoDownload8(tarAddr, 0, 2);//sub index 0: last index
            motdrv.sdoDownload32(tarAddr, 3, 0x24060010);//Digital output
            motdrv.sdoDownload8(tarAddr, 0, 3);
        }


        
        // motdrv.sdoDownload8(tarAddr, 0, 2);

        // djrl.dbg_printf(" mot[%d] alias Address:%d",MotorIdx,
        //   motor[i].getAliasAddress());
    }



    bool setDO(uint16_t DO_pdo_reg)
    {
        uint8_t pdoBufferOutputL=motdrv.pdoGetOutputBytes();
        
        uint8_t *pdoBufferOutput=motdrv.pdoGetOutputBuffer();

        if(pdoBufferOutputL>=8 && pdoBufferOutput)
        {
            pdoBufferOutput[6]=DO_pdo_reg&0xFF;
            pdoBufferOutput[7]=DO_pdo_reg>>8;
            return true;
        }
        return false;

    }


    int waitForStableCounter=0;
    virtual void update(int32_t goDist)
    {
            waitForStableCounter++;
        if (state == RunningState::waitForStable)
        {
            state = RunningState::running;
            // int cmdLoc=motdrv.driveGetPositionActualValue();
            // // motdrv.driveGetAdditionalActualPosition();
            // MnPosAcc = cmdLoc;
            return;
        }
        else 
        {
            MotorCtrlCiA402::update(goDist);
        }
    }
protected:
    virtual void driveUpdate(int32_t goDist)
    {
        MnPosAcc += goDist;
        motdrv.driveSetTargetPosition(MnPosAcc);
    }
};





class MotorCtrlCiA402_FeedBackStepper:public MotorCtrlCiA402
{

public:
    int cmdPosHistL=4;
    int32_t cmdPosHist[4];
    int32_t init_enc_offset;

    int MnPosInit;
    int encStepPerRev=8192;
    bool encoder_check=false;

    MotorCtrlCiA402_FeedBackStepper(int ModuleIdx, int MotorIdx, int MaxMotorSpeed, int DCCycleTime):
        MotorCtrlCiA402(ModuleIdx, MotorIdx, MaxMotorSpeed, DCCycleTime)
    {
        
        encoder_check=true;
    }


    int stableCD = 1000;
    int allowedError=70;
    
    int CMDPosCompOffset_d100=150;

    int motor_skip_adjust_thres=0;
    int latest_enc_error=0;
    virtual void update(int32_t goDist)
    {
        if (state == running)
        {   
            int substeps=16;
            int motor_steps_per_rev=400;

            // driveGetAdditionalActualPosition reads from encoder with some delay
            int32_t cur_Enc = (int32_t)((uint32_t)(motdrv.driveGetAdditionalActualPosition(0)) << 1) / 2;
            int32_t EncDiff = cur_Enc - init_enc_offset;
            int32_t encoder_enc0 = EncDiff;

            
            // driveGetPositionActualValue reads motor current cmd position in the controller
            int32_t posDiff = motdrv.driveGetPositionActualValue() - MnPosInit;

            for(int k=cmdPosHistL-1;k;k--)
            {
                cmdPosHist[k] = cmdPosHist[k-1];
            }
            cmdPosHist[0] = posDiff;

            int divSize=100;
            //  0~100 is value between cmdPosHist[0]~cmdPosHist[1]
            //100~200 is value between cmdPosHist[1]~cmdPosHist[2]
            
            if(CMDPosCompOffset_d100>(cmdPosHistL*divSize-1))CMDPosCompOffset_d100=cmdPosHistL*divSize-1;
            int idx0=CMDPosCompOffset_d100/divSize;
            int percent=divSize-(CMDPosCompOffset_d100-idx0*divSize);
            int value0=cmdPosHist[idx0];
            int value1=cmdPosHist[idx0+1];



            int32_t predict_enc1 =-(value0*(percent)+value1*(divSize-percent))/divSize* encStepPerRev / (substeps*motor_steps_per_rev);
            // predict_enc1 = -(pre_posDiff2*8+pre_posDiff3*2)/10* encStepPerRev / (substeps*motor_steps_per_rev);
            latest_enc_error=encoder_enc0 - predict_enc1;

            int32_t absError=latest_enc_error>0?latest_enc_error:-latest_enc_error;
            if (absError>allowedError) // step skip detection
            {
                if(encoder_check )
                {

                    enable(false);
                    return;
                }




            }

            static int adjTimeSpace=0;
            if(motor_skip_adjust_thres && absError>motor_skip_adjust_thres)
            {

                if(encoder_enc0>predict_enc1)
                {  
                    if(adj_steps<=0)
                    {
                        adj_steps+=substeps;
                        adjTimeSpace=0;
                    }
                }
                else if(adj_steps>=0)
                {
                    if(adj_steps>=0)
                    {
                        adj_steps-=substeps;
                        adjTimeSpace=0;
                    }
                }

            }

            if(adjTimeSpace)
                adjTimeSpace--;

            int slowSpeed=5;
            if(adj_steps!=0 && (goDist>-slowSpeed && goDist<slowSpeed) && adjTimeSpace==0)
            {
                int adj=0;
                if(adj_steps>0)
                {
                    adj=-1;
                }
                else
                {
                    adj=1;
                }
                
                adj_steps+=adj;
                MnPosInit-=adj;
                goDist-=adj;
                

                
                for(int k=0;k<cmdPosHistL;k++)
                {
                    cmdPosHist[k]-=adj;
                }



                adjTimeSpace=1;
            }
            
            driveUpdate(goDist);

            return;
        }


        if (state == RunningState::enabled)
        {
            if (motdrv.driveGetState() == CIA402_OPERATION_ENABLED)
            {
                driveUpdate(0);
                state = RunningState::waitForStable;
                init_enc_offset=-9999;
                stableCD=2000;
            }

            return;
        }
        if (state == RunningState::waitForStable)
        {
            int32_t enc_offset = (int32_t)((uint32_t)(motdrv.driveGetAdditionalActualPosition(0)) << 1) / 2;

            int cmdLoc=motdrv.driveGetPositionActualValue();

            {

                if(abs(init_enc_offset-enc_offset)<5)//make sure the position is static
                {

                    stableCD--;

                }
                else
                {
                    init_enc_offset=enc_offset;
                    stableCD=2000;
                }
            }

            
            if (stableCD == 0)
            {
                init_enc_offset =enc_offset;
                MnPosAcc = MnPosInit = cmdLoc;
                // testCounter=0;
                // testCounter2=10;
                
                for(int k=0;k<cmdPosHistL;k++)
                {
                    cmdPosHist[k] =0;
                }
                state = RunningState::running;
            }
            return;
        }
    }

};

class MotorCtrlCiA402_FeedBackStepper_position : public MotorCtrlCiA402_FeedBackStepper
{
public:
    MotorCtrlCiA402_FeedBackStepper_position(int ModuleIdx, int MotorIdx, int MaxMotorSpeed, int DCCycleTime) : MotorCtrlCiA402_FeedBackStepper(ModuleIdx, MotorIdx, MaxMotorSpeed, DCCycleTime)
    {
    }
    void attach(EthercatMaster &master)
    {
        MotorCtrlCiA402_FeedBackStepper::attach(master);

        if(MotorIdx>=0)
            motdrv.attach(ModuleIdx, MotorIdx, master); // Attach each CiA402 device (motor) to the EtherCAT Master
        else
            motdrv.attach(ModuleIdx, master); // Attach each CiA402 device (motor) to the EtherCAT Master

        motdrv.setDc(DCCycleTime);                  // Set Distributed Clock (DC) parameters for the motor

        //  motor[i].driveSetMaxMotorSpeed(10000 );
        //  motor[i].driveSetMode(CIA402_CSV_MODE); // Set each motor to Cyclic Synchronous Position (CSP) mode
        //  motor[i].sdoDownload32(0x5011, i+1, 6400, 10000);//Motor Pulse
        //  motor[i].sdoDownload8(0x608c+i*0x80, 0, 0, 10000); //Velocity dimension index

        motdrv.driveSetMaxMotorSpeed(MaxMotorSpeed);
        motdrv.driveSetMode(CIA402_CSP_MODE);

        // djrl.dbg_printf(" mot[%d] alias Address:%d",MotorIdx,
        //   motor[i].getAliasAddress());
    }

protected:
    virtual void driveUpdate(int32_t goDist)
    {
        MnPosAcc += goDist;
        motdrv.driveSetTargetPosition(MnPosAcc);
    }
};

class MotorCtrlCiA402_speedControled_FeedBackStepper_position : public MotorCtrlCiA402_FeedBackStepper
{
public:
    MotorCtrlCiA402_speedControled_FeedBackStepper_position(int ModuleIdx, int MotorIdx, int MaxMotorSpeed, int DCCycleTime) : MotorCtrlCiA402_FeedBackStepper(ModuleIdx, MotorIdx, MaxMotorSpeed, DCCycleTime)
    {
    }
    void attach(EthercatMaster &master)
    {
        MotorCtrlCiA402_FeedBackStepper::attach(master);
        motdrv.attach(ModuleIdx, MotorIdx, master); // Attach each CiA402 device (motor) to the EtherCAT Master
        motdrv.setDc(DCCycleTime);                  // Set Distributed Clock (DC) parameters for the motor

        motdrv.driveSetMaxMotorSpeed(MaxMotorSpeed);
        motdrv.driveSetMode(CIA402_CSV_MODE);                       // Set each motor to Cyclic Synchronous Position (CSP) mode
        motdrv.sdoDownload32(0x5011, MotorIdx + 1, 6400, 10000);    // Motor Pulse
        motdrv.sdoDownload8(0x608c + MotorIdx * 0x80, 0, 0, 10000); // Velocity dimension index

        //  motor[i].driveSetMaxMotorSpeed(10000 );
        //  motor[i].driveSetMode(CIA402_CSV_MODE); // Set each motor to Cyclic Synchronous Position (CSP) mode
        //  motor[i].sdoDownload32(0x5011, i+1, 6400, 10000);//Motor Pulse
        //  motor[i].sdoDownload8(0x608c+i*0x80, 0, 0, 10000); //Velocity dimension index

        // djrl.dbg_printf(" mot[%d] alias Address:%d",MotorIdx,
        //   motor[i].getAliasAddress());
    }

protected:
    virtual void driveUpdate(int32_t goDist)
    {

        motdrv.driveSetTargetVelocity(goDist * 1000); // steps/us
    }
};

#endif
