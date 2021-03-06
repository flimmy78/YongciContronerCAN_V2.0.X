/** 
 * <p>application name： yongci.c</p> 
 * <p>application describing： 永磁控制器分合闸主流程</p> 
 * <p>copyright： Copyright (c) 2017 Beijing SOJO Electric CO., LTD.</p> 
 * <p>company： SOJO</p> 
 * <p>time： 2017.05.20</p> 
 * 
 * @updata:[日期2017-05-20] [ZhangXiaomou][更改分合闸计时方式，不再采用中断计时，增加三个机构的控制]
 * @author Zhangxiaomou 
 * @version ver 1.0
 */

#include "../Header.h"
#include "yongci.h"


#define REFUSE_ACTION   200     //拒动错误检测间隔时间（ms）
const uint8_t CompensationTime = 4;    //合分闸时间补偿（ms）
SwitchConfig g_SwitchConfig[LOOP_COUNT];	//配置机构状态

uint8_t ParameterBufferData[8] = {0,0,0,0,0,0,0,0};
//uint32_t _PERSISTENT g_TimeStampCollect.changeLedTime.delayTime;   //改变LED灯闪烁时间 (ms)
static uint16_t _PERSISTENT LockUp;  //命令上锁，在执行了一次合分闸命令之后应处于上锁状态，在延时800ms之后才可以第二次执行
uint16_t _PERSISTENT g_Order;    //需要执行的命令，且在单片机发生复位后不会改变
CAN_msg ReciveMsg;


void InitSetswitchState(void);
void SwitchOpenFirstPhase(SwitchConfig* pConfig);
void SwitchOpenSecondPhase(SwitchConfig* pConfig);
void SwitchOpenThirdPhase(SwitchConfig* pConfig);
void SwitchCloseFirstPhase(SwitchConfig* pConfig);
void SwitchCloseSecondPhase(SwitchConfig* pConfig);
void SwitchCloseThirdPhase(SwitchConfig* pConfig);





/**
 * @brief 用于同步永磁定时器控制
 */
void __attribute__((interrupt, no_auto_psv)) _T3Interrupt(void)
{
    IFS0bits.T3IF = 0;
    ClrWdt();
    uint8_t index = g_SynActionAttribute.currentIndex;
   
    uint8_t loop =  g_SynActionAttribute.Attribute[index].loop - 1;
    if((g_SwitchConfig[loop].currentState == READY_STATE) &&
            (g_SwitchConfig[loop].order == IDLE_ORDER))
	{
		g_SwitchConfig[loop].currentState = RUN_STATE;
        g_SwitchConfig[loop].order = HE_ORDER;
		g_SwitchConfig[loop].systemTime = g_TimeStampCollect.msTicks;
        g_SwitchConfig[loop].SwitchClose(g_SwitchConfig + loop);        
      
        index = ++ g_SynActionAttribute.currentIndex;
        if(index < g_SynActionAttribute.count)
        {
            ChangeTimerPeriod3(g_SynActionAttribute.Attribute[index].offsetTime);//偏移时间          
        } 
        else
        {
            StopTimer3();
        }		
	}
    
}
/**
 * 
 * <p>Function name: [OnLock]</p>
 * <p>Discription: [上锁]</p>
 */
inline void OnLock(void)
{
    LockUp = ON_LOCK;    //上锁
    LockUp = ON_LOCK;    //上锁
}

/**
 * 
 * <p>Function name: [OffLock]</p>
 * <p>Discription: [解锁状态]</p>
 */
inline void OffLock(void)
{
    LockUp = OFF_LOCK;    //解锁
    LockUp = OFF_LOCK;    //解锁
}

/**
 * <p>Function name: [CheckLockState]</p>
 * <p>Discription: [检测上锁状态]</p>
 * @return 处于解锁则返回TRUE 否则返回FALSE
 */
inline uint8_t CheckLockState(void)
{
    if(LockUp == OFF_LOCK)
    {
        return TRUE;
    }
    else if(LockUp == ON_LOCK)
    {
        return FALSE;
    }
    return  FALSE;//TODO: FALSE

}


/**
 * 同步合闸执行
 */
void SynCloseAction(void)
{
    uint8_t i = 0;
    if(CheckLockState())    //解锁状态下不能进行合闸
    {
        g_RemoteControlState.receiveStateFlag = IDLE_ORDER;
        return;
    }
    for(i = 0; i < LOOP_COUNT; i++)
    {
        //不允许多次执行相同/不同的操作
        if(g_SwitchConfig[i].currentState || g_SwitchConfig[i].order || g_SwitchConfig[i].lastOrder)
        {
            g_Order = IDLE_ORDER;    //将命令清零
            return;
        }
    }
    
    OFF_COMMUNICATION_INT();  //关闭通信中断
	uint8_t loop = 0;
    g_SwitchConfig[0].powerOnTime = g_DelayTime.hezhaTime1 + CompensationTime;   //使用示波器发现时间少3ms左右
    g_SwitchConfig[1].powerOnTime = g_DelayTime.hezhaTime2 + CompensationTime;   //使用示波器发现时间少3ms左右
#if(CAP3_STATE)
    g_SwitchConfig[2].powerOnTime = g_DelayTime.hezhaTime3 + CompensationTime;   //使用示波器发现时间少3ms左右
#endif
    g_SynActionAttribute.currentIndex = 0;
    for(uint8_t i = 1; i < g_SynActionAttribute.count; i++)
    {
        loop =  g_SynActionAttribute.Attribute[i].loop - 1;
        g_SwitchConfig[loop].currentState = READY_STATE;
        g_SwitchConfig[loop].order = IDLE_ORDER;
        g_SwitchConfig[loop].offestTime =    g_SynActionAttribute.Attribute[i].offsetTime;
    }
    loop =  g_SynActionAttribute.Attribute[0].loop -1;
	g_SwitchConfig[loop].currentState = RUN_STATE;
    g_SwitchConfig[loop].order = HE_ORDER;
	g_SwitchConfig[loop].systemTime = g_TimeStampCollect.msTicks;
	g_SwitchConfig[loop].SwitchClose(g_SwitchConfig + loop);
    
    //判断执行的路数
    uint8_t nextIndex = ++ g_SynActionAttribute.currentIndex;
    if(nextIndex < g_SynActionAttribute.count)
    {     
       StartTimer3(g_SynActionAttribute.Attribute[nextIndex].offsetTime);
    }
    ClrWdt();
}




/**
 *合闸操作
 */
void CloseOperation(void)
{
    if(CheckLockState())    //解锁状态下不能进行合闸
    {
        g_RemoteControlState.receiveStateFlag = IDLE_ORDER;
        return;
    }
    //不允许多次执行相同/不同的操作   
    ClrWdt();
    uint8_t index = 0;
    //g_NormalAttribute
    for(uint8_t i = 0; i < g_NormalAttribute.count; i++)
    {
        if(g_SwitchConfig[index].currentState || g_SwitchConfig[index].order || g_SwitchConfig[index].lastOrder)
        {
            g_Order = IDLE_ORDER;    //将命令清零
            return ;
        }
    }
    OFF_COMMUNICATION_INT();  //关闭通信中断
    g_SwitchConfig[0].powerOnTime = g_DelayTime.hezhaTime1 + CompensationTime;   //使用示波器发现时间少3ms左右
    g_SwitchConfig[1].powerOnTime = g_DelayTime.hezhaTime2 + CompensationTime;   //使用示波器发现时间少3ms左右
#if(CAP3_STATE)
    g_SwitchConfig[2].powerOnTime = g_DelayTime.hezhaTime3 + CompensationTime;   //使用示波器发现时间少3ms左右
#endif
    for(uint8_t i = 0; i < LOOP_COUNT; i++)
    {
        if (g_NormalAttribute.Attribute[i].enable)
        {
            index = g_NormalAttribute.Attribute[i].loop - 1;
            g_SwitchConfig[index].currentState = RUN_STATE;
            g_SwitchConfig[index].order = HE_ORDER;
            g_SwitchConfig[index].systemTime = g_TimeStampCollect.msTicks;
            ClrWdt();
            g_SwitchConfig[index].SwitchClose(g_SwitchConfig + index);
            g_SwitchConfig[index].systemTime = g_TimeStampCollect.msTicks;

            g_NormalAttribute.Attribute[i].enable = 0;
        }        
    }
    g_NormalAttribute.count = 0;
}
/**
 *分闸操作
 */
void OpenOperation(void)
{
    if(CheckLockState())    //解锁状态下不能进行合闸
    {
        g_RemoteControlState.receiveStateFlag = IDLE_ORDER;
        return;
    }
    //不允许多次执行相同/不同的操作   
    ClrWdt();
    uint8_t index = 0;
    //g_NormalAttribute
    for(uint8_t i = 0; i < g_NormalAttribute.count; i++)
    {
        if(g_SwitchConfig[index].currentState || g_SwitchConfig[index].order || g_SwitchConfig[index].lastOrder)
        {
            g_Order = IDLE_ORDER;    //将命令清零
            return ;
        }   
    }
    OFF_COMMUNICATION_INT();  //关闭通信中断
    g_SwitchConfig[0].powerOffTime = g_DelayTime.fenzhaTime1 + CompensationTime;   //使用示波器发现时间少3ms左右
    g_SwitchConfig[1].powerOffTime = g_DelayTime.fenzhaTime2 + CompensationTime;   //使用示波器发现时间少3ms左右
#if(CAP3_STATE)
    g_SwitchConfig[2].powerOffTime = g_DelayTime.fenzhaTime3 + CompensationTime;   //使用示波器发现时间少3ms左右
#endif
    for(uint8_t i = 0; i < LOOP_COUNT; i++)
    {
        if (g_NormalAttribute.Attribute[i].enable)
        {
               index = g_NormalAttribute.Attribute[i].loop - 1;
               g_SwitchConfig[index].currentState = RUN_STATE;
               g_SwitchConfig[index].order = FEN_ORDER;
               g_SwitchConfig[index].systemTime = g_TimeStampCollect.msTicks;
               ClrWdt();
               g_SwitchConfig[index].SwitchOpen(g_SwitchConfig + index);
               g_SwitchConfig[index].systemTime = g_TimeStampCollect.msTicks;

               g_NormalAttribute.Attribute[i].enable = 0;
        }        
    }
    g_NormalAttribute.count = 0;
}

/**
 * 
 * <p>Function name: [SingleCloseOperation]</p>
 * <p>Discription: [对机构执行合闸操作，对外提供接口，方便引用]</p>
 * @param index 执行机构的索引号
 * @param time IGBT导通时间
 */
void SingleCloseOperation(uint8_t index,uint16_t time)
{
    if(CheckLockState())    //解锁状态下不能进行合闸
    {
        g_RemoteControlState.receiveStateFlag = IDLE_ORDER;
        return;
    }
    //不允许多次执行相同/不同的操作
    if(g_SwitchConfig[index].currentState || g_SwitchConfig[index].order || g_SwitchConfig[index].lastOrder)
    {
        g_Order = IDLE_ORDER;    //将命令清零
        return ;
    }
    OFF_COMMUNICATION_INT();  //关闭通信中断
    ClrWdt();
    g_SwitchConfig[index].currentState = RUN_STATE;
    g_SwitchConfig[index].order = HE_ORDER;
    g_SwitchConfig[index].systemTime = g_TimeStampCollect.msTicks;
    g_SwitchConfig[index].powerOnTime = time + CompensationTime;   //使用示波器发现时间少3ms左右
    ClrWdt();
    g_SwitchConfig[index].SwitchClose(g_SwitchConfig + index);
    g_SwitchConfig[index].systemTime = g_TimeStampCollect.msTicks;
}

/**
 * 
 * <p>Function name: [SingleOpenOperation]</p>
 * <p>Discription: [对机构执行合闸操作，对外提供接口，方便引用]</p>
 * @param index 执行机构的索引号
 * @param time IGBT导通时间
 */
void SingleOpenOperation(uint8_t index,uint16_t time)
{
    if(CheckLockState())    //解锁状态下不能进行分闸
    {
        g_RemoteControlState.receiveStateFlag = IDLE_ORDER;
        return;
    }
    //不允许多次执行相同/不同的操作
    if(g_SwitchConfig[index].currentState || g_SwitchConfig[index].order || g_SwitchConfig[index].lastOrder)
    {
        g_Order = IDLE_ORDER;    //将命令清零
        return ;
    }
    OFF_COMMUNICATION_INT();  //关闭通信中断
    ClrWdt();
    g_SwitchConfig[index].currentState = RUN_STATE;
    g_SwitchConfig[index].order = FEN_ORDER;
    g_SwitchConfig[index].systemTime = g_TimeStampCollect.msTicks;
    g_SwitchConfig[index].powerOffTime = time + CompensationTime;   //使用示波器发现分闸时间少2ms左右
    ClrWdt();
    g_SwitchConfig[index].SwitchOpen(g_SwitchConfig + index);
    g_SwitchConfig[index].systemTime = g_TimeStampCollect.msTicks;
}

/**
 * 
 * <p>Function name: [ RefreshActionState]</p>
 * <p>Discription: [刷新合分闸工作状态]</p>
 * <p>return :[非0 -- 有正在工作的合分闸动作状态]
 *            0--空闲状态
 *    </p>
 */
uint8_t RefreshActionState()
{
    uint8_t state = 0;
    ClrWdt();
    //机构1合闸、分闸刷新
    if((g_SwitchConfig[DEVICE_I].order == HE_ORDER) && (g_SwitchConfig[DEVICE_I].currentState == RUN_STATE))
    {
        ClrWdt();
        OFF_COMMUNICATION_INT();  //刷新关闭通信中断
        g_SwitchConfig[DEVICE_I].SwitchClose(g_SwitchConfig);
        state |= 0x81;
    }
    else if((g_SwitchConfig[DEVICE_I].order == FEN_ORDER) && (g_SwitchConfig[DEVICE_I].currentState == RUN_STATE))
    {
        ClrWdt();
        OFF_COMMUNICATION_INT();  //刷新关闭通信中断
        g_SwitchConfig[DEVICE_I].SwitchOpen(g_SwitchConfig);
        state |= 0x82;
    }
    else    //防止持续合闸或者分闸
    {
//        __delay_us(100);
//        g_SwitchConfig[DEVICE_I].order = IDLE_ORDER;
//        RESET_CURRENT_A();
    }
        
    //机构2合闸、分闸刷新
    if((g_SwitchConfig[DEVICE_II].order == HE_ORDER) && (g_SwitchConfig[DEVICE_II].currentState == RUN_STATE))
    {
        ClrWdt();
        OFF_COMMUNICATION_INT();  //刷新关闭通信中断
        g_SwitchConfig[DEVICE_II].SwitchClose(g_SwitchConfig + 1);
        state |= 0x84;
    }
    else if((g_SwitchConfig[DEVICE_II].order == FEN_ORDER) && (g_SwitchConfig[DEVICE_II].currentState == RUN_STATE))
    {
        ClrWdt();
        OFF_COMMUNICATION_INT();  //刷新关闭通信中断
        g_SwitchConfig[DEVICE_II].SwitchOpen(g_SwitchConfig + 1);
        state |= 0x88;
    }
    else    //防止持续合闸或者分闸
    {
//            __delay_us(100);
//            g_SwitchConfig[DEVICE_II].order = IDLE_ORDER;
//            RESET_CURRENT_B();
    }

#if(CAP3_STATE)
        //机构3合闸、分闸刷新
        if((g_SwitchConfig[DEVICE_III].order == HE_ORDER) && (g_SwitchConfig[DEVICE_III].currentState == RUN_STATE))
        {
            ClrWdt();
            OFF_COMMUNICATION_INT();  //刷新关闭通信中断
            g_SwitchConfig[DEVICE_III].SwitchClose(g_SwitchConfig + 2);
            state |= 0x10;
        }
        else if((g_SwitchConfig[DEVICE_III].order == FEN_ORDER) && (g_SwitchConfig[DEVICE_III].currentState == RUN_STATE))
        {
            ClrWdt();
            g_SwitchConfig[DEVICE_III].SwitchOpen(g_SwitchConfig + 2);
            OFF_COMMUNICATION_INT();  //刷新关闭通信中断
             state |= 0x20;
        }
        else    //防止持续合闸或者分闸
        {
//                __delay_us(100);
//                g_SwitchConfig[DEVICE_III].order = IDLE_ORDER;
//                RESET_CURRENT_C();
        }
#endif

        //就绪状态则继续等待 TODO：就绪超时
#if(CAP3_STATE)
        if((g_SwitchConfig[DEVICE_I].currentState == READY_STATE)
            || (g_SwitchConfig[DEVICE_II].currentState == READY_STATE)
            ||(g_SwitchConfig[DEVICE_III].currentState == READY_STATE))
        {
            state |= 0x40;
        }
       
        //再次检测是否处于运行状态，若处于则继续执行。
        if((g_SwitchConfig[DEVICE_I].currentState == RUN_STATE)
            || (g_SwitchConfig[DEVICE_II].currentState == RUN_STATE)
            ||(g_SwitchConfig[DEVICE_III].currentState == RUN_STATE))
        {
            state |= 0x80;
        }
#else
        if((g_SwitchConfig[DEVICE_I].currentState == READY_STATE)
            || (g_SwitchConfig[DEVICE_II].currentState == READY_STATE))
        {
            state |= 0x40;
        }
       
        //再次检测是否处于运行状态，若处于则继续执行。
        if((g_SwitchConfig[DEVICE_I].currentState == RUN_STATE)
            || (g_SwitchConfig[DEVICE_II].currentState == RUN_STATE))
        {
            state |= 0x80;
        }
#endif
    
        return state;
}
/**
 * 
 * <p>Function name: [RefreshIdleState]</p>
 * <p>Discription: [刷新空闲的工作状态]</p>
 * <p>return : 非0--跳出
 *              0--正常执行
 *    </p>
 */
static uint8_t rxErrorCount = 0;
static uint8_t txErrorCount = 0;

uint8_t RefreshIdleState()
{
    static uint8_t runLedState = TURN_ON;
    static uint8_t runLedCount = 0;
    uint8_t result = 0;
    
    rxErrorCount = C2EC & 0x00FF;
    txErrorCount = (C2EC & 0xFF00) >> 8;
    
   //任意一项不是空闲状态
    if(!((g_SwitchConfig[DEVICE_I].order == IDLE_ORDER) &&
        (g_SwitchConfig[DEVICE_II].order == IDLE_ORDER) && 
        CHECK_ORDER3()))
    {
        return 0xF1;
    }

    ClrWdt();
    if((g_SwitchConfig[DEVICE_I].alreadyAction == TRUE) || 
       (g_SwitchConfig[DEVICE_II].alreadyAction == TRUE) || 
        CHECK_LAST_ORDER3())
    {
        ClrWdt();
        ReadCapDropVoltage();  //读取电容跌落电压
        //Clear Flag
        g_SwitchConfig[DEVICE_I].order = IDLE_ORDER; //清零
        g_SwitchConfig[DEVICE_I].currentState = IDLE_ORDER;
        g_SwitchConfig[DEVICE_I].alreadyAction = FALSE;
        g_SwitchConfig[DEVICE_II].order = IDLE_ORDER;
        g_SwitchConfig[DEVICE_II].currentState = IDLE_ORDER;
        g_SwitchConfig[DEVICE_II].alreadyAction = FALSE;
#if(CAP3_STATE)
        g_SwitchConfig[DEVICE_III].order = IDLE_ORDER;
        g_SwitchConfig[DEVICE_III].alreadyAction = FALSE;
        g_SwitchConfig[DEVICE_III].currentState = IDLE_ORDER;
#endif
        g_RemoteControlState.receiveStateFlag = IDLE_ORDER;
        //此处开启计时功能，延时大约在200ms内判断是否正确执行功能，不是的话返回错误
        g_TimeStampCollect.refusalActionTime.delayTime = REFUSE_ACTION;
        g_TimeStampCollect.refusalActionTime.startTime = g_TimeStampCollect.msTicks;
    }
    
    //检测是否欠电压， 并更新显示
    if(IsOverTimeStamp(&g_TimeStampCollect.getCapVolueTime)) //大约每300ms获取一次电容电压值
    {
        UpdataCapVoltageState();  //获取电容电压
        ClrWdt();
        g_TimeStampCollect.getCapVolueTime.startTime = g_TimeStampCollect.msTicks;           
    }  

    if(IsOverTimeStamp(&g_TimeStampCollect.scanTime)) //大约每2ms扫描一次
    {
        ClrWdt();
        SwitchScan();   //执行按键扫描程序 TODO:用时时长 764cyc or 615cyc
        g_TimeStampCollect.scanTime.startTime = g_TimeStampCollect.msTicks;  
    }
    if (CheckIOState()) //收到合分闸指令，退出后立即进行循环
    {
        g_Order = IDLE_ORDER;    //将命令清零
        return 0xff;
    }

    //超时检测复位
    if(g_RemoteControlState.overTimeFlag && g_RemoteControlState.receiveStateFlag)  //判断是否需要超时检测
    {
        if(IsOverTimeStamp( &g_TimeStampCollect.overTime))
        {
            ON_COMMUNICATION_INT();
            OffLock();  //解锁
            SendErrorFrame(g_RemoteControlState.orderId , OVER_TIME_ERROR);
            ClrWdt();
            g_RemoteControlState.orderId = 0;   //Clear
            g_RemoteControlState.receiveStateFlag = IDLE_ORDER; //Clear order
            g_RemoteControlState.overTimeFlag = FALSE;  //Clear Flag
            if(g_RemoteControlState.orderId == SyncReadyClose)  //同步合闸预制
            {
                ON_COMMUNICATION_INT();
                TurnOffInt2();
            }
        }
    }
    
    //始终进行处理，处理完缓冲区。 TODO:远方本地检测?
    do
    {
        result = BufferDequeue(&ReciveMsg);
        if (result)
        {
            DeviceNetReciveCenter(&ReciveMsg.id, ReciveMsg.data, ReciveMsg.len);
        }
        if(g_RemoteControlState.receiveStateFlag)
        {
            ClrWdt();
            return 0xFF;
        }
    }
    while(result);

    StopTimer3();  //刷新关闭定时器3

    //拒动错误检测
    if(IsOverTimeStamp( &g_TimeStampCollect.refusalActionTime)) //TODO：错误是
    {
        CheckOrder();  //检测命令是否执行
        if(g_SuddenState.RefuseAction != FALSE)
        {
            g_TimeStampCollect.changeLedTime.delayTime = 200;  //发生拒动错误后，指示灯闪烁间隔变短
            SendErrorFrame(g_RemoteControlState.orderId , REFUSE_ERROR);
        }
        else
        {
            g_TimeStampCollect.changeLedTime.delayTime = 500;  //正常状态下指示灯闪烁的间隔
            UpdateCount();//没有拒动才会更新计数
        }
        g_SuddenState.suddenFlag = TRUE;  //发送突发错误
        g_SwitchConfig[DEVICE_I].lastOrder = IDLE_ORDER;
        g_SwitchConfig[DEVICE_II].lastOrder = IDLE_ORDER;
#if(CAP3_STATE)
        g_SwitchConfig[DEVICE_III].lastOrder = IDLE_ORDER;
#endif
        g_TimeStampCollect.refusalActionTime.delayTime = UINT32_MAX;    //延时时间达到最大
        OffLock();  //解锁
        //在判断完拒动后才会开启中断
        ON_COMMUNICATION_INT();
    }
    ClrWdt();
    
#if(CAP3_STATE)
    if((g_SwitchConfig[DEVICE_I].lastOrder) || (g_SwitchConfig[DEVICE_II].lastOrder) || (g_SwitchConfig[DEVICE_III].lastOrder))
    {
        return 0;     
    }
#else
    if((g_SwitchConfig[DEVICE_I].lastOrder) || (g_SwitchConfig[DEVICE_II].lastOrder))
    {
        return 0;
    }
#endif
    
    //空闲状态，状态刷新  
    //在判断完拒动后才会开启中断,一个合闸、分闸周期完成
    
    ON_COMMUNICATION_INT();
    g_TimeStampCollect.refusalActionTime.startTime = g_TimeStampCollect.msTicks;
    
    
    //周期性状态更新
    if((IsOverTimeStamp( &g_TimeStampCollect.sendDataTime)) || g_SuddenState.suddenFlag)
    {
        ClrWdt();
        DsplaySwitchState();    //更新机构的状态显示
        //建立连接且不是在预制状态下才会周期上传
        if((StatusChangedConnedctionObj.state == STATE_LINKED) && (g_RemoteControlState.receiveStateFlag == IDLE_ORDER))   
        {
            UpdataState();  //更新状态
        }
        g_SuddenState.suddenFlag = FALSE;  //Clear
        OffLock();  //解锁
        g_TimeStampCollect.sendDataTime.startTime = g_TimeStampCollect.msTicks;  
        g_TimeStampCollect.sendDataTime.delayTime = g_SystemState.updatePeriod * 1000;//更新周期
    }

    //DeviceNet超时离线检测
    if(IsOverTimeStamp( &g_TimeStampCollect.offlineTime))
    {
        BufferInit();     
        InitStandardCAN(0, 0);      //初始化CAN模块
        ClrWdt();
        InitDeviceNet();            //初始化DeviceNet服务
        g_TimeStampCollect.offlineTime.startTime = g_TimeStampCollect.msTicks;
    }
    ClrWdt();


    //运行指示灯
    if(IsOverTimeStamp( &g_TimeStampCollect.changeLedTime))
    {
        UpdateLEDIndicateState(RUN_LED, runLedState);
        runLedState = ~runLedState;
        if(g_RemoteControlState.CanErrorFlag || C2INTFbits.TXBO)
        {
            runLedCount++;
            if(runLedCount >= 200)
            {
                InitStandardCAN(0, 0);  //初始化CAN模块
                ON_COMMUNICATION_INT();
                g_TimeStampCollect.changeLedTime.delayTime = 500;  //运行指示灯闪烁间隔为500ms
                g_RemoteControlState.CanErrorFlag = FALSE;  //Clear
                runLedCount = 0;
            }
            //TODO:什么时候恢复？
        }
        g_TimeStampCollect.changeLedTime.startTime = g_TimeStampCollect.msTicks;  
    }

     ClrWdt();
    //获取温度数据
    if(IsOverTimeStamp( &g_TimeStampCollect.getTempTime))
    {                   
        g_SystemVoltageParameter.temp = DS18B20GetTemperature();    //获取温度值            
        g_TimeStampCollect.getTempTime.startTime = g_TimeStampCollect.msTicks;         
        g_TimeStampCollect.getTempTime.delayTime = GET_TEMP_TIME;  
    }

    if(g_RemoteControlState.setFixedValue == TRUE)
    {
        WriteAccumulateSum();  //写入累加和
        g_RemoteControlState.setFixedValue = FALSE;
    }
    if(C2INTFbits.RX0OVR)
    {
        C2INTFbits.RX0OVR = 0;  //清除接收缓冲器0溢出中断
        C2INTFbits.RX0IF = 0;
        C2INTEbits.RXB0IE = 1;
       
    }

    return 0;
        
        
}
/**************************************************
 *函数名：YongciMainTask()
 *功能: 永磁控制主循环 
 *形参：  void
 *返回值：void
****************************************************/
void YongciMainTask(void)
{
    uint8_t result = 0;
 
    while(0xFFFF) //主循环
    {        
        result = RefreshActionState();  //刷新合分闸工作状态
        //有正在执行的合分闸动作，则继续刷新状态。
        if (result)
        {
            continue;
        }
        //均没有处于运行状态：TODO:添加预制状态处理，防止遗漏时间错开的情况
        RESET_CURRENT_A();
        RESET_CURRENT_B();
        RESET_CURRENT_C();
        result =  RefreshIdleState();      
        //检测到按钮动作 或者有信号输入
        if (result)
        {
            continue;
        }
    }
}

/**************************************************
 *函数名：YongciFirstInit()
 *功能:初始化IGBT控制端口，合分闸状态检测端口
 *形参：  void
 *返回值：void
****************************************************/
void YongciFirstInit(void)
{
    ClrWdt();
    
    g_ParameterBuffer.pData = ParameterBufferData;
    g_ParameterBuffer.len = 8;
    
    LockUp = OFF_LOCK;    //处于解锁状态
    
    //IGBT引脚
    //A
    DRIVER_A_1_DIR = 0;
    DRIVER_B_1_DIR = 0;
    DRIVER_C_1_DIR = 0;
    DRIVER_D_1_DIR = 0;
    RESET_CURRENT_A();
    ClrWdt();
    
    //IGBT引脚
    //B
    DRIVER_A_2_DIR = 0;
    DRIVER_B_2_DIR = 0;
    DRIVER_C_2_DIR = 0;
    DRIVER_D_2_DIR = 0;
    RESET_CURRENT_B();
    ClrWdt();
    
    //IGBT引脚
    //B
    DRIVER_A_3_DIR = 0;
    DRIVER_B_3_DIR = 0;
    DRIVER_C_3_DIR = 0;
    DRIVER_D_3_DIR = 0;
    RESET_CURRENT_C();
    ClrWdt();
    
    g_Order = IDLE_ORDER;   //初始化
    ClrWdt();
    InitSetswitchState();
    
    g_TimeStampCollect.getTempTime.startTime = g_TimeStampCollect.msTicks;  
    g_TimeStampCollect.sendDataTime.startTime = g_TimeStampCollect.msTicks;   
    g_TimeStampCollect.scanTime.startTime = g_TimeStampCollect.msTicks;   
    g_TimeStampCollect.changeLedTime.startTime = g_TimeStampCollect.msTicks;    
    g_TimeStampCollect.getCapVolueTime.startTime = g_TimeStampCollect.msTicks;  
    
    g_TimeStampCollect.changeLedTime.delayTime = 500;    //初始化值为500ms
    
    //远方控制标识位初始化
    g_RemoteControlState.receiveStateFlag = IDLE_ORDER;
    g_RemoteControlState.overTimeFlag = FALSE;
    g_RemoteControlState.orderId = 0x00;    //Clear
    g_RemoteControlState.setFixedValue = FALSE;    //Clear    
    g_RemoteControlState.GetAllValueFalg = FALSE;    //Clear
    g_RemoteControlState.GetOneValueFalg = FALSE;    //Clear
    g_RemoteControlState.CanErrorFlag = FALSE;    //Clear
    
    //****************************************
    //突发状态量更新初始化
    ClrWdt();
    g_SuddenState.suddenFlag = FALSE;
    g_SuddenState.RefuseAction = FALSE;
    
    g_SuddenState.capSuddentFlag = FALSE;
    g_SuddenState.switchsuddenFlag = FALSE;
    
    g_SuddenState.buffer[0] = 0;
    g_SuddenState.buffer[1] = 0;
    //****************************************
    
    OffLock();  //解锁，可以检测
}  

/**
 * 
 * <p>Function name: [InitSetswitchState]</p>
 * <p>Discription: [机构状态的初始化]</p>
 */
void InitSetswitchState(void)
{
	g_SwitchConfig[DEVICE_I].currentState = IDLE_ORDER;	//默认为空闲状态
	g_SwitchConfig[DEVICE_I].alreadyAction = FALSE;	//默认未动作
	g_SwitchConfig[DEVICE_I].order = IDLE_ORDER; //默认未执行
    g_SwitchConfig[DEVICE_I].lastOrder = IDLE_ORDER; //默认上一次未执行任何指令
	g_SwitchConfig[DEVICE_I].powerOnTime = HEZHA_TIME;  //默认合闸时间50ms
	g_SwitchConfig[DEVICE_I].powerOffTime = FENZHA_TIME;    //默认合闸时间50ms
	g_SwitchConfig[DEVICE_I].offestTime = 0; //默认无偏移时间
	g_SwitchConfig[DEVICE_I].systemTime = 0;    //默认系统时间为零
	g_SwitchConfig[DEVICE_I].SwitchClose = SwitchCloseFirstPhase;
	g_SwitchConfig[DEVICE_I].SwitchOpen = SwitchOpenFirstPhase;
    ClrWdt();

	g_SwitchConfig[DEVICE_II].currentState = IDLE_ORDER;	//默认为空闲状态
	g_SwitchConfig[DEVICE_II].alreadyAction = FALSE;	//默认未动作
	g_SwitchConfig[DEVICE_II].order = IDLE_ORDER; //默认未执行
    g_SwitchConfig[DEVICE_II].lastOrder = IDLE_ORDER; //默认上一次未执行任何指令
	g_SwitchConfig[DEVICE_II].powerOnTime = HEZHA_TIME;  //默认合闸时间50ms
	g_SwitchConfig[DEVICE_II].powerOffTime = FENZHA_TIME;    //默认分闸时间30ms
	g_SwitchConfig[DEVICE_II].offestTime = 0; //默认无偏移时间
	g_SwitchConfig[DEVICE_II].systemTime = 0;    //默认系统时间为零
	g_SwitchConfig[DEVICE_II].SwitchClose = SwitchCloseSecondPhase;
	g_SwitchConfig[DEVICE_II].SwitchOpen = SwitchOpenSecondPhase;
    ClrWdt();
#if(CAP3_STATE)
	g_SwitchConfig[DEVICE_III].currentState = IDLE_ORDER;	//默认为空闲状态
	g_SwitchConfig[DEVICE_III].alreadyAction = FALSE;	//默认未动作
	g_SwitchConfig[DEVICE_III].order = IDLE_ORDER; //默认未执行
    g_SwitchConfig[DEVICE_III].lastOrder = IDLE_ORDER; //默认上一次未执行任何指令
	g_SwitchConfig[DEVICE_III].powerOnTime = HEZHA_TIME;  //默认合闸时间50ms
	g_SwitchConfig[DEVICE_III].powerOffTime = FENZHA_TIME;    //默认分闸时间30ms
	g_SwitchConfig[DEVICE_III].offestTime = 0; //默认无偏移时间
	g_SwitchConfig[DEVICE_III].systemTime = 0;    //默认系统时间为零
	g_SwitchConfig[DEVICE_III].SwitchClose = SwitchCloseThirdPhase;
	g_SwitchConfig[DEVICE_III].SwitchOpen = SwitchOpenThirdPhase;
#endif
    ClrWdt();
    
}


//各个机构的合闸函数
//*************************************

/**
 * 
 * <p>Function name: [SwitchCloseFirstPhase]</p>
 * <p>Discription: [第一相合闸]</p>
 * @param pConfig 执行该函数功能的指针
 */
void SwitchCloseFirstPhase(SwitchConfig* pConfig)
{
	if((pConfig->order == HE_ORDER) && (pConfig->currentState == RUN_STATE))	//首先判断是否是合闸命令,且是否是可以执行状态
	{
		HEZHA_A();
		HEZHA_A();
	}
	if(IsOverTime(pConfig->systemTime, pConfig->powerOnTime))	//超时时间到则复位
	{
		RESET_CURRENT_A();
		RESET_CURRENT_A();
		pConfig->order = IDLE_ORDER;
		pConfig->currentState = IDLE_ORDER;
        ClrWdt();
        pConfig->lastOrder = HE_ORDER; //刚执行完合闸指令
        pConfig->alreadyAction = TRUE; //已经动作了
	}
}

/**
 * 
 * <p>Function name: [SwitchCloseSecondPhase]</p>
 * <p>Discription: [第二相合闸]</p>
 * @param pConfig 执行该函数功能的指针
 */
void SwitchCloseSecondPhase(SwitchConfig* pConfig)
{
	if((pConfig->order == HE_ORDER) && (pConfig->currentState == RUN_STATE))	//首先判断是否是合闸命令,且是否是可以执行状态
	{
		HEZHA_B();
		HEZHA_B();
	}
	if(IsOverTime(pConfig->systemTime, pConfig->powerOnTime))	//超时时间到则复位
	{
		RESET_CURRENT_B();
		RESET_CURRENT_B();
		pConfig->order = IDLE_ORDER;
		pConfig->currentState = IDLE_ORDER;
        ClrWdt();
        pConfig->lastOrder = HE_ORDER; //刚执行完合闸指令
        pConfig->alreadyAction = TRUE; //已经动作了
	}
}

/**
 * 
 * <p>Function name: [SwitchCloseThirdPhase]</p>
 * <p>Discription: [第三相合闸]</p>
 * @param pConfig 执行该函数功能的指针
 */
void SwitchCloseThirdPhase(SwitchConfig* pConfig)
{
	if((pConfig->order == HE_ORDER) && (pConfig->currentState == RUN_STATE))	//首先判断是否是合闸命令,且是否是可以执行状态
	{
		HEZHA_C();
		HEZHA_C();
	}
	if(IsOverTime(pConfig->systemTime,pConfig->powerOnTime))	//超时时间到则复位
	{
		RESET_CURRENT_C();
		RESET_CURRENT_C();
		pConfig->order = IDLE_ORDER;
		pConfig->currentState = IDLE_ORDER;
        ClrWdt();
        pConfig->lastOrder = HE_ORDER; //刚执行完合闸指令
        pConfig->alreadyAction = TRUE; //已经动作了
	}
}


//*************************************

//各个机构的分闸函数
//*************************************
/**
 * 
 * <p>Function name: [SwitchOpenFirstPhase]</p>
 * <p>Discription: [第一相分闸]</p>
 * @param pConfig 执行该函数功能的指针
 */
void SwitchOpenFirstPhase(SwitchConfig* pConfig)
{
	if((pConfig->order == FEN_ORDER) && (pConfig->currentState == RUN_STATE))	//首先判断是否是分闸命令
	{
		FENZHA_A();
		FENZHA_A();
	}
	if(IsOverTime(pConfig->systemTime , pConfig->powerOffTime))	//超时时间到则复位
	{
		RESET_CURRENT_A();
		RESET_CURRENT_A();
		pConfig->order = IDLE_ORDER;
		pConfig->currentState = IDLE_ORDER;
        ClrWdt();
        pConfig->lastOrder = FEN_ORDER; //刚执行完分闸指令
        pConfig->alreadyAction = TRUE; //已经动作了
	}
}

/**
 * 
 * <p>Function name: [SwitchOpenSecondPhase]</p>
 * <p>Discription: [第二相分闸]</p>
 * @param pConfig 执行该函数功能的指针
 */
void SwitchOpenSecondPhase(SwitchConfig* pConfig)
{
	if((pConfig->order == FEN_ORDER) && (pConfig->currentState == RUN_STATE))	//首先判断是否是分闸命令
	{
		FENZHA_B();
		FENZHA_B();
	}
	if(IsOverTime(pConfig->systemTime, pConfig->powerOffTime))	//超时时间到则复位
	{
		RESET_CURRENT_B();
		RESET_CURRENT_B();
		pConfig->order = IDLE_ORDER;
		pConfig->currentState = IDLE_ORDER;
        ClrWdt();
        pConfig->lastOrder = FEN_ORDER; //刚执行完分闸指令
        pConfig->alreadyAction = TRUE; //已经动作了
	}
}

/**
 * 
 * <p>Function name: [SwitchOpenThirdPhase]</p>
 * <p>Discription: [第三相分闸]</p>
 * @param pConfig 执行该函数功能的指针
 */
void SwitchOpenThirdPhase(SwitchConfig* pConfig)
{
    if((pConfig->order == FEN_ORDER) && (pConfig->currentState == RUN_STATE))	//首先判断是否是分闸命令
	{
		FENZHA_C();
		FENZHA_C();
	}
	if(IsOverTime(pConfig->systemTime,pConfig->powerOffTime))	//超时时间到则复位
	{
		RESET_CURRENT_C();
		RESET_CURRENT_C();
		pConfig->order = IDLE_ORDER;
		pConfig->currentState = IDLE_ORDER;
        ClrWdt();
        pConfig->lastOrder = FEN_ORDER; //刚执行完分闸指令
        pConfig->alreadyAction = TRUE; //已经动作了
	}
}

//*************************************

/**************************************************
 *函数名： UpdateCount()
 *功能: 根据合闸与分闸更新EEPROM存在的计数
 *形参：  uint16 state--合闸或分闸命令
 *返回值：void
****************************************************/
void UpdateCount(void)
{
    //应禁止中断
    if (g_SwitchConfig[DEVICE_I].lastOrder == HE_ORDER) //机构1合闸
    {
        ClrWdt();
        SaveActionCount(JG1_HE_COUNT_ADDRESS , &g_ActionCount.hezhaCount1);
        g_SwitchConfig[DEVICE_I].lastOrder = IDLE_ORDER;
    }
    else if (g_SwitchConfig[DEVICE_I].lastOrder == FEN_ORDER)  //机构1分闸
    {
        ClrWdt();
        SaveActionCount(JG1_FEN_COUNT_ADDRESS , &g_ActionCount.fenzhaCount1);
        g_SwitchConfig[DEVICE_I].lastOrder = IDLE_ORDER;
    }
    
    if (g_SwitchConfig[DEVICE_II].lastOrder == HE_ORDER)   //机构2合闸
    {
        ClrWdt();
        SaveActionCount(JG2_HE_COUNT_ADDRESS , &g_ActionCount.hezhaCount2);
        g_SwitchConfig[DEVICE_II].lastOrder = IDLE_ORDER;
    }
    else if (g_SwitchConfig[DEVICE_II].lastOrder == FEN_ORDER)  //机构2分闸
    {
        ClrWdt();
        SaveActionCount(JG2_FEN_COUNT_ADDRESS , &g_ActionCount.fenzhaCount2);
        g_SwitchConfig[DEVICE_II].lastOrder = IDLE_ORDER;
    }
    
#if(CAP3_STATE)
    if (g_SwitchConfig[DEVICE_III].lastOrder == HE_ORDER)   //机构3合闸
    {
        ClrWdt();
        SaveActionCount(JG3_HE_COUNT_ADDRESS , &g_ActionCount.hezhaCount3);
        g_SwitchConfig[DEVICE_III].lastOrder = IDLE_ORDER;
    }
    else if (g_SwitchConfig[DEVICE_III].lastOrder == FEN_ORDER)  //机构3分闸
    {
        ClrWdt();
        SaveActionCount(JG3_FEN_COUNT_ADDRESS , &g_ActionCount.fenzhaCount3);
        g_SwitchConfig[DEVICE_III].lastOrder = IDLE_ORDER;
    }
#endif    
}



