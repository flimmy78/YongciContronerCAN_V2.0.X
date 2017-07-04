/** 
 * <p>application name： SwtichCondition.c</p> 
 * <p>application describing： 开入量值获取</p> 
 * <p>copyright： Copyright (c) 2017 Beijing SOJO Electric CO., LTD.</p> 
 * <p>company： SOJO</p> 
 * <p>time： 2017.05.20</p> 

 * 摘要:
 * 2015/8/10:完善远本转换。
 * 2015/11/17: 改变IO检测模式，由纯延时形式，变成采样点样式。
 * 将远方检测IO与本地合并。

 * @updata:[2017-05-20] [张东旭][更改IO读取方式，采用的是并口转串口，165采样。扫描改为定时器扫描]
 * @author ZhangXiaomou 
 * @version ver 1.0
 */
#include "SwtichCondition.h"
#include "../Header.h"
#include "DeviceParameter.h"

uint32_t volatile g_kairuValue = 0;    //165返回值

#define DELAY_MS    20

#define STATE_HE_ZHA_Z  0x5535  //检测同时合闸信号
#define STATE_FEN_ZHA_Z 0x553A  //检测同时分闸信号

#define STATE_HE_ZHA_1  0x5505  //检测机构1合闸信号
#define STATE_FEN_ZHA_1 0x550A  //检测机构1分闸信号

#define STATE_HE_ZHA_2  0x5515  //检测机构2合闸信号
#define STATE_FEN_ZHA_2 0x551A  //检测机构2分闸信号

#define STATE_HE_ZHA_3  0x5525  //检测机构3合闸信号
#define STATE_FEN_ZHA_3 0x552A  //检测机构3分闸信号

/**
 * <p>Discription: [检测机构的状态，处于合位还是分位]</p>
 */
#define STATE_HE_WEI_1  0x2205  //机构1合位检测
#define STATE_FEN_WEI_1 0x220A  //机构1分位检测

#define STATE_HE_WEI_2  0x2215  //机构2合位检测
#define STATE_FEN_WEI_2 0x221A  //机构2分位检测

#define STATE_HE_WEI_3  0x2225  //机构3合位检测
#define STATE_FEN_WEI_3 0x222A  //机构3分位检测

/**
 * <p>Discription: [检测机构出现错误的命令]</p>
 */
#define STATE_ERROR_1   0xAB00  //机构1错误
#define STATE_ERROR_2   0xAB11  //机构2错误
#define STATE_ERROR_3   0xAB22  //机构3错误

#define STATE_ERROR_4   0xAB33  //机构1错误，同时输入了合闸、分闸信号
#define STATE_ERROR_5   0xAB44  //机构2错误，同时输入了合闸、分闸信号
#define STATE_ERROR_6   0xAB55  //机构3错误，同时输入了合闸、分闸信号

#define STATE_ERROR_7   0xAB66  //同时输入了总的合闸、分闸信号

#define STATE_FEN_CONTINOUS        0xAA55

#define COIL1_HEZHA()   (HZHA1_INPUT | FENWEI1_INPUT)   //机构1合闸条件
#define COIL1_FENZHA()  (FZHA1_INPUT | HEWEI1_INPUT)    //机构1分闸条件

#define COIL2_HEZHA()   (HZHA2_INPUT | FENWEI2_INPUT)   //机构2合闸条件
#define COIL2_FENZHA()  (FZHA2_INPUT | HEWEI2_INPUT)    //机构2分闸条件

#define COIL3_HEZHA()   (HZHA3_INPUT | FENWEI3_INPUT)   //机构3合闸条件
#define COIL3_FENZHA()  (FZHA3_INPUT | HEWEI3_INPUT)    //机构3分闸条件

//远方\本地控制检测
#define YUAN_BEN_CONDITION()    ((g_kairuValue & YUAN_INPUT) == YUAN_INPUT)

//工作\调试模式检测
#define WORK_DEBUG_CONDITION()  ((g_kairuValue & WORK_INPUT) == WORK_INPUT)

#ifdef SMALL_CHOSE
//本地的总合闸条件：总分位 && 总合闸信号 && 本地控制 && 工作模式 && 电压满足 && 合闸信号未输入
#define Z_HEZHA_CONDITION()     ((g_kairuValue)== (Z_HEZHA_INPUT | FENWEI1_INPUT | FENWEI2_INPUT | FENWEI3_INPUT | WORK_INPUT))

//本地的总分闸条件：总合位 && 总分闸信号 && 本地控制 && 工作模式 && 不带电 && 电压满足 && 分闸信号未输入
#define Z_FENZHA_CONDITION()    \
(g_kairuValue == (Z_FENZHA_INPUT | HEWEI1_INPUT | HEWEI2_INPUT | HEWEI3_INPUT | WORK_INPUT) && (g_kairuValue & DIANXIAN_INPUT) == 0)

#elif	BIG_CHOSE
//本地的总合闸条件：总分位 && 总合闸信号 && 本地控制 && 工作模式 && 电压满足 && 合闸信号未输入
#define Z_HEZHA_CONDITION()     (g_kairuValue == (Z_HEZHA_INPUT | FENWEI1_INPUT | FENWEI2_INPUT | WORK_INPUT))

//本地的总分闸条件：总合位 && 总分闸信号 && 本地控制 && 工作模式 && 不带电 && 电压满足 && 分闸信号未输入
#define Z_FENZHA_CONDITION()    \
    (g_kairuValue == (Z_FENZHA_INPUT | HEWEI1_INPUT | HEWEI2_INPUT | WORK_INPUT) && (g_kairuValue & DIANXIAN_INPUT) == 0)
#endif


//本地的机构1合闸条件：分位1 && 合闸1信号 && 电压1满足 && 本地控制 && 调试模式 && 合闸信号未输入
#define HEZHA1_CONDITION()      ((g_kairuValue & (COIL1_HEZHA() | WORK_INPUT)) == COIL1_HEZHA() &&  \
                                 (g_SystemVoltageParameter.voltageCap1  >= g_SystemLimit.capVoltage1.down))
//本地的机构1分闸条件：合位1 && 分闸1信号 && 电压1满足 && 本地控制 && 调试模式 && 不带电 && 分闸信号未输入
#define FENZHA1_CONDITION()    \
((g_kairuValue & (COIL1_FENZHA() | WORK_INPUT)) == COIL1_FENZHA() && (g_kairuValue & DIANXIAN_INPUT) == 0 &&    \
 (g_SystemVoltageParameter.voltageCap1  >= g_SystemLimit.capVoltage1.down))

//本地的机构2合闸条件：分位2 && 合闸2信号 && 电压2满足 && 本地控制 && 调试模式 && 合闸信号未输入
#define HEZHA2_CONDITION()      ((g_kairuValue & (COIL2_HEZHA() | WORK_INPUT)) == COIL2_HEZHA() &&  \
                                 (g_SystemVoltageParameter.voltageCap2  >= g_SystemLimit.capVoltage2.down))
//本地的机构2分闸条件：合位2 && 分闸2信号 && 电压2满足 && 本地控制 && 调试模式 && 不带电 && 分闸信号未输入
#define FENZHA2_CONDITION()    \
((g_kairuValue & (COIL2_FENZHA() | WORK_INPUT)) == COIL2_FENZHA() && (g_kairuValue & DIANXIAN_INPUT) == 0 &&    \
 (g_SystemVoltageParameter.voltageCap2  >= g_SystemLimit.capVoltage2.down))

//本地的机构3合闸条件：分位2 && 合闸3信号 && 电压2满足 && 本地控制 && 调试模式 && 合闸信号未输入
#define HEZHA3_CONDITION()      ((g_kairuValue & (COIL3_HEZHA() | WORK_INPUT)) == COIL3_HEZHA() &&  \
                                 (g_SystemVoltageParameter.voltageCap3  >= g_SystemLimit.capVoltage3.down))
//本地的机构3分闸条件：合位2 && 分闸3信号 && 电压2满足 && 本地控制 && 调试模式 && 不带电 && 分闸信号未输入
#define FENZHA3_CONDITION()    \
((g_kairuValue & (COIL3_FENZHA() | WORK_INPUT)) == COIL3_FENZHA() && (g_kairuValue & DIANXIAN_INPUT) == 0 &&    \
 (g_SystemVoltageParameter.voltageCap3  >= g_SystemLimit.capVoltage3.down))


/**
 * 
 * <p>Discription: [对机构的合分位检测]</p>
 */
//******************************************************************************
//机构1的合位检测
#define HEWEI1_CONDITION()  ((g_kairuValue & HEWEI1_INPUT) == HEWEI1_INPUT)
//机构1的分位检测
#define FENWEI1_CONDITION()  ((g_kairuValue & FENWEI1_INPUT) == FENWEI1_INPUT)

//机构2的合位检测
#define HEWEI2_CONDITION()  ((g_kairuValue & HEWEI2_INPUT) == HEWEI2_INPUT)
//机构2的分位检测
#define FENWEI2_CONDITION()  ((g_kairuValue & FENWEI2_INPUT) == FENWEI2_INPUT)

//机构3的合位检测
#define HEWEI3_CONDITION()  ((g_kairuValue & HEWEI3_INPUT) == HEWEI3_INPUT)
//机构3的分位检测
#define FENWEI3_CONDITION()  ((g_kairuValue & FENWEI3_INPUT) == FENWEI3_INPUT)
//******************************************************************************


/**
 * <p>Discription: [一下错误均为不可屏蔽掉的错误，且错误严重]</p>
 */
//***************************************************************************************************
//机构1的本地错误条件: 合位和分位同时成立 或 同时不成立
#define ERROR1_CONDITION()      \
        (((g_kairuValue & (FENWEI1_INPUT | HEWEI1_INPUT)) == (FENWEI1_INPUT | HEWEI1_INPUT)) ||   \
        ((g_kairuValue & (FENWEI1_INPUT | HEWEI1_INPUT)) == 0))

//机构2的本地错误条件: 合位和分位同时成立 或 同时不成立
#define ERROR2_CONDITION()      \
        (((g_kairuValue & (FENWEI2_INPUT | HEWEI2_INPUT)) == (FENWEI2_INPUT | HEWEI2_INPUT)) ||   \
        ((g_kairuValue & (FENWEI2_INPUT | HEWEI2_INPUT)) == 0))

//机构3的本地错误条件: 合位和分位同时成立 或 同时不成立
#define ERROR3_CONDITION()      \
        ((g_kairuValue & (FENWEI3_INPUT | HEWEI3_INPUT)) == (FENWEI3_INPUT | HEWEI3_INPUT) ||   \
        ((g_kairuValue & (FENWEI3_INPUT | HEWEI3_INPUT)) == 0))
//***************************************************************************************************


#define KEY_COUNT_DOWN 30   // *2ms
uint8_t g_timeCount[22] = {0};
uint8_t g_lockflag[22] = {0}; 


/**
 * 
 * <p>Function name: [CheckIOState]</p>
 * <p>Discription: [检测IO状态，并更新状态显示]</p>
 * @return 接收到分合闸命令返回0xFF,否则返回0
 */
uint8_t CheckIOState(void)
{
    ClrWdt();     
    switch (g_Order)
    {
        //检测按键状态
        case CHECK_Z_HE_ORDER: //收到合闸命令 需要判断一下电容电压能否达到要求
        {
            ClrWdt();
            if((g_SystemState.workMode == WORK_STATE) && (GetCapVolatageState()))
            {
                if(g_lockUp == OFF_LOCK)    //解锁状态下不能进行合闸
                {
                    g_Order = IDLE_ORDER;    //将命令清零
                    return 0xFF;
                }
               // TongBuHeZha(); TODO:
                ClrWdt();
                HEZHA_Action(SWITCH_ONE , g_DelayTime.hezhaTime1);
                HEZHA_Action(SWITCH_TWO , g_DelayTime.hezhaTime2);
                #if(CAP3_STATE)  //判断第三块驱动是否存在
                {
                    HEZHA_Action(SWITCH_THREE , g_DelayTime.hezhaTime3);
                }
                #endif
                g_RemoteControlState.orderId = 0x05;    //拒动错误ID号
                ClrWdt();
                g_SuddenState.ExecuteOrder1 = 0x01;
                g_SuddenState.ExecuteOrder2 = 0x04;
                g_SuddenState.ExecuteOrder3 = 0x10;
                return 0xFF;
            }
            else
            {
                g_Order = IDLE_ORDER;    //将命令清零
                return 0xFF;
            }
        }
        case CHECK_Z_FEN_ORDER: //收到分闸命令
        {
            ClrWdt();
            if(g_lockUp == OFF_LOCK)    //解锁状态下不能进行合闸
            {
                g_Order = IDLE_ORDER;    //将命令清零
                return 0xFF;
            }
            if((g_SystemState.workMode == WORK_STATE) && (GetCapVolatageState())) //多加入一重验证
            {
                ClrWdt();
                FENZHA_Action(SWITCH_ONE , g_DelayTime.fenzhaTime1);
                FENZHA_Action(SWITCH_TWO , g_DelayTime.fenzhaTime2);
                #if(CAP3_STATE)  //判断第三块驱动是否存在
                {
                    FENZHA_Action(SWITCH_THREE , g_DelayTime.fenzhaTime3);
                }
                #endif
                g_RemoteControlState.orderId = 0x04;    //拒动错误ID号
                ClrWdt();
                g_SuddenState.ExecuteOrder1 = 0x02;
                g_SuddenState.ExecuteOrder2 = 0x08;
                g_SuddenState.ExecuteOrder3 = 0x20;
                return 0xFF;
            }
            else
            {
                g_Order = IDLE_ORDER;    //将命令清零
                return 0xFF;
            }
        }
        
        case CHECK_1_HE_ORDER: //收到机构1合闸命令
        {
            ClrWdt();
            if(g_SystemVoltageParameter.voltageCap1  >= g_SystemLimit.capVoltage1.down)
            {
                HEZHA_Action(SWITCH_ONE , g_DelayTime.hezhaTime1);
                g_SuddenState.ExecuteOrder1 = 0x02;
                g_RemoteControlState.orderId = 0x02;    //拒动错误ID号
            }
            else
            {
                g_Order = IDLE_ORDER;    //将命令清零
                return 0xFF;
            }
            return 0xFF;
        }
        case CHECK_1_FEN_ORDER: //收到机构1分闸命令
        {
            ClrWdt();
            if(g_SystemVoltageParameter.voltageCap1  >= g_SystemLimit.capVoltage1.down)
            {
                FENZHA_Action(SWITCH_ONE , g_DelayTime.fenzhaTime1);
                g_SuddenState.ExecuteOrder1 = 0x01;
                g_RemoteControlState.orderId = 0x04;    //拒动错误ID号
            }
            else
            {
                g_Order = IDLE_ORDER;    //将命令清零
                return 0xFF;
            }
            return 0xFF;
        }
        
        case CHECK_2_HE_ORDER: //收到机构2合闸命令
        {
            ClrWdt();        
            if(g_SystemVoltageParameter.voltageCap2  >= g_SystemLimit.capVoltage2.down)
            {
                HEZHA_Action(SWITCH_TWO , g_DelayTime.hezhaTime2);
                g_SuddenState.ExecuteOrder2 = 0x08;
                g_RemoteControlState.orderId = 0x02;    //拒动错误ID号
            }
            else
            {
                g_Order = IDLE_ORDER;    //将命令清零
                return 0xFF;
            }
            return 0xFF;
        }
        case CHECK_2_FEN_ORDER: //收到机构2分闸命令
        {
            ClrWdt();
            if(g_SystemVoltageParameter.voltageCap2  >= g_SystemLimit.capVoltage2.down)
            {
                FENZHA_Action(SWITCH_TWO , g_DelayTime.fenzhaTime2);
                g_SuddenState.ExecuteOrder2 = 0x04;
                g_RemoteControlState.orderId = 0x04;    //拒动错误ID号
            }
            else
            {
                g_Order = IDLE_ORDER;    //将命令清零
                return 0xFF;
            }
            return 0xFF;
        }
        
        case CHECK_3_HE_ORDER: //收到机构3合闸命令
        {
            ClrWdt();        
            if(CAP3_STATE && (g_SystemVoltageParameter.voltageCap3  >= g_SystemLimit.capVoltage3.down))  //判断第三块驱动是否存在
            {
                HEZHA_Action(SWITCH_THREE , g_DelayTime.hezhaTime3);
                g_SuddenState.ExecuteOrder3 = 0x20;
                g_RemoteControlState.orderId = 0x02;    //拒动错误ID号
            }       
            else
            {
                g_Order = IDLE_ORDER;    //将命令清零
                return 0xFF;
            }     
            return 0xFF;
        }
        case CHECK_3_FEN_ORDER: //收到机构3分闸命令
        {
            ClrWdt();
            if(CAP3_STATE && (g_SystemVoltageParameter.voltageCap3  >= g_SystemLimit.capVoltage3.down))  //判断第三块驱动是否存在
            {
                FENZHA_Action(SWITCH_THREE , g_DelayTime.fenzhaTime3);
                g_SuddenState.ExecuteOrder3 = 0x10;
                g_RemoteControlState.orderId = 0x04;    //拒动错误ID号
            }       
            else
            {
                g_Order = IDLE_ORDER;    //将命令清零
                return 0xFF;
            }     
            return 0xFF;
        }
        default:
        {
            ClrWdt();
        }
    }
    return 0;
}
/**
 * 
 * <p>Function name: [CheckSwitchState]</p>
 * <p>Discription: [执行相应的指示]</p>
 */
void DsplaySwitchState(void)
{    
    ClrWdt();
    if(g_SuddenState.Cap1Error == CAP1_ERROR || 
       g_SystemState.heFenState1 == CHECK_ERROR1_STATE) //机构1错误
    {
        UpdateIndicateState(ERROR1_RELAY,ERROR1_LED,TURN_ON);
        if(g_SystemState.heFenState1 == CHECK_ERROR1_STATE) //进一步判断是分合位错误，还是电容电压错误
        {
            UpdateIndicateState(HEWEI1_RELAY,HEWEI1_LED,TURN_OFF);  //关闭合位指示灯
            UpdateIndicateState(FENWEI1_RELAY,FENWEI1_LED,TURN_OFF);    //关闭分位指示灯
        }
        ClrWdt();
        g_SystemState.warning = CHECK_ERROR1_STATE;
    }
    else 
    {
        if(g_SystemState.warning == CHECK_ERROR1_STATE)
        {
            g_SystemState.warning = NO_ERROR;
        }
        UpdateIndicateState(ERROR1_RELAY,ERROR1_LED,TURN_OFF);
    }
    
    if(g_SuddenState.Cap2Error == CAP2_ERROR || 
       g_SystemState.heFenState2 == CHECK_ERROR2_STATE) //机构2错误
    {
        UpdateIndicateState(ERROR2_RELAY,ERROR2_LED,TURN_ON);
        if(g_SystemState.heFenState2 == CHECK_ERROR2_STATE) //进一步判断是分合位错误，还是电容电压错误
        {
            UpdateIndicateState(HEWEI2_RELAY,HEWEI2_LED,TURN_OFF);  //关闭合位指示灯
            UpdateIndicateState(FENWEI2_RELAY,FENWEI2_LED,TURN_OFF);    //关闭分位指示灯
        }
        ClrWdt();
        g_SystemState.warning = CHECK_ERROR2_STATE;
    }
    else
    {
        if(g_SystemState.warning == CHECK_ERROR2_STATE)
        {
            g_SystemState.warning = NO_ERROR;
        }
        UpdateIndicateState(ERROR2_RELAY,ERROR2_LED,TURN_OFF);
    }
    
    #if(CAP3_STATE)
    {
        if(g_SuddenState.Cap3Error == CAP3_ERROR || 
           g_SystemState.heFenState3 == CHECK_ERROR3_STATE) //机构3错误
        {
            UpdateIndicateState(ERROR3_RELAY,ERROR3_LED,TURN_ON);
            if(g_SystemState.heFenState3 == CHECK_ERROR3_STATE) //进一步判断是分合位错误，还是电容电压错误
            {
                UpdateIndicateState(HEWEI3_RELAY,HEWEI3_LED,TURN_OFF);  //关闭合位指示灯
                UpdateIndicateState(FENWEI3_RELAY,FENWEI3_LED,TURN_OFF);    //关闭分位指示灯
            }
            ClrWdt();
            g_SystemState.warning = CHECK_ERROR3_STATE;
        }
        else
        {
            if(g_SystemState.warning == CHECK_ERROR3_STATE)
            {
                g_SystemState.warning = NO_ERROR;
            }
            UpdateIndicateState(ERROR3_RELAY,ERROR3_LED,TURN_OFF);
        }
    }
    #endif
    
    if((g_SystemState.warning == CHECK_ERROR1_STATE) ||
       (g_SystemState.warning == CHECK_ERROR2_STATE) ||
       (g_SystemState.warning == CHECK_ERROR3_STATE))
    {
        return ;
    }
    g_SystemState.warning = NO_ERROR;
    
    //以上既不是故障 也不是 合分闸命令情况下 检测合分状态，检测合分位时不考虑过多的因素
    //机构1的分合位指示
    ClrWdt();
    if(g_SystemState.heFenState1 == CHECK_1_FEN_STATE) //分闸状态
    {
        UpdateIndicateState(FENWEI1_RELAY,FENWEI1_LED,TURN_ON);
        UpdateIndicateState(HEWEI1_RELAY,HEWEI1_LED,TURN_OFF);
        ClrWdt();
        if(g_SuddenState.RefuseAction == JIGOU1_FEN_ERROR)
        {
            g_TimeStampCollect.changeLedTime.delayTime = 500;  //发生拒动错误后，指示灯闪烁间隔变短
            g_SuddenState.RefuseAction = FALSE; //Clear
        }
    }
    else if(g_SystemState.heFenState1 == CHECK_1_HE_STATE)  //合闸状态
    {
        UpdateIndicateState(HEWEI1_RELAY,HEWEI1_LED,TURN_ON);
        UpdateIndicateState(FENWEI1_RELAY,FENWEI1_LED,TURN_OFF);
        ClrWdt();
        if(g_SuddenState.RefuseAction == JIGOU1_HE_ERROR)
        {
            g_TimeStampCollect.changeLedTime.delayTime = 500;  //发生拒动错误后，指示灯闪烁间隔变短
            g_SuddenState.RefuseAction = FALSE; //Clear
        }
    }
    
    //机构2的分合位指示
    ClrWdt();
    if(g_SystemState.heFenState2 == CHECK_2_FEN_STATE) //分闸状态
    {
        UpdateIndicateState(FENWEI2_RELAY,FENWEI2_LED,TURN_ON);
        UpdateIndicateState(HEWEI2_RELAY,HEWEI2_LED,TURN_OFF);
        ClrWdt();
        if(g_SuddenState.RefuseAction == JIGOU2_FEN_ERROR)
        {
            g_TimeStampCollect.changeLedTime.delayTime = 500;  //发生拒动错误后，指示灯闪烁间隔变短
            g_SuddenState.RefuseAction = FALSE; //Clear
        }
        
    }    
    else if(g_SystemState.heFenState2 == CHECK_2_HE_STATE)   //合闸状态
    {
        UpdateIndicateState(HEWEI2_RELAY,HEWEI2_LED,TURN_ON);
        UpdateIndicateState(FENWEI2_RELAY,FENWEI2_LED,TURN_OFF);
        ClrWdt();
        if(g_SuddenState.RefuseAction == JIGOU2_HE_ERROR)
        {
            g_TimeStampCollect.changeLedTime.delayTime = 500;  //发生拒动错误后，指示灯闪烁间隔变短
            g_SuddenState.RefuseAction = FALSE; //Clear
        }
    }
    
    #if(CAP3_STATE)
    {
        //机构3的分合位指示    
        ClrWdt();
        if(g_SystemState.heFenState3 == CHECK_3_FEN_STATE) //分闸状态
        {
            UpdateIndicateState(FENWEI3_RELAY,FENWEI3_LED,TURN_ON);
            UpdateIndicateState(HEWEI3_RELAY,HEWEI3_LED,TURN_OFF);
            ClrWdt();
            if(g_SuddenState.RefuseAction == JIGOU3_FEN_ERROR)
            {
                g_TimeStampCollect.changeLedTime.delayTime = 500;  //发生拒动错误后，指示灯闪烁间隔变短
                g_SuddenState.RefuseAction = FALSE; //Clear
            }

        }    
        else if(g_SystemState.heFenState3 == CHECK_3_HE_STATE)   //合闸状态
        {
            UpdateIndicateState(HEWEI3_RELAY,HEWEI3_LED,TURN_ON);
            UpdateIndicateState(FENWEI3_RELAY,FENWEI3_LED,TURN_OFF);
            ClrWdt();
            if(g_SuddenState.RefuseAction == JIGOU3_HE_ERROR)
            {
                g_TimeStampCollect.changeLedTime.delayTime = 500;  //发生拒动错误后，指示灯闪烁间隔变短
                g_SuddenState.RefuseAction = FALSE; //Clear
            }
        }    
    }
    #else
    {        
        g_SystemState.heFenState3 = g_SystemState.heFenState2 + 2;  //主要应用在判断总分合位
    }
    #endif
    
    //总的分合位检测    
    //分闸状态
    ClrWdt();
    if((g_SystemState.heFenState2 == CHECK_2_FEN_STATE) && (g_SystemState.heFenState1 == CHECK_1_FEN_STATE) && 
       (g_SystemState.heFenState3 == CHECK_3_FEN_STATE))
    {
        //合位指示灯亮 
        UpdateIndicateState(Z_FENWEI_RELAY,Z_FENWEI_LED,TURN_ON);
        UpdateIndicateState(Z_HEWEI_RELAY,Z_HEWEI_LED,TURN_OFF);
        ClrWdt();
        if(g_SuddenState.RefuseAction == Z_HE_ERROR)
        {
            g_TimeStampCollect.changeLedTime.delayTime = 500;  //发生拒动错误后，指示灯闪烁间隔变短
            g_SuddenState.RefuseAction = FALSE; //Clear
        }
    }
    else if((g_SystemState.heFenState2 == CHECK_2_HE_STATE)&&(g_SystemState.heFenState1 == CHECK_1_HE_STATE) && 
       (g_SystemState.heFenState3 == CHECK_3_HE_STATE)) //合闸状态
    {
        //分位指示灯亮 
        UpdateIndicateState(Z_HEWEI_RELAY,Z_HEWEI_LED,TURN_ON);
        UpdateIndicateState(Z_FENWEI_RELAY,Z_FENWEI_LED,TURN_OFF);
        ClrWdt();
        if(g_SuddenState.RefuseAction == Z_FEN_ERROR)
        {
            g_TimeStampCollect.changeLedTime.delayTime = 500;  //发生拒动错误后，指示灯闪烁间隔变短
            g_SuddenState.RefuseAction = FALSE; //Clear
        }
    }
    else
    {
        UpdateIndicateState(Z_FENWEI_RELAY,Z_FENWEI_LED,TURN_OFF);
        UpdateIndicateState(Z_HEWEI_RELAY,Z_HEWEI_LED,TURN_OFF);
    }    
}
/**
 * 
 * <p>Function name: [SwitchScan]</p>
 * <p>Discription: [检测开入量]</p>
 */
void SwitchScan(void)
{
    g_kairuValue = ReHC74165();
    //远方\就地检测
    if(YUAN_BEN_CONDITION() && (g_lockflag[17] == 0))
    {
        ClrWdt();
        g_timeCount[17]++;
        if(g_timeCount[17] >= KEY_COUNT_DOWN)
        {            
            g_lockflag[17] = 1;
            g_timeCount[17] = 0;
            g_SystemState.yuanBenState = YUAN_STATE;
        }
        g_lockflag[18] = 0;
        g_timeCount[18] = 0;
    }
    else if(!YUAN_BEN_CONDITION() && (g_lockflag[18] == 0))
    {
        ClrWdt();
        g_timeCount[18]++;
        if(g_timeCount[18] >= KEY_COUNT_DOWN)
        {            
            g_lockflag[18] = 1;
            g_timeCount[18] = 0;
            g_SystemState.yuanBenState = BEN_STATE;
        }
        g_lockflag[17] = 0;
        g_timeCount[17] = 0;
    }
    //*****************************
    //作用，屏蔽掉远方就地
    uint32_t i = ~YUAN_INPUT;
    g_kairuValue &= i;
    //*****************************
    if(g_lockUp == OFF_LOCK)
    {
        ClrWdt();
        //同时合闸信号检测
        if(Z_HEZHA_CONDITION() && g_lockflag[0] == 0)   
        {
            ClrWdt();
            g_timeCount[0]++;
            if(g_timeCount[0] >= KEY_COUNT_DOWN)
            {
                ClrWdt();
                g_lockflag[0] = 1;
                g_timeCount[0] = 0;
                if((g_SystemState.heFenState1 == CHECK_1_FEN_STATE) || 
                   (g_SystemState.heFenState2 == CHECK_2_FEN_STATE) || 
                   (g_SystemState.heFenState3 == CHECK_3_FEN_STATE))
                {
                    g_Order = CHECK_Z_HE_ORDER;     //同时合闸命令
                    OnLock();   //上锁
                }
            }
        }
        else if(!Z_HEZHA_CONDITION())
        {
            ClrWdt();
            if(g_timeCount[0] != 0)
            {
                g_timeCount[0] -= 1;
            }
            g_lockflag[0] = 0;
        }

        //同时分闸信号检测
        if(Z_FENZHA_CONDITION() && g_lockflag[1] == 0)   
        {
            g_timeCount[1]++;
            ClrWdt();
            if(g_timeCount[1] >= KEY_COUNT_DOWN)
            {
                ClrWdt();
                g_lockflag[1] = 1;
                g_timeCount[1] = 0;
                if((g_SystemState.heFenState1 == CHECK_1_HE_STATE) || 
                   (g_SystemState.heFenState2 == CHECK_2_HE_STATE) || 
                   (g_SystemState.heFenState3 == CHECK_3_HE_STATE) )
                {
                    g_Order = CHECK_Z_FEN_ORDER;     //同时分闸命令
                    OnLock();   //上锁
                }
            }
        }
        else if(!Z_FENZHA_CONDITION())
        {
            if(g_timeCount[1] != 0)
            {
                ClrWdt();
                g_timeCount[1] -= 1;
            }
            g_lockflag[1] = 0;
        }

        //机构1合闸信号检测
        if(HEZHA1_CONDITION() && g_lockflag[2] == 0)   
        {
            ClrWdt();
            g_timeCount[2]++;
            if(g_timeCount[2] >= KEY_COUNT_DOWN)
            {
                ClrWdt();
                g_lockflag[2] = 1;
                g_timeCount[2] = 0;
                if(g_SystemState.heFenState1 == CHECK_1_FEN_STATE)
                {
                    g_Order = CHECK_1_HE_ORDER;     //机构1合闸命令
                    OnLock();   //上锁
                }
            }
        }
        else if(!HEZHA1_CONDITION())
        {
            ClrWdt();
            if(g_timeCount[2] != 0)
            {
                ClrWdt();
                g_timeCount[2] -= 1;
            }
            g_lockflag[2] = 0;
        }

        //机构1分闸信号检测
        if(FENZHA1_CONDITION() && g_lockflag[3] == 0)   
        {
            g_timeCount[3]++;
            ClrWdt();
            if(g_timeCount[3] >= KEY_COUNT_DOWN)
            {
                ClrWdt();
                g_lockflag[3] = 1;
                g_timeCount[3] = 0;
                if(g_SystemState.heFenState1 == CHECK_1_HE_STATE)
                {
                    g_Order = CHECK_1_FEN_ORDER;     //机构1分闸命令
                    OnLock();   //上锁
                }
            }
        }
        else if(!FENZHA1_CONDITION())
        {
            ClrWdt();
            if(g_timeCount[3] != 0)
            {
                ClrWdt();
                g_timeCount[3] -= 1;
            }
            g_lockflag[3] = 0;
        }

        //机构2合闸信号检测
        if(HEZHA2_CONDITION() && g_lockflag[4] == 0)   
        {
            ClrWdt();
            g_timeCount[4]++;
            if(g_timeCount[4] >= KEY_COUNT_DOWN)
            {
                ClrWdt();
                g_lockflag[4] = 1;
                g_timeCount[4] = 0;
                if(g_SystemState.heFenState2 == CHECK_2_FEN_STATE)
                {
                    g_Order = CHECK_2_HE_ORDER;     //机构2合闸命令
                    OnLock();   //上锁
                }
            }
        }
        else if(!HEZHA2_CONDITION())
        {
            ClrWdt();
            if(g_timeCount[4] != 0)
            {
                ClrWdt();
                g_timeCount[4] -= 1;
            }
            g_lockflag[4] = 0;
        }

        //机构2分闸信号检测
        if(FENZHA2_CONDITION() && g_lockflag[5] == 0)   
        {
            g_timeCount[5]++;
            ClrWdt();
            if(g_timeCount[5] >= KEY_COUNT_DOWN)
            {
                g_lockflag[5] = 1;
                g_timeCount[5] = 0;
                if(g_SystemState.heFenState2 == CHECK_2_HE_STATE)
                {
                    g_Order = CHECK_2_FEN_ORDER;     //机构2分闸命令
                    OnLock();   //上锁
                }          
            }
        }
        else if(!FENZHA2_CONDITION())
        {
            ClrWdt();
            if(g_timeCount[5] != 0)
            {
                g_timeCount[5] -= 1;
            }
            g_lockflag[5] = 0;
        }
        
        #if(CAP3_STATE)  //判断第三块驱动是否存在
        {
            //机构3合闸信号检测
            if(HEZHA3_CONDITION() && g_lockflag[6] == 0)   
            {
                ClrWdt();
                g_timeCount[6]++;
                if(g_timeCount[6] >= KEY_COUNT_DOWN)
                {
                    ClrWdt();
                    g_lockflag[6] = 1;
                    g_timeCount[6] = 0;
                    if(g_SystemState.heFenState3 == CHECK_3_FEN_STATE)
                    {
                        g_Order = CHECK_3_HE_ORDER;     //机构3合闸命令
                        OnLock();   //上锁
                    }                
                }
            }
            else if(!HEZHA3_CONDITION())
            {
                ClrWdt();
                if(g_timeCount[6] != 0)
                {
                    ClrWdt();
                    g_timeCount[6] -= 1;
                }
                g_lockflag[6] = 0;
            }

            //机构3分闸信号检测
            if(FENZHA3_CONDITION() && g_lockflag[7] == 0)   
            {
                g_timeCount[7]++;
                ClrWdt();
                if(g_timeCount[7] >= KEY_COUNT_DOWN)
                {
                    g_lockflag[7] = 1;
                    g_timeCount[7] = 0;
                    if(g_SystemState.heFenState3 == CHECK_3_HE_STATE)
                    {
                        g_Order = CHECK_3_FEN_ORDER;     //机构3分闸命令
                        OnLock();   //上锁
                    }
                }
            }
            else if(!FENZHA3_CONDITION())
            {
                ClrWdt();
                if(g_timeCount[7] != 0)
                {
                    g_timeCount[7] -= 1;
                }
                g_lockflag[7] = 0;
            }
        }
        #endif
    }
    
    //机构1的合位检测
    if(HEWEI1_CONDITION() && g_lockflag[8] == 0)
    {
        ClrWdt();
        g_timeCount[8]++;
        if(g_timeCount[8] >= KEY_COUNT_DOWN)
        {
            g_lockflag[8] = 1;
            g_timeCount[8] = 0;
            g_SystemState.heFenState1 = CHECK_1_HE_STATE;  
            g_SuddenState.SwitchState1 = 0x02; //0b10
            g_SuddenState.SuddenFlag = TRUE;
        }
    }
    else if(!HEWEI1_CONDITION())
    {
        ClrWdt();
        if(g_timeCount[8] != 0)
        {
            g_timeCount[8] -= 1;
        }
        g_lockflag[8] = 0;
    }   
    
    //机构1的分位检测
    if(FENWEI1_CONDITION() && g_lockflag[9] == 0)
    {
        ClrWdt();
        g_timeCount[9]++;
        if(g_timeCount[9] >= KEY_COUNT_DOWN)
        {
            g_lockflag[9] = 1;
            g_timeCount[9] = 0;
            g_SystemState.heFenState1 = CHECK_1_FEN_STATE;  
            g_SuddenState.SwitchState1 = 0x01; //0b01
            g_SuddenState.SuddenFlag = TRUE;
        }
    }
    else if(!FENWEI1_CONDITION())
    {
        ClrWdt();
        if(g_timeCount[9] != 0)
        {
            g_timeCount[9] -= 1;
        }
        g_lockflag[9] = 0;
    }   
    
    //机构2的合位检测
    if(HEWEI2_CONDITION() && g_lockflag[10] == 0)
    {
        ClrWdt();
        g_timeCount[10]++;
        if(g_timeCount[10] >= KEY_COUNT_DOWN)
        {
            g_lockflag[10] = 1;
            g_timeCount[10] = 0;
            g_SystemState.heFenState2 = CHECK_2_HE_STATE;  
            g_SuddenState.SwitchState2 = 0x08; //0b010
            g_SuddenState.SuddenFlag = TRUE;
        }
    }
    else if(!HEWEI2_CONDITION())
    {
        ClrWdt();
        if(g_timeCount[10] != 0)
        {
            g_timeCount[10] -= 1;
        }
        g_lockflag[10] = 0;
    }   
    
    //机构2的分位检测
    if(FENWEI2_CONDITION() && g_lockflag[11] == 0)
    {
        ClrWdt();
        g_timeCount[11]++;
        if(g_timeCount[11] >= KEY_COUNT_DOWN)
        {
            g_lockflag[11] = 1;
            g_timeCount[11] = 0;
            g_SystemState.heFenState2 = CHECK_2_FEN_STATE;  
            g_SuddenState.SwitchState2 = 0x04; //0b001
            g_SuddenState.SuddenFlag = TRUE;
        }
    }
    else if(!FENWEI2_CONDITION())
    {
        ClrWdt();
        if(g_timeCount[11] != 0)
        {
            g_timeCount[11] -= 1;
        }
        g_lockflag[11] = 0;
    }   
    #if(CAP3_STATE)  //判断第三块驱动是否存在
    {
        //机构3的合位检测
        if(HEWEI3_CONDITION() && g_lockflag[12] == 0)
        {
            ClrWdt();
            g_timeCount[12]++;
            if(g_timeCount[12] >= KEY_COUNT_DOWN)
            {
                g_lockflag[12] = 1;
                g_timeCount[12] = 0;
                g_SystemState.heFenState3 = CHECK_3_HE_STATE;  
                g_SuddenState.SwitchState3 = 0x20; //0b10                
                g_SuddenState.SuddenFlag = TRUE;
            }
        }
        else if(!HEWEI3_CONDITION())
        {
            ClrWdt();
            if(g_timeCount[12] != 0)
            {
                g_timeCount[12] -= 1;
            }
            g_lockflag[12] = 0;
        }   

        //机构3的分位检测
        if(FENWEI3_CONDITION() && g_lockflag[13] == 0)
        {
            ClrWdt();
            g_timeCount[13]++;
            if(g_timeCount[13] >= KEY_COUNT_DOWN)
            {
                g_lockflag[13] = 1;
                g_timeCount[13] = 0;
                g_SystemState.heFenState3 = CHECK_3_FEN_STATE;      
                g_SuddenState.SwitchState3 = 0x10; //0b01
                g_SuddenState.SuddenFlag = TRUE;
            }
        }
        else if(!FENWEI3_CONDITION())
        {
            ClrWdt();
            if(g_timeCount[13] != 0)
            {
                g_timeCount[13] -= 1;
            }
            g_lockflag[13] = 0;
        }   
    }
    #endif
        
    
    //机构1的合分位同时存在\不存在
    if(ERROR1_CONDITION() && g_lockflag[14] == 0)
    {
        ClrWdt();
        g_timeCount[14]++;
        if(g_timeCount[14] >= KEY_COUNT_DOWN)
        {
            g_lockflag[14] = 1;
            g_timeCount[14] = 0;
            g_SystemState.heFenState1 = CHECK_ERROR1_STATE;     //合分位同时存在            
            g_SuddenState.SwitchState1 = 0x03;
            g_SuddenState.SuddenFlag = TRUE;
        }
    }
    else if(!ERROR1_CONDITION())
    {
        ClrWdt();
        if(g_timeCount[14] != 0)
        {
            g_timeCount[14] -= 1;
        }
        g_lockflag[14] = 0;
    }
    
    //机构2的合分位同时存在\不存在
    if(ERROR2_CONDITION() && g_lockflag[15] == 0)
    {
        ClrWdt();
        g_timeCount[15]++;
        if(g_timeCount[15] >= KEY_COUNT_DOWN)
        {
            g_lockflag[15] = 1;
            g_timeCount[15] = 0;
            g_SystemState.heFenState2 = CHECK_ERROR2_STATE;     //合分位同时存在
            g_SuddenState.SwitchState2 = 0x0C;
            g_SuddenState.SuddenFlag = TRUE;
        }
    }
    else if(!ERROR2_CONDITION())
    {
        ClrWdt();
        if(g_timeCount[15] != 0)
        {
            g_timeCount[15] -= 1;
        }
        g_lockflag[15] = 0;
    }
    
    #if(CAP3_STATE)
    {
        //机构3的合分位同时存在\不存在
        if(ERROR3_CONDITION() && g_lockflag[16] == 0)
        {
            ClrWdt();
            g_timeCount[16]++;
            if(g_timeCount[16] >= KEY_COUNT_DOWN)
            {
                g_lockflag[16] = 1;
                g_timeCount[16] = 0;
                g_SystemState.heFenState3 = CHECK_ERROR3_STATE;     //合分位同时存在
                g_SuddenState.SwitchState3 = 0x30;
                g_SuddenState.SuddenFlag = TRUE;
            }
        }
        else if(!ERROR3_CONDITION())
        {
            ClrWdt();
            if(g_timeCount[16] != 0)
            {
                g_timeCount[16] -= 1;
            }
            g_lockflag[16] = 0;
        }
    }
    #endif
    
    //工作\调试模式检测
    if(WORK_DEBUG_CONDITION() && (g_lockflag[19] == 0))
    {
        ClrWdt();
        g_timeCount[19]++;
        if(g_timeCount[19] >= KEY_COUNT_DOWN)
        {            
            g_lockflag[19] = 1;
            g_timeCount[19] = 0;
            g_SystemState.workMode = WORK_STATE;
        }
        g_lockflag[20] = 0;
        g_timeCount[20] = 0;
    }
    else if(!WORK_DEBUG_CONDITION() && (g_lockflag[20] == 0))
    {
        ClrWdt();
        g_timeCount[20]++;
        if(g_timeCount[20] >= KEY_COUNT_DOWN)
        {            
            g_lockflag[20] = 1;
            g_timeCount[20] = 0;
            g_SystemState.workMode = DEBUG_STATE;
        }
        g_lockflag[19] = 0;
        g_timeCount[19] = 0;
    }
}


