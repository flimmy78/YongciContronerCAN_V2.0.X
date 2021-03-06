
#include "Action.h"

/** 
 * <p>application name： Action.c</p> 
 * <p>application describing： 配置Action</p> 
 * <p>copyright： Copyright (c) 2017 Beijing SOJO Electric CO., LTD.</p> 
 * <p>company： SOJO</p> 
 * <p>time： 2017.05.20</p> 
 * 
 * @updata:[日期YYYY-MM-DD] [更改人姓名][变更描述]
 * @author ZhangXiaomou 
 * @version ver 1.0
 */
#include "../Header.h"
#include "../SerialPort/RefParameter.h"
#include <string.h>
#define SUDDEN_ID 0x9A     //突发状态上传ID
#define ID_GET_TEMP 0x45    //!< 温度监控参数ID

static void SendAckMesssage(uint8_t fun);
static void GetLoopSwitch(struct DefFrameData* pReciveFrame);
static void SendMonitorParameter(struct DefFrameData* pReciveFrame);
static uint8_t SynCloseReady(struct DefFrameData* pReciveFrame, struct DefFrameData* pSendFrame);
static uint8_t GetLoopSet(struct DefFrameData* pReciveFrame);
static uint8_t CheckOpenCondition(void);
static uint8_t CheckCloseCondition(void);
static uint8_t ReadyCloseOrOpen(struct DefFrameData* pReciveFrame,  struct DefFrameData* pSendFrame,uint8_t id);
static uint8_t ActionCloseOrOpen(struct DefFrameData* pReciveFrame, struct DefFrameData* pSendFrame, uint8_t id);
static uint8_t SyncCloseSingleCheck(struct DefFrameData* pReciveFrame, struct DefFrameData* pSendFrame);
static uint8_t ConfigModeOperation(struct DefFrameData* pReciveFrame, struct DefFrameData* pSendFrame);
RemoteControlState g_RemoteControlState; //远方控制状态标识位


/**
 * 同步动作属性
 */
ActionAttribute g_SynActionAttribute;

/**
 * 普通合分闸动作属性
 */
ActionAttribute g_NormalAttribute;

/**
 * 暂存命令字
 */
uint8_t LastCommand[8] = {0};//最新的命令字

SystemSuddenState g_SuddenState;    //需要上传的机构状态值Action.h
struct DefFrameData ActionCommandTemporaryAck;   //CAN数据帧
PointUint8 g_ParameterBuffer;   

static uint8_t AckBuffer[8] = {0};
static uint16_t EffectiveLevelSignalCount = 0;


/**
 * Action参数初始化
 */
void ActionParameterInit(void)
{
    g_SynActionAttribute.count = 0;
    g_SynActionAttribute.loopByte = 0;
    g_SynActionAttribute.currentIndex = 0; 
     
    g_SynActionAttribute.Attribute[0].enable = 0;
    g_SynActionAttribute.Attribute[0].loop = 0;    
    g_SynActionAttribute.Attribute[0].offsetTime = 0;
    g_SynActionAttribute.Attribute[0].readyFlag = 0;
      
    g_SynActionAttribute.Attribute[1].enable = 0;
    g_SynActionAttribute.Attribute[1].loop = 0;   
    g_SynActionAttribute.Attribute[1].offsetTime = 0;
    g_SynActionAttribute.Attribute[1].readyFlag = 0;
   
    g_SynActionAttribute.Attribute[2].enable = 0;
    g_SynActionAttribute.Attribute[2].loop = 0;
    g_SynActionAttribute.Attribute[2].offsetTime = 0;
    g_SynActionAttribute.Attribute[2].readyFlag = 0;
  
    g_NormalAttribute.count = 0;
    g_NormalAttribute.loopByte = 0;
    g_NormalAttribute.currentIndex = 0; 
     
    g_NormalAttribute.Attribute[0].enable = 0;
    g_NormalAttribute.Attribute[0].loop = 0;    
    g_NormalAttribute.Attribute[0].offsetTime = 0;
    g_NormalAttribute.Attribute[0].readyFlag = 0;
      
    g_NormalAttribute.Attribute[1].enable = 0;
    g_NormalAttribute.Attribute[1].loop = 0;   
    g_NormalAttribute.Attribute[1].offsetTime = 0;
    g_NormalAttribute.Attribute[1].readyFlag = 0;
   
    g_NormalAttribute.Attribute[2].enable = 0;
    g_NormalAttribute.Attribute[2].loop = 0;
    g_NormalAttribute.Attribute[2].offsetTime = 0;
    g_NormalAttribute.Attribute[2].readyFlag = 0;   
    
    ActionCommandTemporaryAck.pBuffer = AckBuffer;
    
}


/**
 * 引用帧服务
 *
 * @param  指向接收数据的指针
 * @param  指向发送数据的指针
 * @bref   对完整帧进行提取判断
 */
uint8_t FrameServer(struct DefFrameData* pReciveFrame, struct DefFrameData* pSendFrame)
{
    if(pReciveFrame->pBuffer[0] > 0x20 || pReciveFrame->len == 0)
    {
        return ID_ERROR;
    }
    uint8_t i = 1;    
    uint8_t id = pReciveFrame->pBuffer[0];
    uint8_t error = 0;  //错误号
    uint8_t result = 0;
    
    if(!CheckLockState()) //在锁定模式下不允许执行任何的操作
    {      
        return LOCK_ERROR;
    }
    
    /*就地控制时可以读取和设置参数，而不能执行分合闸、以及阈值指令*/
    if(id <= 5)
    {
        if(g_SystemState.workMode == DEBUG_STATE) //调试模式下不能执行
        {          
            return RUN_MODE_ERROR;
        }
        if(g_SystemState.yuanBenState == BEN_STATE)
        {           
            return WORK_MODE_ERROR;
        }
        for(i = 1;i < pReciveFrame->len;i++)
		{
            ClrWdt();
			ActionCommandTemporaryAck.pBuffer[i] = pReciveFrame->pBuffer[i];
		}
    }
    
    ActionCommandTemporaryAck.ID = MAKE_GROUP1_ID(GROUP1_POLL_STATUS_CYCLER_ACK, DeviceNetObj.MACID);
    ActionCommandTemporaryAck.pBuffer[0] = id | 0x80;
    ActionCommandTemporaryAck.len = pReciveFrame->len;
    
    switch(id)
    {
        case ReadyClose : //合闸预制
        {      
            pSendFrame->len = 0;//禁止底层发送
            return ReadyCloseOrOpen( pReciveFrame, &ActionCommandTemporaryAck, id);
           
        }
        case CloseAction: //合闸执行
        {           
            pSendFrame->len = 0;//禁止底层发送
            return ActionCloseOrOpen( pReciveFrame, &ActionCommandTemporaryAck, id);        
        }
        case ReadyOpen: //分闸预制
        {
            if(g_SystemState.charged)   //带电情况下不能执行分闸操作
            {
                result = CHARGED_CONFIG_ERROR;
            }
            else
            {
                pSendFrame->len = 0;//禁止底层发送
                result = ReadyCloseOrOpen( pReciveFrame, &ActionCommandTemporaryAck, id);
            }
            return result;
        }
        case OpenAction: //分闸执行
        {
           pSendFrame->len = 0;//禁止底层发送
           return  ActionCloseOrOpen( pReciveFrame, &ActionCommandTemporaryAck, id);
        }        
        case SyncReadyClose: //同步合闸预制
        {
            pSendFrame->len = 0;//禁止底层发送
             return SynCloseReady(pReciveFrame, &ActionCommandTemporaryAck);
        }       
        case MasterParameterSetOne:  //非顺序参数设置
        {
            pSendFrame->len = 0;//禁止底层发送
            if (g_SystemState.congfigMode != ENTER_CONFIG)
            {               
                return  ERROR_CONFIG_MODE;
            }
            ClrWdt();
            ActionCommandTemporaryAck.pBuffer[1] = pReciveFrame->pBuffer[1];  //配置号
            g_ParameterBuffer.pData = pReciveFrame->pBuffer + 2;
            error = SetParamValue(pReciveFrame->pBuffer[1], &g_ParameterBuffer);
            if(error == 0xFF)
            {
                ClrWdt();              
                return ID_ERROR;
            }
            else if(error)
            {
                ClrWdt();               
                return DATA_LEN_ERROR;
            }
            ActionCommandTemporaryAck.pBuffer[2] = g_ParameterBuffer.pData[0];
            ActionCommandTemporaryAck.pBuffer[3] = g_ParameterBuffer.pData[1];
            ActionCommandTemporaryAck.len = g_ParameterBuffer.len + 2;
            ClrWdt();
            SendData(&ActionCommandTemporaryAck);
            g_RemoteControlState.setFixedValue = TRUE;  //设置定值成功
            
            return 0;
        }
        case MasterParameterRead:  //顺序参数读取
        {    
            pSendFrame->len = 0;//禁止底层发送
            SendMonitorParameter(pReciveFrame);
            return 0;
        }  
        case ConfigMode:// 配置模式
        {
            return ConfigModeOperation(pReciveFrame, pSendFrame);
        }
        default:
        {
            //错误的ID号处理
            ClrWdt();
            return ID_ERROR;
        }          
    }
}
static uint8_t ActionCloseOrOpen(struct DefFrameData* pReciveFrame, struct DefFrameData* pSendFrame, uint8_t id)
{
    uint8_t loopID = 0;
    uint8_t result = 0;
    uint16_t order = 0;
    
    
    if(g_SystemState.workMode == DEBUG_STATE) //调试模式下不能执行
    {
        return RUN_MODE_ERROR;
    }
    if(g_SystemState.yuanBenState == BEN_STATE) //本地模式不能执行远方操作
    {        
        return WORK_MODE_ERROR;
    }   
    
    loopID = pReciveFrame->pBuffer[1];
#if(CAP3_STATE)
    if ((loopID == 0) || (loopID > 0x07))
    {
        return LOOP_ERROR;
    }
#else
    if ((loopID == 0) || (loopID > 0x03))
    {
        return LOOP_ERROR;
    }
#endif
    result = CheckLoopCapVoltage(loopID);  //检测所有电容电压状态是否正确
    if(result)
    {
        return result;
    }
    
     //区分合分闸检测
    if (id == CloseAction )
    {
        result =  CheckCloseCondition(); //检测合分闸条件
        order = HE_ORDER;
    }
    else if (id == OpenAction)
    {
        result =  CheckOpenCondition(); //检测合分闸条件
        order = FEN_ORDER;
    }
    else
    {
        return ID_ERROR;
    }
     
    //是否已经预制或者超时                 
    if((g_RemoteControlState.receiveStateFlag != order) || 
        (g_RemoteControlState.overTimeFlag != TRUE))
    {
        ClrWdt();
        SendErrorFrame(id, NOT_PERFABRICATE_ERROR);
        g_RemoteControlState.receiveStateFlag = IDLE_ORDER;  //空闲命令
    }
   //比较配置字是否一致
    for(uint8_t i = 1 ; i < pReciveFrame->len; i++)
    {
        if (pReciveFrame->pBuffer[i] != LastCommand[i])
        {          
            return DIFF_CONFIG_ERROR;
        }
    }     
    
     
    OnLock();   //上锁             
    ClrWdt();               
    SendData(pSendFrame);
    
    OnLock();   //上锁 
    if (id == CloseAction )
    {
        CloseOperation();      
    }
    else if (id == OpenAction)
    {
        OpenOperation();
    }
    
    g_RemoteControlState.overTimeFlag = FALSE;  //Clear Overtime Flag    
    g_RemoteControlState.receiveStateFlag = IDLE_ORDER;  //空闲命令     
    return 0;
}
static uint8_t ReadyCloseOrOpen(struct DefFrameData* pReciveFrame, struct DefFrameData* pSendFrame, uint8_t id)
{
    uint8_t result = 0;
    uint8_t loop = 0;
    uint16_t order = 0;
    if(g_SystemState.workMode == DEBUG_STATE) //调试模式下不能执行
    {
        return RUN_MODE_ERROR;
    }
    if(g_SystemState.yuanBenState == BEN_STATE) //本地模式不能执行远方操作
    {       
        return WORK_MODE_ERROR;
    }      
     if (g_RemoteControlState.receiveStateFlag != IDLE_ORDER)//检测是否多次预制
    {                
        g_RemoteControlState.receiveStateFlag = IDLE_ORDER;       
        g_RemoteControlState.overTimeFlag = FALSE;  //预制成功后才会开启超时检测
        return SEVERAL_PERFABRICATE_ERROR;
    }   
    result = GetLoopSet(pReciveFrame);   //获取回路   
    if (result)
    {       
        return result;
    }   
    loop = pReciveFrame->pBuffer[1];
    result = CheckLoopCapVoltage(loop);  //检测所有电容电压状态是否正确
    if(result)
    {
        return result;
    }
    //区分合分闸检测
    if (id == ReadyClose)
    {
        result =  CheckCloseCondition(); //检测合分闸条件
        order = HE_ORDER;
    }
    else if (id == ReadyOpen)
    {
        result =  CheckOpenCondition(); //检测合分闸条件
        order = FEN_ORDER;
    }
    else
    {
        return ID_ERROR;
    }
    
    if (result)
    {
          return result;
    } 
    ClrWdt();  

    memcpy(LastCommand, pReciveFrame->pBuffer, pReciveFrame->len);//暂存预制指令
    g_RemoteControlState.overTimeFlag = TRUE;  //预制成功后才会开启超时检测
    SendData(pSendFrame);
    g_RemoteControlState.receiveStateFlag = order;    //合闸命令
    g_RemoteControlState.orderId = id;
    g_TimeStampCollect.overTime.delayTime = g_RemoteWaitTime;//设置预制超时等待时间     
    g_TimeStampCollect.overTime.startTime = g_TimeStampCollect.msTicks; 
    return 0;
}


static uint8_t SynCloseReady(struct DefFrameData* pReciveFrame, struct DefFrameData* pSendFrame)
{
    uint8_t id = 0;
    uint8_t configbyte = 0;
    uint8_t loop[3] = {0};
    if(SyncCloseSingleCheck(pReciveFrame, pSendFrame))
    {
        return SIGNEL_INVALID_ERROR;
    }
    if(g_SystemState.workMode == DEBUG_STATE) //调试模式下不能执行
    {
        return RUN_MODE_ERROR;
    }
    if(g_SystemState.yuanBenState == BEN_STATE) //本地模式不能执行远方操作
    {
        return WORK_MODE_ERROR;
    }
    //数据长度不对，数据长度不应为奇数
    if(pReciveFrame->len % 2 != 0) 
    {
        ClrWdt();      
        return DATA_LEN_ERROR;
    }
    //必须不小于4
    if (pReciveFrame->len < 2)
    {
        return DATA_LEN_ERROR;
    }
        
    id  = pReciveFrame->pBuffer[0];
    configbyte = pReciveFrame->pBuffer[1];
           
    //分合位错误
    if((g_SystemState.heFenState1 != OPEN_STATE) || 
       (g_SystemState.heFenState2 != OPEN_STATE) || 
       (g_SystemState.heFenState3 != OPEN_STATE))
    {       
        return HEFEN_STATE_ERROR;
    }   
    
    //电容电压未达到设定电压错误
    if(CheckLoopCapVoltage(LOOP_ID_ALL))
    {
        return CAPVOLTAGE_ERROR;
    }
    
    uint8_t count = (pReciveFrame->len - 2) / 2 + 1;//参数时间为差值，比回路数少一个
    
    //检查回路数
#if(CAP3_STATE)
    if (count > 3)
    {
        return LOOP_ERROR;
    }
#else
    if (count > 2)
    {
        return LOOP_ERROR;
    }
#endif
    
    //获取回路参数   
    for(uint8_t i = 0; i < count; i++)
    {
        loop[i] = (uint8_t)((configbyte>>(2*i)) & 0x03); 
        if (loop[i] == 0)
        {
            return  LOOP_ERROR;
        }
        //TODO:添加回路数限制        
    }
    
    //简单查重
    if (count == 2)
    {
        if (loop[0] == loop[1])
        {
            return LOOP_ERROR;
        }
    }
    else  if (count == 3)
    {
        if ((loop[0] == loop[1]) || (loop[1] == loop[2])
                || (loop[0] == loop[2]))
        {
            return LOOP_ERROR;
        } 
    }
    
    //按照执行顺序参数赋值    
    g_SynActionAttribute.count = count;
    g_SynActionAttribute.loopByte = configbyte;
    g_SynActionAttribute.currentIndex = 0;

    g_SynActionAttribute.Attribute[0].enable = TRUE;
    g_SynActionAttribute.Attribute[0].loop = loop[0];        
    g_SynActionAttribute.Attribute[0].offsetTime = 0;
        
    uint16_t time = 0;
    for(uint8_t i = 1; i < count; i++)
    {
         
        g_SynActionAttribute.Attribute[i].enable = TRUE;
        g_SynActionAttribute.Attribute[i].loop = loop[i];       
        time = pReciveFrame->pBuffer[2*i + 1];   
        time = (time<<8)|  pReciveFrame->pBuffer[2*i];   
        g_SynActionAttribute.Attribute[i].offsetTime = time;
    }   
    for (uint8_t i = count; i < 3;i++)
    {
         g_SynActionAttribute.Attribute[i].enable = FALSE;
    }
    
    g_RemoteControlState.orderId = id;
    
    if(g_RemoteControlState.receiveStateFlag == IDLE_ORDER)
    {               
        ClrWdt();              
      
        g_TimeStampCollect.overTime.delayTime = g_SyncReadyWaitTime;//设置同步预制超时等待时间
        g_TimeStampCollect.overTime.startTime = g_TimeStampCollect.msTicks;
        ClrWdt();
        g_RemoteControlState.receiveStateFlag = TONGBU_HEZHA;    //同步合闸命令
        g_RemoteControlState.overTimeFlag = TRUE;  //预制成功后才会开启超时检测        
        OnLock();      
        
                   
        TurnOnInt2();   //必须是在成功的预制之后才能开启外部中断1 
        OFF_COMMUNICATION_INT();
        OnLock();
        ClrWdt();
        __delay_ms(5);
        ClrWdt();
        __delay_ms(5); //留出同步控制器进入检测的时间
        SendData(pSendFrame);
       
       
    }
    else
    {
        TurnOffInt2();
        g_RemoteControlState.receiveStateFlag = IDLE_ORDER;
        return SEVERAL_PERFABRICATE_ERROR;    //多次预制
    }
    return 0;
}

/**
 * 
 * @description: 检测同步合闸信号是否正确
 * @param pReciveFrame
 * @param pSendFrame
 * @return 
 */
static uint8_t SyncCloseSingleCheck(struct DefFrameData* pReciveFrame, struct DefFrameData* pSendFrame)
{
    uint8_t i = 0;
    uint8_t state = 0x00;
    uint8_t result = 0;
    uint32_t waitCount = 0;
    EffectiveLevelSignalCount = 0;
    OFF_COMMUNICATION_INT();    //关闭通信中断    
    while(RXD1_LASER == 0)
    {
        ClrWdt();
        waitCount++;
        if(waitCount >= 1000000)    //大约1s
        {
            return SIGNEL_INVALID_ERROR;   //退出
        }
    }
    //检测高电平
    for(i = 0; i < 4; i++)
    {
        state = ~state;
        state &= 0x01;
        while(RXD1_LASER == state)
        {
            EffectiveLevelSignalCount++;
            if(EffectiveLevelSignalCount >= 0xFFFF)    //防止进入死循环//TODO:设定最大值
            {
                ON_COMMUNICATION_INT();    //开启通信中断
                return SIGNEL_INVALID_ERROR;   //退出
            }
        }
        if((EffectiveLevelSignalCount < 60) || (EffectiveLevelSignalCount > 100))  //较宽的范围85~125
        {
            break;
        }
        EffectiveLevelSignalCount = 0;
    }
    if((RXD1_LASER == 1) && (i == 4))
    {
        result = 0; //成功接收到
    }
    else
    {
        result = SIGNEL_INVALID_ERROR;
    }
    ON_COMMUNICATION_INT();    //开启通信中断
    if(result == 0)
    {
        for(uint8_t i = 1; i < pReciveFrame->len; i++)
        {
            ClrWdt();
            pSendFrame->pBuffer[i] = pReciveFrame->pBuffer[i];
        } 
      //  SendData(pSendFrame);
    }
    return result;

}

/**
 * @description: 发送错误帧数据
 * @param receiveID 主站发送的ID号
 * @param errorID   错误代码
 */
void SendErrorFrame(uint8_t receiveID, uint8_t errorID)
{
    uint8_t data[8] = {0,0,0,0,0,0,0,0};
    struct DefFrameData pSendFrame;
    
    pSendFrame.pBuffer = data;
    pSendFrame.complteFlag = 0xFF;
    ClrWdt();
    pSendFrame.ID =  MAKE_GROUP1_ID(GROUP1_POLL_STATUS_CYCLER_ACK, DeviceNetObj.MACID);
    pSendFrame.pBuffer[0] = ERROR_REPLY_ID;   //错误应答ID
    pSendFrame.pBuffer[1] = receiveID;  //主站发送ID
    pSendFrame.pBuffer[2] = errorID;   //错误代码
    pSendFrame.pBuffer[3] = ERROR_EXTEND_ID;  //扩展ID号            
    pSendFrame.len = ERROR_DATA_LEN;   //错误帧长度
    ClrWdt();
    SendData(&pSendFrame);
}


/**
 * 
 * <p>Function name: [UpdataState]</p>
 * <p>Discription: [对运行状态进行更新显示]</p>
 */
void UpdataState(void)
{
    uint8_t data[8] = {0,0,0,0,0,0,0,0};
    uint8_t offsetCount = 0;
    uint8_t i = 0;
    uint8_t state = 0;
    
    struct DefFrameData pSendFrame;

    pSendFrame.pBuffer = data;
    pSendFrame.complteFlag = 0xFF;

    ClrWdt();
    pSendFrame.ID = MAKE_GROUP1_ID(GROUP1_STATUS_CYCLE_ACK, DeviceNetObj.MACID);
	pSendFrame.pBuffer[0] = SUDDEN_ID;   //突发状态ID
    
    if(g_SuddenState.switchsuddenFlag || g_SuddenState.capSuddentFlag)
    {
        offsetCount = 0;
        if(g_SuddenState.switchsuddenFlag)
        {
            g_SuddenState.buffer[0] = 0;    //Clear
        }
        if(g_SuddenState.capSuddentFlag)
        {
            g_SuddenState.buffer[1] = 0;    //Clear
        }
        for(i = 0;i < LOOP_COUNT; i++)   //循环赋值
        {
            g_SuddenState.buffer[0] |= (g_SuddenState.switchState[i] << offsetCount);
            g_SuddenState.buffer[1] |= (g_SuddenState.capState[i] << offsetCount);
            g_SuddenState.executeOrder[i] <<= offsetCount;
            offsetCount += 2;   
        }
        g_SuddenState.switchsuddenFlag = FALSE; //Clear
        g_SuddenState.capSuddentFlag = FALSE;   //Clear
    }
    
    pSendFrame.pBuffer[1] = g_SuddenState.buffer[0];	
    
#if(CAP3_STATE)
    pSendFrame.pBuffer[2] = g_SuddenState.executeOrder[DEVICE_I] | g_SuddenState.executeOrder[DEVICE_II] | g_SuddenState.executeOrder[DEVICE_III];	
#else
    pSendFrame.pBuffer[2] = g_SuddenState.executeOrder[DEVICE_I] | g_SuddenState.executeOrder[DEVICE_II];	
#endif
  
    pSendFrame.pBuffer[3] = g_SuddenState.buffer[1];	
    
	if(!g_SystemState.warning)
	{
        ClrWdt();
		pSendFrame.pBuffer[4] = 1;
	}
	else
	{
        ClrWdt();
		pSendFrame.pBuffer[4] = 0;
	}
    
    //远方就地
    if(g_SystemState.yuanBenState == YUAN_STATE) //远方
    {
        state |= 1 << 1;
    }
    else
    {
        state |= 1 << 0;
    }
    
    //工作模式
    if(g_SystemState.workMode == WORK_STATE)    //工作模式
    {
        state |= 1 << 3;
    }
    else
    {
        state |= 1 << 2;
    }

    //带电与否
    if(g_SystemState.charged)   //带电
    {
        state |= 1 << 5;
    }
    else
    {
        state |= 1 << 4;
    }
    
    pSendFrame.pBuffer[5] = state;  //将上述标志赋值上传
    pSendFrame.len = 6;   //数据帧长度
    SendData(&pSendFrame);
    
    ClrWdt();
    g_SuddenState.executeOrder[DEVICE_I] = 0;
    g_SuddenState.executeOrder[DEVICE_II] = 0;
#if(CAP3_STATE)
    g_SuddenState.executeOrder[DEVICE_III] = 0;
#endif
}


/**
 * 获取合分闸设置回路参数
 * @retrun 错误代码
 */
static uint8_t GetLoopSet(struct DefFrameData* pReciveFrame)
{
    if(pReciveFrame->len != 3)
    {
        return DATA_LEN_ERROR;
    }
    uint8_t configLoop =  pReciveFrame->pBuffer[1];//回路选择配置字
#if(CAP3_STATE)
    if ((configLoop == 0) || (configLoop > 0x07))
    {
        return  LOOP_ERROR;
    }
#else
    if ((configLoop == 0) || (configLoop > 0x03))
    {
        return  LOOP_ERROR;
    }
#endif
    g_NormalAttribute.count = 0;
    for(uint8_t i = 0; i < LOOP_COUNT; i++)
    {       
        if (configLoop & (1<<i))
        {
            g_NormalAttribute.Attribute[i].enable = TRUE;
            g_NormalAttribute.Attribute[i].loop = i+1;
            g_NormalAttribute.Attribute[i].offsetTime = 0;            
            g_NormalAttribute.count++;            
        }
        else
        {
            g_NormalAttribute.Attribute[i].enable = FALSE;
        }
            
    }
    return 0;
}

/**
 * 检查合闸条件
 * @return 错误代码
 */
static uint8_t CheckCloseCondition(void)
{
    for(uint8_t i = 0; i< LOOP_COUNT; i++)
    {
        if( g_NormalAttribute.Attribute[i].enable)
        {
            switch( g_NormalAttribute.Attribute[i].loop)
            {
                case 1:
                {
                    if (g_SystemState.heFenState1 != OPEN_STATE)
                    {
                        return HEFEN_STATE_ERROR;
                    }

                    break;
                }
                case 2:
                {
                    if (g_SystemState.heFenState2 != OPEN_STATE)
                    {
                        return HEFEN_STATE_ERROR;
                    }
                    break;
                }
#if(CAP3_STATE)
                case 3:
                {
                    if (g_SystemState.heFenState3 != OPEN_STATE)
                    {
                        return HEFEN_STATE_ERROR;
                    }
                     break;
                }
#endif
                default:
                {
                    return  LOOP_ERROR;
                }

            }
        }
    }
    if (g_NormalAttribute.count != 0)
    {
        return 0;
    }
    else
    {
         return  LOOP_ERROR;
    }
    
}

/**
 * 检查分闸条件
 * @return 错误代码
 */
static uint8_t CheckOpenCondition(void)
{
    
    for(uint8_t i = 0; i< LOOP_COUNT; i++)
    {
        if ( g_NormalAttribute.Attribute[i].enable)
        {
            switch( g_NormalAttribute.Attribute[i].loop)
            {
                case 1:
                {
                    if (g_SystemState.heFenState1 != CLOSE_STATE)
                    {
                        return HEFEN_STATE_ERROR;
                    }

                    break;
                }
                case 2:
                {
                    if (g_SystemState.heFenState2 != CLOSE_STATE)
                    {
                        return HEFEN_STATE_ERROR;
                    }
                    break;
                }
#if(CAP3_STATE)
                case 3:
                {
                    if (g_SystemState.heFenState3 != CLOSE_STATE)
                    {
                        return HEFEN_STATE_ERROR;
                    }
                     break;
                }
#endif
                default:
                {
                    return  LOOP_ERROR;
                }
            }
                            
        }
    }
    if (g_NormalAttribute.count != 0)
    {
        return 0;
    }
    else
    {
         return  LOOP_ERROR;
    }
    
}


/**
 * 
 * <p>Function name: [CheckOrder]</p>
 * <p>Discription: [检测命令是否正确执行]</p>
 * @param lastOrder 上一次执行的命令
 */
void CheckOrder(void)
{
    uint8_t i = 0;
    uint8_t closeOrderCount = 0;
    uint8_t openOrderCount = 0;
    
    for(i = 0; i < LOOP_COUNT; i++)
    {
        if(g_SwitchConfig[i].lastOrder == HE_ORDER)
        {
            if(g_SuddenState.switchState[i] != CLOSE_STATE)
            {
                closeOrderCount++;
            }
        }
        else if(g_SwitchConfig[i].lastOrder == FEN_ORDER)
        {
            if(g_SuddenState.switchState[i] != OPEN_STATE)
            {
                openOrderCount++;
            }
        }
    }
    ActionCommandTemporaryAck.complteFlag = TRUE;
    SendData(&ActionCommandTemporaryAck); //发送执行反馈指令
    if(closeOrderCount > 0)
    {
        g_SuddenState.RefuseAction = HE_ERROR;  //发生拒动
    }
    else if(openOrderCount > 0)
    {
        g_SuddenState.RefuseAction = FEN_ERROR;  //发生拒动
    }
    else
    {
        g_SuddenState.RefuseAction = FALSE;  //未发生拒动
    }
    
}

/**
 * 
 * <p>Function name: [SendMonitorParameter]</p>
 * <p>Discription: [发送参数]</p>
 */
static void SendMonitorParameter(struct DefFrameData* pReciveFrame)
{
    uint16_t idIndex = 0;
    idIndex = pReciveFrame->pBuffer[1];
    uint8_t error = 0;
    uint8_t i = 0;
    struct DefFrameData pSendFrame;   //要发送的数据
    uint8_t data[8] = {0};
    pSendFrame.pBuffer = data;
    pSendFrame.ID = MAKE_GROUP1_ID(GROUP1_POLL_STATUS_CYCLER_ACK, DeviceNetObj.MACID);
    pSendFrame.complteFlag = 0xFF;
    
    uint8_t start = pReciveFrame->pBuffer[1];
    uint8_t end = pReciveFrame->pBuffer[2];
    
    pSendFrame.pBuffer[0] = 0x92;
    for(idIndex = start; idIndex <= end; idIndex++)    //抛除ID号所占的长度
    {
        ClrWdt();
        g_ParameterBuffer.len = 8;
        error = ReadParamValue(idIndex,&g_ParameterBuffer);
        if((error == 0xF1)||(error == 0xF3))    //数据长度错误
        {
            ClrWdt();
            SendErrorFrame(pSendFrame.pBuffer[0], DATA_LEN_ERROR);
        }
        pSendFrame.pBuffer[1] = idIndex;  //配置号  
        pSendFrame.len = g_ParameterBuffer.len + 2;
        for(i = 0;i < pSendFrame.len;i++)
        {
            pSendFrame.pBuffer[i + 2] = g_ParameterBuffer.pData[i];
        }
        ClrWdt();
        SendData(&pSendFrame);
        if(idIndex == ID_GET_TEMP) //获取温度
        {
            g_TimeStampCollect.getTempTime.delayTime = 0x00;  //退出发送后立即执行一次获取温度
        }
    }
}
static uint8_t ConfigModeOperation(struct DefFrameData* pReciveFrame, struct DefFrameData* pSendFrame)
{   
    ClrWdt();
    if (pReciveFrame->len == 4) //ID+配置号 至少2字节
    {
        if (pReciveFrame->pBuffer[1] != DeviceNetObj.MACID)
        {
            return ERROR_LOCAL_MAC;
        }
        if (pReciveFrame->pBuffer[2] != DeviceNetObj.assign_info.master_MACID)
        {
            return ERROR_MASTER_MAC;
        }
        if (pReciveFrame->pBuffer[3] == EXIT_CONFIG)//离开配置模式
        {
            g_SystemState.congfigMode = pReciveFrame->pBuffer[3];
             //应答回复
            pSendFrame->pBuffer[0] = ConfigMode| 0x80;
            pSendFrame->pBuffer[1] = DeviceNetObj.MACID;
            pSendFrame->pBuffer[2] = DeviceNetObj.assign_info.master_MACID;
            pSendFrame->pBuffer[3] = EXIT_CONFIG;
            pSendFrame->pBuffer[4] = 0;
            pSendFrame->len = 5;
            return 0;

        }
        else if (pReciveFrame->pBuffer[3] == ENTER_CONFIG)//进入配置模式
        {
            g_SystemState.congfigMode = pReciveFrame->pBuffer[3];
            //应答回复
             pSendFrame->pBuffer[0] = ConfigMode| 0x80;
            memcpy( pSendFrame->pBuffer + 1, pReciveFrame->pBuffer + 1,
                 pReciveFrame->len - 1);
             pSendFrame->len = pReciveFrame->len;           
            return 0;
        }
   
    
    }
    
    return 0xff;
}

/**
 * <p>Function name: [_INT2Interrupt]</p>
 * <p>Discription: [外部中断函数]</p>
 */

void __attribute__((interrupt, no_auto_psv)) _INT2Interrupt(void)
{
    uint8_t i = 0;
    uint8_t state = 0x00;
    ClrWdt();
    EffectiveLevelSignalCount = 0;   
    
    IFS1bits.INT2IF = 0;
    //*************************************************************************************
    //以下判断防止误触发
    if(g_RemoteControlState.receiveStateFlag != TONGBU_HEZHA)   //判断是否执行了同步合闸预制
    {
        ON_COMMUNICATION_INT();
        TurnOffInt2();
        return;
    }
    if(g_RemoteControlState.overTimeFlag == FALSE) //发生超时
    {
        ON_COMMUNICATION_INT();
        TurnOffInt2();
        return;
    }
	//检测高电平
	for(i = 0; i < 4; i++)
	{
		state = ~state;
		state &= 0x01;
		while(RXD1_LASER == state)
		{
			EffectiveLevelSignalCount++;
            if(EffectiveLevelSignalCount >= 0xFFFF)    //防止进入死循环//TODO:设定最大值
            {
                return;
            }
		}
		if((EffectiveLevelSignalCount < 30) || (EffectiveLevelSignalCount > 50))  //较宽的范围85~125
		{
			return;
		}
        EffectiveLevelSignalCount = 0;
	}
	if((RXD1_LASER == 1) && (i == 4))
	{
        TurnOffInt2();
        g_RemoteControlState.overTimeFlag = FALSE;  //Clear Overtime Flag   
        SynCloseAction();
	}
}
/**
 * <p>Function name: [_INT3Interrupt]</p>
 * <p>Discription: [检测外部脉冲时序]</p>
 */

//void __attribute__((interrupt, no_auto_psv)) _INT3Interrupt(void)
//{
//    uint16_t usCount = 0;
//  
//    
//    IFS2bits.INT3IF = 0;   
//    StartTimer4();
//    while(RXD2_LASER == 1)
//    {        
//        ClrWdt();
//        if( IFS1bits.T4IF == 1)    //超出设定最大值返回
//        {   
//            StopTimer4();
//            return;
//        }
//    }
//    StopTimer4();
//    usCount = GetTimeUs() + 8;//补偿进入误差
//    //检查脉冲宽度是否在10us偏差之间
//    if(((g_SystemState.sequencePulseWidth - 20) <= usCount ) && ( usCount <= (g_SystemState.sequencePulseWidth + 20)))  //较宽的范围
//    {
//       uint16_t id = MAKE_GROUP2_ID(GROUP2_POLL_STATUS_CYCLE, SYNC_MAC);
//       uint8_t sendData[2] = {0};
//       sendData[0] = SynTimeSequence;
//       sendData[1] =  TIME_SEQUENCE;      
//       CANSendData(id, sendData, 2);
//    }
// 
//}
